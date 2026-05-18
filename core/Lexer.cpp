#include <string>
#include <vector>
#include <cctype>
#include <unordered_map>
#include "Lexer.h"
#include "Tokens/Keywords.h"

Lexer::Lexer(const std::string& input) : input(input), position(0), line(1), col(1) {
    current_char = input.empty() ? '\0' : input[0];
}

char Lexer::peek() const {
    size_t next = position + 1;
    return (next < input.size()) ? input[next] : '\0';
}

char Lexer::advance() {
    char prev = current_char;
    position++;
    current_char = (position < input.size()) ? input[position] : '\0';
    if (prev == '\n') { line++; col = 1; }
    else col++;
    return prev;
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (current_char != '\0') {
        // whitespace
        if (isspace(current_char)) { advance(); continue; }

        // single-line comment
        if (current_char == '/' && peek() == '/') {
            while (current_char != '\n' && current_char != '\0') advance();
            continue;
        }

        // block comment
        if (current_char == '/' && peek() == '*') {
            advance(); advance();
            while (!(current_char == '*' && peek() == '/') && current_char != '\0') advance();
            if (current_char != '\0') { advance(); advance(); }
            continue;
        }

        int tok_line = line, tok_col = col;

        // compiler directive: `define, `include, `ifdef, etc.
        if (current_char == '`') {
            std::string lexeme(1, advance());
            while (isalnum(current_char) || current_char == '_') lexeme += advance();
            tokens.push_back({TokenType::COMPILER_DIRECTIVE, lexeme, tok_line, tok_col});
            continue;
        }

        // system task: $display, $finish, $time, etc.
        if (current_char == '$') {
            std::string lexeme(1, advance());
            while (isalnum(current_char) || current_char == '_') lexeme += advance();
            tokens.push_back({TokenType::SYSTEM_TASK, lexeme, tok_line, tok_col});
            continue;
        }

        // string literal
        if (current_char == '"') {
            std::string lexeme(1, advance());
            while (current_char != '"' && current_char != '\0') {
                if (current_char == '\\') lexeme += advance(); // escape char
                lexeme += advance();
            }
            if (current_char == '"') lexeme += advance();
            tokens.push_back({TokenType::STRING_LITERAL, lexeme, tok_line, tok_col});
            continue;
        }

        // number: integer, sized literal (4'b1010, 8'hFF), or real (3.14)
        if (isdigit(current_char)) {
            std::string lexeme;
            while (isdigit(current_char) || current_char == '_') lexeme += advance();

            char base = tolower(peek());
            if (current_char == '\'' && (base == 'b' || base == 'o' || base == 'd' || base == 'h')) {
                lexeme += advance(); // '
                lexeme += advance(); // base specifier
                while (isalnum(current_char) || current_char == '_') lexeme += advance();
                tokens.push_back({TokenType::INTEGER_LITERAL, lexeme, tok_line, tok_col});
            } else if (current_char == '.' && isdigit(peek())) {
                lexeme += advance(); // .
                while (isdigit(current_char) || current_char == '_') lexeme += advance();
                if (current_char == 'e' || current_char == 'E') {
                    lexeme += advance();
                    if (current_char == '+' || current_char == '-') lexeme += advance();
                    while (isdigit(current_char)) lexeme += advance();
                }
                tokens.push_back({TokenType::REAL_LITERAL, lexeme, tok_line, tok_col});
            } else {
                tokens.push_back({TokenType::INTEGER_LITERAL, lexeme, tok_line, tok_col});
            }
            continue;
        }

        // identifier or keyword
        if (isalpha(current_char) || current_char == '_') {
            std::string lexeme;
            while (isalnum(current_char) || current_char == '_') lexeme += advance();
            tokens.push_back({lookupKeyword(lexeme), lexeme, tok_line, tok_col});
            continue;
        }

        // operators and punctuation — check longest match first
        {
            std::string lexeme(1, advance());
            char nx = current_char;

            // three-char operators
            if (lexeme == "=" && nx == '=' && peek() == '=') { lexeme += advance(); lexeme += advance(); tokens.push_back({TokenType::EQ_EQ_EQ,               lexeme, tok_line, tok_col}); continue; }
            if (lexeme == "!" && nx == '=' && peek() == '=') { lexeme += advance(); lexeme += advance(); tokens.push_back({TokenType::BANG_EQ_EQ,             lexeme, tok_line, tok_col}); continue; }
            if (lexeme == "<" && nx == '<' && peek() == '<') { lexeme += advance(); lexeme += advance(); tokens.push_back({TokenType::LESS_LESS_LESS,          lexeme, tok_line, tok_col}); continue; }
            if (lexeme == ">" && nx == '>' && peek() == '>') { lexeme += advance(); lexeme += advance(); tokens.push_back({TokenType::GREATER_GREATER_GREATER, lexeme, tok_line, tok_col}); continue; }

            // two-char operators
            if (lexeme == "=" && nx == '=') { lexeme += advance(); tokens.push_back({TokenType::EQ_EQ,           lexeme, tok_line, tok_col}); continue; }
            if (lexeme == "!" && nx == '=') { lexeme += advance(); tokens.push_back({TokenType::BANG_EQ,         lexeme, tok_line, tok_col}); continue; }
            if (lexeme == "<" && nx == '=') { lexeme += advance(); tokens.push_back({TokenType::LESS_EQ,         lexeme, tok_line, tok_col}); continue; }
            if (lexeme == ">" && nx == '=') { lexeme += advance(); tokens.push_back({TokenType::GREATER_EQ,      lexeme, tok_line, tok_col}); continue; }
            if (lexeme == "<" && nx == '<') { lexeme += advance(); tokens.push_back({TokenType::LESS_LESS,        lexeme, tok_line, tok_col}); continue; }
            if (lexeme == ">" && nx == '>') { lexeme += advance(); tokens.push_back({TokenType::GREATER_GREATER, lexeme, tok_line, tok_col}); continue; }
            if (lexeme == "*" && nx == '*') { lexeme += advance(); tokens.push_back({TokenType::STAR_STAR,        lexeme, tok_line, tok_col}); continue; }
            if (lexeme == "&" && nx == '&') { lexeme += advance(); tokens.push_back({TokenType::AMP_AMP,         lexeme, tok_line, tok_col}); continue; }
            if (lexeme == "|" && nx == '|') { lexeme += advance(); tokens.push_back({TokenType::PIPE_PIPE,        lexeme, tok_line, tok_col}); continue; }
            if (lexeme == "~" && nx == '&') { lexeme += advance(); tokens.push_back({TokenType::TILDE_AMP,       lexeme, tok_line, tok_col}); continue; }
            if (lexeme == "~" && nx == '|') { lexeme += advance(); tokens.push_back({TokenType::TILDE_PIPE,      lexeme, tok_line, tok_col}); continue; }
            if (lexeme == "~" && nx == '^') { lexeme += advance(); tokens.push_back({TokenType::TILDE_CARET,     lexeme, tok_line, tok_col}); continue; }
            if (lexeme == "^" && nx == '~') { lexeme += advance(); tokens.push_back({TokenType::TILDE_CARET,     lexeme, tok_line, tok_col}); continue; }
            if (lexeme == ":" && nx == ':') { lexeme += advance(); tokens.push_back({TokenType::COLON_COLON,     lexeme, tok_line, tok_col}); continue; }

            // single-char
            static const std::unordered_map<char, TokenType> SINGLE = {
                {'+', TokenType::PLUS},      {'-', TokenType::MINUS},
                {'*', TokenType::STAR},      {'/', TokenType::SLASH},
                {'%', TokenType::PERCENT},   {'^', TokenType::CARET},
                {'!', TokenType::BANG},      {'&', TokenType::AMP},
                {'|', TokenType::PIPE},      {'~', TokenType::TILDE},
                {'<', TokenType::LESS},      {'>', TokenType::GREATER},
                {'=', TokenType::EQ},        {'?', TokenType::QUESTION},
                {':', TokenType::COLON},     {';', TokenType::SEMICOLON},
                {',', TokenType::COMMA},     {'.', TokenType::DOT},
                {'#', TokenType::HASH},      {'@', TokenType::AT},
                {'\'',TokenType::APOSTROPHE},
                {'(', TokenType::LPAREN},    {')', TokenType::RPAREN},
                {'[', TokenType::LBRACKET},  {']', TokenType::RBRACKET},
                {'{', TokenType::LBRACE},    {'}', TokenType::RBRACE},
            };
            auto it = SINGLE.find(lexeme[0]);
            TokenType type = (it != SINGLE.end()) ? it->second : TokenType::UNKNOWN;
            tokens.push_back({type, lexeme, tok_line, tok_col});
        }
    }

    tokens.push_back({TokenType::END_OF_FILE, "", line, col});
    return tokens;
}
