#pragma once
#include <string>
#include <unordered_map>
#include "Token.h"

// Maps every Verilog keyword string to its TokenType.

inline TokenType lookupKeyword(const std::string& word) {
    // GCOVR_EXCL_START: a data table, not code. gcc's gcov gives every line of a
    // multi-line initializer its own zero counter, which read as ~90 missed lines.
    static const std::unordered_map<std::string, TokenType> KEYWORDS = {
        // Module structure
        {"module",      TokenType::MODULE},
        {"endmodule",   TokenType::ENDMODULE},

        // Port directions
        {"input",       TokenType::INPUT},
        {"output",      TokenType::OUTPUT},
        {"inout",       TokenType::INOUT},

        // Net types
        {"wire",        TokenType::WIRE},
        {"tri",         TokenType::TRI},
        {"tri0",        TokenType::TRI0},
        {"tri1",        TokenType::TRI1},
        {"wand",        TokenType::WAND},
        {"wor",         TokenType::WOR},
        {"supply0",     TokenType::SUPPLY0},
        {"supply1",     TokenType::SUPPLY1},

        // Variable types
        {"reg",         TokenType::REG},
        {"logic",       TokenType::LOGIC},
        {"integer",     TokenType::INTEGER},
        {"real",        TokenType::REAL},
        {"time",        TokenType::TIME},
        {"realtime",    TokenType::REALTIME},

        // Parameters
        {"parameter",   TokenType::PARAMETER},
        {"localparam",  TokenType::LOCALPARAM},
        {"defparam",    TokenType::DEFPARAM},

        // Procedural blocks
        {"always",      TokenType::ALWAYS},
        {"always_ff",   TokenType::ALWAYS_FF},
        {"always_comb", TokenType::ALWAYS_COMB},
        {"always_latch",TokenType::ALWAYS_LATCH},
        {"initial",     TokenType::INITIAL},
        {"begin",       TokenType::BEGIN},
        {"end",         TokenType::END},

        // Control flow
        {"if",          TokenType::IF},
        {"else",        TokenType::ELSE},
        {"case",        TokenType::CASE},
        {"casex",       TokenType::CASEX},
        {"casez",       TokenType::CASEZ},
        {"endcase",     TokenType::ENDCASE},
        {"default",     TokenType::DEFAULT},
        {"for",         TokenType::FOR},
        {"while",       TokenType::WHILE},
        {"repeat",      TokenType::REPEAT},
        {"forever",     TokenType::FOREVER},
        {"disable",     TokenType::DISABLE},
        {"fork",        TokenType::FORK},
        {"join",        TokenType::JOIN},

        // Edge specifiers
        {"posedge",     TokenType::POSEDGE},
        {"negedge",     TokenType::NEGEDGE},

        // Continuous assignment
        {"assign",      TokenType::ASSIGN},
        {"deassign",    TokenType::DEASSIGN},
        {"force",       TokenType::FORCE},
        {"release",     TokenType::RELEASE},

        // Gate primitives
        {"and",         TokenType::AND},
        {"or",          TokenType::OR},
        {"not",         TokenType::NOT},
        {"nand",        TokenType::NAND},
        {"nor",         TokenType::NOR},
        {"xor",         TokenType::XOR},
        {"xnor",        TokenType::XNOR},
        {"buf",         TokenType::BUF},
        {"bufif0",      TokenType::BUFIF0},
        {"bufif1",      TokenType::BUFIF1},
        {"notif0",      TokenType::NOTIF0},
        {"notif1",      TokenType::NOTIF1},

        // Functions & tasks
        {"function",    TokenType::FUNCTION},
        {"endfunction", TokenType::ENDFUNCTION},
        {"task",        TokenType::TASK},
        {"endtask",     TokenType::ENDTASK},
        {"automatic",   TokenType::AUTOMATIC},
        {"return",      TokenType::RETURN},

        // Generate
        {"generate",    TokenType::GENERATE},
        {"endgenerate", TokenType::ENDGENERATE},
        {"genvar",      TokenType::GENVAR},

        // Strength specifiers
        {"strong0",     TokenType::STRONG0},
        {"strong1",     TokenType::STRONG1},
        {"weak0",       TokenType::WEAK0},
        {"weak1",       TokenType::WEAK1},
        {"highz0",      TokenType::HIGHZ0},
        {"highz1",      TokenType::HIGHZ1},
        {"pull0",       TokenType::PULL0},
        {"pull1",       TokenType::PULL1},

        // Signed/unsigned
        {"signed",      TokenType::SIGNED},
        {"unsigned",    TokenType::UNSIGNED},

        // Specify block
        {"specify",     TokenType::SPECIFY},
        {"endspecify",  TokenType::ENDSPECIFY},
        {"specparam",   TokenType::SPECPARAM},

        // SystemVerilog type constructs
        {"typedef",     TokenType::TYPEDEF},
        {"enum",        TokenType::ENUM},
        {"struct",      TokenType::STRUCT},
        {"union",       TokenType::UNION},
        {"packed",      TokenType::PACKED},
    };
    // GCOVR_EXCL_STOP

    auto it = KEYWORDS.find(word);
    return (it != KEYWORDS.end()) ? it->second : TokenType::IDENTIFIER;
}
