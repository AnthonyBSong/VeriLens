#include <string>
#include <iostream>
#include <vector>
#include "TokenTypes/TokenType.h"
#include "TokenTypes/Keywords.h"

class Lexer {
    private:
        std::string input;
        size_t position;
        char current_char;

    public:
        Lexer(const std::string& input) : input(input) {
            position = 0;
            current_char = input[position];
        }

        char advance() {
            position++;
            char previous_char = current_char;
            current_char = input[position];
            return previous_char;
        }

        std::vector<Token> run() {
            
        }


};