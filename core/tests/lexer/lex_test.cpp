#include <gtest/gtest.h>

#include "Lexer.h"
#include <fstream>
#include <string>
#include <iostream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "Tokens/Token.h"

using std::filesystem::path;
using std::filesystem::recursive_directory_iterator;

class LexerFileTest : public ::testing::TestWithParam<std::filesystem::path> {};

std::vector<std::filesystem::path> collect_files(const std::string& dir) {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file() && (entry.path().extension() == ".v" || entry.path().extension() == ".vh" || entry.path().extension() == ".sv" || entry.path().extension() == ".svh"))
            files.push_back(entry.path());
    }
    return files;
}

std::vector<Token> tokenizef(path& file) {
    bool is_file = std::filesystem::is_regular_file(file);
    if (!is_file) {
        return;
    }
        
    std::ifstream f(file);
    std::string contents((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());

    Lexer lexer(contents);
    return lexer.tokenize();
}


TEST_P(LexerFileTest, MatchesGolden) {
    auto file = GetParam();
    auto tokens = tokenizef(file);

    // load expected from foo.vy.json
    std::ifstream golden_file(file.string() + ".json");
    nlohmann::json expected;
    golden_file >> expected;

    ASSERT_EQ(tokens.size(), expected.size());
    for (size_t i = 0; i < tokens.size(); i++) {
        EXPECT_EQ(tokens[i].type, expected[i]["type"]);
        EXPECT_EQ(tokens[i].lexeme, expected[i]["lexeme"]);
    }
}


INSTANTIATE_TEST_SUITE_P(
    DirectoryFiles,
    LexerFileTest,
    ::testing::ValuesIn(collect_files("path/to/your/dir"))
);