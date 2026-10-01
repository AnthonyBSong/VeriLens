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

        // `// verilens: <word> [<word>...]` comments, as (line, word) pairs, in source order.
        // Words are attached to modules by attachPragmas() (Parser.h).
        const std::vector<std::pair<int, std::string>>& pragmas() const { return pragmas_; }
    private:
        std::vector<std::pair<int, std::string>> pragmas_;
};
