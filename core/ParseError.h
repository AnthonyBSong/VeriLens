#pragma once
#include <stdexcept>
#include <string>
#include "Tokens/Token.h"

class ParseError : public std::runtime_error {
public:
    int line;
    int column;

    ParseError(const std::string& message, int line, int column)
        : std::runtime_error(
              "Parse error at " + std::to_string(line) + ":" +
              std::to_string(column) + " — " + message),
          line(line), column(column) {}

    // Convenience: construct directly from a token
    ParseError(const std::string& message, const Token& token)
        : ParseError(message, token.line, token.column) {}
};
