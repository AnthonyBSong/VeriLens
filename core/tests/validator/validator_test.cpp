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
