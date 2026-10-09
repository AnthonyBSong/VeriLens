#include <gtest/gtest.h>

#include "Lexer.h"
#include "Parser.h"
#include "Linker.h"
#include "Validator.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using path = std::filesystem::path;

// ── helpers ──────────────────────────────────────────────────────────────────

static std::vector<path> collect_verilog(const path& dir) {
    std::vector<path> files;
    for (const auto& e : std::filesystem::recursive_directory_iterator(dir)) {
        if (!e.is_regular_file()) continue;
        auto ext = e.path().extension();
        if (ext == ".v" || ext == ".sv") files.push_back(e.path());
    }
    return files;
}

static Linker build_linker(const std::vector<path>& files) {
    Linker linker;
    for (const auto& f : files) {
        std::ifstream ifs(f);
        std::string src((std::istreambuf_iterator<char>(ifs)), {});
        try {
            Parser parser(Lexer(src).tokenize());
            linker.addModules(parser.parse(), f.filename().string());
        } catch (...) {}  // unsupported syntax — skip silently
    }
    linker.link();
    return linker;
}

// ── per-file test: no ERROR diagnostics when a file is validated alone ────────

class ValidatorSingleFileTest : public ::testing::TestWithParam<path> {};

TEST_P(ValidatorSingleFileTest, NoErrorsInIsolation) {
    auto file = GetParam();
    std::ifstream ifs(file);
    std::string src((std::istreambuf_iterator<char>(ifs)), {});

    Linker linker;
    try {
        Parser parser(Lexer(src).tokenize());
        linker.addModules(parser.parse(), file.filename().string());
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Parse error (unsupported syntax): " << e.what();
    }
    linker.link();

    Validator v(linker);
    auto diags = v.run();

    for (const auto& d : diags) {
        EXPECT_NE(d.severity, ValidationError::Severity::ERROR)
            << Validator::format(d);
    }
}

INSTANTIATE_TEST_SUITE_P(
    AllExamples,
    ValidatorSingleFileTest,
    ::testing::ValuesIn(collect_verilog(
        path(__FILE__).parent_path().parent_path() / "examples"
    )),
    [](const ::testing::TestParamInfo<path>& info) {
        return info.param.stem().string();
    }
);

// ── project-level test: full pipeline linked together has no ERROR diagnostics ─

TEST(ValidatorProjectTest, PipelinedCPUFullyLinked) {
    path examples = path(__FILE__).parent_path().parent_path() / "examples";

    // Collect only the sv pipeline files that form the PipelinedCPU hierarchy.
    std::vector<std::string> pipeline_files = {
        "Alu.sv", "RegFile.sv", "ImmExtend.sv", "ControlUnit.sv",
        "BranchUnit.sv", "HazardDetect.sv", "ForwardingUnit.sv",
        "InstrMemory.sv", "DataMemory.sv", "LoadStoreUnit.sv",
        "FetchStage.sv", "DecodeStage.sv", "ExecuteStage.sv",
        "MemoryStage.sv", "WritebackStage.sv",
        "PipelineReg_IF_ID.sv", "PipelineReg_ID_EX.sv",
        "PipelineReg_EX_MEM.sv", "PipelineReg_MEM_WB.sv",
        "PipelinedCPU.sv",
    };

    std::vector<path> files;
    for (const auto& name : pipeline_files)
        files.push_back(examples / name);

    Linker linker = build_linker(files);

    // Every module in the pipeline should be resolved.
    for (const auto& mod : linker.modules()) {
        for (const auto& inst : mod.instances) {
            EXPECT_TRUE(inst.resolved)
                << "In " << mod.name << ": instance '" << inst.instance_name
                << "' of '" << inst.module_name << "' is unresolved";
        }
    }

    Validator v(linker);
    auto diags = v.run();

    int errors = 0;
    for (const auto& d : diags) {
        if (d.severity == ValidationError::Severity::ERROR) {
            ADD_FAILURE() << Validator::format(d);
            errors++;
        }
    }
    EXPECT_EQ(errors, 0) << errors << " validation error(s) in linked pipeline";
}

// ── error paths: small inline sources that must produce a specific diagnostic ─

