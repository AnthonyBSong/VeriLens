#include <gtest/gtest.h>

#include "Lexer.h"
#include "Parser.h"
#include "Linker.h"
#include <fstream>
#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>

using std::filesystem::path;
using std::filesystem::recursive_directory_iterator;

std::vector<path> collect_files(const std::string& dir) {
    std::vector<path> files;
    for (const auto& entry : recursive_directory_iterator(dir)) {
        if (entry.is_regular_file() && (
            entry.path().extension() == ".v"  ||
            entry.path().extension() == ".sv"))
            files.push_back(entry.path());
    }
    return files;
}

std::vector<path> all_test_files() {
    path parser_dir = path(__FILE__).parent_path();
    path lexer_dir  = parser_dir.parent_path() / "lexer";

    auto files = collect_files(parser_dir.string());
    for (auto& f : collect_files(lexer_dir.string()))
        files.push_back(f);
    return files;
}

std::string parse_file(const path& file) {
    std::ifstream f(file);
    std::string src((std::istreambuf_iterator<char>(f)), {});
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    Linker linker;
    linker.addModules(parser.parse(), file.filename().string());
    linker.link();
    return modulesToJSON(linker.modules());
}

class ParserFileTest : public ::testing::TestWithParam<path> {};

TEST_P(ParserFileTest, MatchesGolden) {
    auto file = GetParam();
    std::string ast_str = parse_file(file);

    // goldens always live in the parser test directory
    path parser_dir = path(__FILE__).parent_path();
    path golden_path = parser_dir / (file.filename().string() + ".ast.json");
    std::ifstream golden_f(golden_path);
    ASSERT_TRUE(golden_f.is_open()) << "Missing golden: " << golden_path;

    nlohmann::json actual   = nlohmann::json::parse(ast_str);
    nlohmann::json expected;
    golden_f >> expected;

    EXPECT_EQ(actual, expected)
        << "AST mismatch for " << file.filename() << "\n"
        << "Got:\n"      << actual.dump(2)   << "\n"
        << "Expected:\n" << expected.dump(2);
}

INSTANTIATE_TEST_SUITE_P(
    AllFiles,
    ParserFileTest,
    ::testing::ValuesIn(all_test_files()),
    [](const ::testing::TestParamInfo<path>& info) {
        return info.param.stem().string();
    }
);
