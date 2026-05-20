#include "Lexer.h"
#include "Parser.h"
#include "Linker.h"
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

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: gen_ast <file.v|directory> [file2.v ...]\n";
        return 1;
    }

    std::vector<fs::path> inputs;
    for (int i = 1; i < argc; i++) {
        fs::path p(argv[i]);
        if (fs::is_directory(p)) {
            for (auto& f : collectVerilog(p)) inputs.push_back(f);
        } else {
            inputs.push_back(p);
        }
    }

    if (inputs.empty()) {
        std::cerr << "No .v/.sv files found\n";
        return 1;
    }

    Linker linker;
    for (const auto& path : inputs) {
        std::string src = readFile(path);
        if (src.empty()) {
            std::cerr << "Cannot read: " << path << "\n";
            continue;
        }
        try {
            Lexer lexer(src);
            Parser parser(lexer.tokenize());
            linker.addModules(parser.parse(), path.filename().string());
        } catch (const std::exception& e) {
            std::cerr << "Error parsing " << path.filename() << ": " << e.what() << "\n";
        }
    }

    linker.link();
    std::cout << modulesToJSON(linker.modules()) << std::endl;
    return 0;
}
