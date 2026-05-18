#pragma once
#include <string>
#include <vector>
#include "Tokens/Token.h"

class Lexer {
    private:
        std::string input;
        size_t position;
        char current_char;
        int line;
        int col;

    public:
        Lexer(const std::string& input);

        char peek() const;
        char advance();
        std::vector<Token> tokenize();
};