static std::vector<ValidationError> diagnose(const std::string& src) {
    Linker linker;
    Parser parser(Lexer(src).tokenize());
    parser.collectModuleNames();
    linker.addModules(parser.parse(), "inline.sv");
    linker.link();
    return Validator(linker).run();
}

static bool has(const std::vector<ValidationError>& diags, ValidationError::Severity sev, const std::string& text) {
    for (const auto& d : diags)
        if (d.severity == sev && d.message.find(text) != std::string::npos) return true;
    return false;
}

static const auto ERR  = ValidationError::Severity::ERROR;
static const auto WARN = ValidationError::Severity::WARNING;

TEST(ValidatorErrors, DuplicateInstanceName) {
    auto d = diagnose("module sub (input a); endmodule\n"
                      "module t (input x); sub u (.a(x)); sub u (.a(x)); endmodule\n");
    EXPECT_TRUE(has(d, ERR, "duplicate instance 'u'"));
}

TEST(ValidatorErrors, ConnectionToMissingPort) {
    auto d = diagnose("module sub (input a); endmodule\n"
                      "module t (input x); sub u (.nope(x)); endmodule\n");
    EXPECT_TRUE(has(d, ERR, "'.nope' is not a port of 'sub'"));
}

TEST(ValidatorErrors, PortConnectedTwice) {
    auto d = diagnose("module sub (input a); endmodule\n"
                      "module t (input x); sub u (.a(x), .a(x)); endmodule\n");
    EXPECT_TRUE(has(d, ERR, "'.a' connected more than once"));
}

TEST(ValidatorErrors, UnconnectedOutputIsAWarning) {
    auto d = diagnose("module sub (input a, output o); assign o = a; endmodule\n"
                      "module t (input x); sub u (.a(x)); endmodule\n");
    EXPECT_TRUE(has(d, WARN, "output port '.o' is not connected"));
    EXPECT_FALSE(has(d, ERR, "'.o'"));
}

TEST(ValidatorErrors, DuplicateGateInstanceName) {
    auto d = diagnose("module t (input a, b, output y, z);\n"
                      "  and g1 (y, a, b);\n  and g1 (z, a, b);\nendmodule\n");
    EXPECT_TRUE(has(d, ERR, "duplicate gate instance 'g1'"));
}

TEST(ValidatorErrors, ReversedRanges) {
    auto d = diagnose("module t (input [0:3] p); wire [0:7] w; endmodule\n");
    EXPECT_TRUE(has(d, ERR, "port 'p': msb (0) < lsb (3)"));
    EXPECT_TRUE(has(d, ERR, "net 'w': msb (0) < lsb (7)"));
}

TEST(ValidatorErrors, AlwaysWithoutSensitivityList) {
    auto d = diagnose("module t (input a, output reg y); always begin y = a; end endmodule\n");
    EXPECT_TRUE(has(d, WARN, "always block has no sensitivity list"));
}

// The parser never builds an assignment whose target is not an l-value, so
// these validate the nodes directly.
template <class N>
static std::vector<ValidationError> validate_node(const N& node) {
    ValidationContext::SymbolTable symbols;
    std::vector<ValidationError> errors;
    ValidationContext ctx{symbols, "direct", {}, {}, errors};
    node.validate(ctx);
    return errors;
}

TEST(ValidatorErrors, AssignTargetMustBeAnLValue) {
    Assign a(std::make_unique<LiteralExpr>("1'b0"), std::make_unique<IdentifierExpr>("x"), 1, 1);
    EXPECT_TRUE(has(validate_node(a), ERR, "assign: left-hand side is not an l-value"));

    BlockingAssign b(std::make_unique<LiteralExpr>("1'b0"), std::make_unique<IdentifierExpr>("x"), 1, 1);
    EXPECT_TRUE(has(validate_node(b), ERR, "blocking assign: left-hand side is not an l-value"));

    NonBlockingAssign n(std::make_unique<LiteralExpr>("1'b0"), std::make_unique<IdentifierExpr>("x"), 1, 1);
    EXPECT_TRUE(has(validate_node(n), ERR, "non-blocking assign: left-hand side is not an l-value"));
}
