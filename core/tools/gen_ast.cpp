#include "Lexer.h"
#include "Parser.h"
#include "Linker.h"
#include "Validator.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::string readFile(const fs::path& p) {
    std::ifstream f(p);
    return std::string(std::istreambuf_iterator<char>(f), {});
}

static std::vector<fs::path> collectVerilog(const fs::path& root) {
    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension();
        if (ext == ".v" || ext == ".sv")
            files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    return files;
}

static void usage() {
    std::cerr << "Usage: gen_ast [--no-validate] <file.v|directory> [more inputs ...]\n";
}

int main(int argc, char* argv[]) {
    bool run_validator = true;
    std::vector<fs::path> inputs;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--no-validate") { run_validator = false; continue; }
        if (a == "-h" || a == "--help") { usage(); return 0; }
        fs::path p(a);
        if (fs::is_directory(p)) {
            for (auto& f : collectVerilog(p)) inputs.push_back(f);
        } else {
            inputs.push_back(p);
        }
    }

    if (inputs.empty()) { usage(); return 1; }

    Linker linker;
    bool parse_failed = false;
    for (const auto& path : inputs) {
        std::string src = readFile(path);
        if (src.empty()) {
            std::cerr << "Cannot read: " << path << "\n";
            continue;
        }
        try {
            Lexer lexer(src);
            Parser parser(lexer.tokenize());
            auto mods = parser.parse();
            attachPragmas(mods, lexer.pragmas());
            linker.addModules(std::move(mods), path.filename().string());
        } catch (const std::exception& e) {
            std::cerr << "Error parsing " << path.filename() << ": " << e.what() << "\n";
            parse_failed = true;
        }
    }

    linker.link();
    std::cout << modulesToJSON(linker.modules()) << std::endl;

    // Diagnostics go to stderr so JSON output on stdout stays consumable
    // by downstream renderers. Errors flip the exit code.
    int exit_code = parse_failed ? 3 : 0;   // 3: something was dropped from the model
    for (const auto& mod : linker.modules())
        for (const auto& n : mod.notes) {
            std::cerr << Validator::format(n) << "\n";
            if (n.severity == ValidationError::Severity::ERROR) exit_code = 3;
        }
    if (run_validator) {
        Validator validator(linker);
        for (const auto& diag : validator.run()) {
            std::cerr << Validator::format(diag) << "\n";
            if (diag.severity == ValidationError::Severity::ERROR) exit_code = 2;
        }
    }
    return exit_code;
}
