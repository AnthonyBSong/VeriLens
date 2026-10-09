#include <gtest/gtest.h>

#include "Lexer.h"
#include <fstream>
#include <string>
#include <iostream>
#include <filesystem>
#include <set>
#include <nlohmann/json.hpp>
#include "Tokens/Token.h"

using std::filesystem::path;
using std::filesystem::recursive_directory_iterator;

// Every TokenType has a distinct name in toString() (Token.h); the golden
// files are keyed on these names.
TEST(TokenTypes, EveryTypeHasADistinctName) {
    std::set<std::string> names;
    for (int i = 0; i <= static_cast<int>(TokenType::UNKNOWN); i++) {
        std::string n = toString(static_cast<TokenType>(i));
        EXPECT_TRUE(names.insert(n).second) << "duplicate name " << n << " for token " << i;
    }
}

std::vector<std::filesystem::path> collect_files(const std::string& dir) {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file() && (
            entry.path().extension() == ".v"   ||
            entry.path().extension() == ".vh"  ||
            entry.path().extension() == ".sv"  ||
            entry.path().extension() == ".svh"))
            files.push_back(entry.path());
    }
    return files;
}

std::vector<Token> tokenizef(const path& file) {
    std::ifstream f(file);
    std::string contents((std::istreambuf_iterator<char>(f)),
                          std::istreambuf_iterator<char>());
    Lexer lexer(contents);
    return lexer.tokenize();
}

class LexerFileTest : public ::testing::TestWithParam<std::filesystem::path> {};

TEST_P(LexerFileTest, MatchesGolden) {
    auto file = GetParam();
    auto tokens = tokenizef(file);

    // Goldens live in lexer/ regardless of where the source file is.
    path lexer_dir  = path(__FILE__).parent_path();
    path golden_path = lexer_dir / (file.filename().string() + ".json");
    std::ifstream golden_file(golden_path);
    ASSERT_TRUE(golden_file.is_open()) << "Missing golden file: " << golden_path;

    nlohmann::json expected;
    golden_file >> expected;

    ASSERT_EQ(tokens.size(), expected.size());
    for (size_t i = 0; i < tokens.size(); i++) {
        EXPECT_EQ(toString(tokens[i].type), expected[i]["type"].get<std::string>());
        EXPECT_EQ(tokens[i].lexeme, expected[i]["lexeme"].get<std::string>());
    }
}

INSTANTIATE_TEST_SUITE_P(
    AllExamples,
    LexerFileTest,
    ::testing::ValuesIn(collect_files(
        (std::filesystem::path(__FILE__).parent_path().parent_path() / "examples").string()
    )),
    [](const ::testing::TestParamInfo<std::filesystem::path>& info) {
        return info.param.stem().string();
    }
);
