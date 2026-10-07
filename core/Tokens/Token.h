#pragma once
#include <string>

enum class TokenType {
    // --- Literals ---
    INTEGER_LITERAL,    // 4'b1010
    REAL_LITERAL,       // 3.14, 1.2e5
    STRING_LITERAL,     // "hello world"
    TIME_LITERAL,       // 10ns, 5ps (used in timescale/delays)

    // --- Identifiers ---
    IDENTIFIER,         // user-defined names: clk, reset, my_wire
    SYSTEM_TASK,        // $display, $finish, $time, $random
    COMPILER_DIRECTIVE, // `define, `include, `ifdef, `ifndef, `endif, `timescale

    // --- Module Structure ---
    MODULE,
    ENDMODULE,

    // --- Port Directions ---
    INPUT,
    OUTPUT,
    INOUT,

    // --- Net Types ---
    WIRE,
    TRI,
    TRI0,
    TRI1,
    WAND,
    WOR,
    SUPPLY0,
    SUPPLY1,

    // --- Variable Types ---
    REG,
    LOGIC,
    INTEGER,
    REAL,
    TIME,
    REALTIME,

    // --- Parameters ---
    PARAMETER,
    LOCALPARAM,
    DEFPARAM,

    // --- Procedural Blocks ---
    ALWAYS,
    ALWAYS_FF,
    ALWAYS_COMB,
    ALWAYS_LATCH,
    INITIAL,
    BEGIN,
    END,

    // --- Control Flow ---
    IF,
    ELSE,
    CASE,
    CASEX,
    CASEZ,
    ENDCASE,
    DEFAULT,
    FOR,
    WHILE,
    REPEAT,
    FOREVER,
    DISABLE,
    FORK,
    JOIN,

    // --- Edge Specifiers ---
    POSEDGE,
    NEGEDGE,

    // --- Continuous Assignment ---
    ASSIGN,
    DEASSIGN,
    FORCE,
    RELEASE,

    // --- Gate Primitives ---
    AND,
    OR,
    NOT,
    NAND,
    NOR,
    XOR,
    XNOR,
    BUF,
    BUFIF0,
    BUFIF1,
    NOTIF0,
    NOTIF1,

    // --- Functions & Tasks ---
    FUNCTION,
    ENDFUNCTION,
    TASK,
    ENDTASK,
    AUTOMATIC,
    RETURN,

    // --- Generate ---
    GENERATE,
    ENDGENERATE,
    GENVAR,

    // --- Strength Specifiers ---
    STRONG0,
    STRONG1,
    WEAK0,
    WEAK1,
    HIGHZ0,
    HIGHZ1,
    PULL0,
    PULL1,

    // --- Signed/Unsigned ---
    SIGNED,
    UNSIGNED,

    // --- Specify Block ---
    SPECIFY,
    ENDSPECIFY,
    SPECPARAM,

    // --- SystemVerilog Type Constructs ---
    TYPEDEF,
    ENUM,
    STRUCT,
    UNION,
    PACKED,

    // --- Arithmetic Operators ---
    PLUS,               // +
    MINUS,              // -
    STAR,               // *
    SLASH,              // /
    PERCENT,            // %
    STAR_STAR,          // **

    // --- Logical Operators ---
    BANG,               // !
    AMP_AMP,            // &&
    PIPE_PIPE,          // ||

    // --- Relational Operators ---
    EQ_EQ,              // ==
    BANG_EQ,            // !=
    EQ_EQ_EQ,           // ===
    BANG_EQ_EQ,         // !==
    LESS,               // <
    LESS_EQ,            // <=
    GREATER,            // >
    GREATER_EQ,         // >=

    // --- Bitwise Operators ---
    TILDE,              // ~
    AMP,                // &
    PIPE,               // |
    CARET,              // ^
    TILDE_AMP,          // ~&
    TILDE_PIPE,         // ~|
    TILDE_CARET,        // ~^ or ^~

    // --- Shift Operators ---
    LESS_LESS,          // <<
    GREATER_GREATER,    // >>
    LESS_LESS_LESS,     // <<<
    GREATER_GREATER_GREATER, // >>>
    PLUS_COLON,         // +: (indexed part-select)
    MINUS_COLON,        // -:
    EQ_EQ_QUESTION,     // ==? (wildcard equality)
    BANG_EQ_QUESTION,   // !=?

    // --- Assignment ---
    EQ,                 // =

    // --- Ternary & Misc ---
    QUESTION,           // ?
    COLON,              // :
    COLON_COLON,        // :: (scope resolution for SystemVerilog)

    // --- Punctuation ---
    SEMICOLON,          // ;
    COMMA,              // ,
    DOT,                // .  (named port connection)
    HASH,               // #  (delay or parameter override)
    AT,                 // @  (event control)
    APOSTROPHE,         // '  (used in sized literals: 4'b, 8'h)

    // --- Delimiters ---
    LPAREN,             // (
    RPAREN,             // )
    LBRACKET,           // [
    RBRACKET,           // ]
    LBRACE,             // {
    RBRACE,             // }

    // --- Special ---
    END_OF_FILE,
    UNKNOWN
};

inline std::string toString(TokenType t) {
    switch (t) {
        case TokenType::INTEGER_LITERAL:        return "INTEGER_LITERAL";
        case TokenType::REAL_LITERAL:           return "REAL_LITERAL";
        case TokenType::STRING_LITERAL:         return "STRING_LITERAL";
        case TokenType::TIME_LITERAL:           return "TIME_LITERAL";
        case TokenType::IDENTIFIER:             return "IDENTIFIER";
        case TokenType::SYSTEM_TASK:            return "SYSTEM_TASK";
        case TokenType::COMPILER_DIRECTIVE:     return "COMPILER_DIRECTIVE";
        case TokenType::MODULE:                 return "MODULE";
        case TokenType::ENDMODULE:              return "ENDMODULE";
        case TokenType::INPUT:                  return "INPUT";
        case TokenType::OUTPUT:                 return "OUTPUT";
        case TokenType::INOUT:                  return "INOUT";
        case TokenType::WIRE:                   return "WIRE";
        case TokenType::TRI:                    return "TRI";
        case TokenType::TRI0:                   return "TRI0";
        case TokenType::TRI1:                   return "TRI1";
        case TokenType::WAND:                   return "WAND";
        case TokenType::WOR:                    return "WOR";
        case TokenType::SUPPLY0:                return "SUPPLY0";
        case TokenType::SUPPLY1:                return "SUPPLY1";
        case TokenType::REG:                    return "REG";
        case TokenType::LOGIC:                  return "LOGIC";
        case TokenType::INTEGER:                return "INTEGER";
        case TokenType::REAL:                   return "REAL";
        case TokenType::TIME:                   return "TIME";
        case TokenType::REALTIME:               return "REALTIME";
        case TokenType::PARAMETER:              return "PARAMETER";
        case TokenType::LOCALPARAM:             return "LOCALPARAM";
        case TokenType::DEFPARAM:               return "DEFPARAM";
        case TokenType::ALWAYS:                 return "ALWAYS";
        case TokenType::ALWAYS_FF:              return "ALWAYS_FF";
        case TokenType::ALWAYS_COMB:            return "ALWAYS_COMB";
        case TokenType::ALWAYS_LATCH:           return "ALWAYS_LATCH";
        case TokenType::INITIAL:                return "INITIAL";
        case TokenType::BEGIN:                  return "BEGIN";
        case TokenType::END:                    return "END";
        case TokenType::IF:                     return "IF";
        case TokenType::ELSE:                   return "ELSE";
        case TokenType::CASE:                   return "CASE";
        case TokenType::CASEX:                  return "CASEX";
        case TokenType::CASEZ:                  return "CASEZ";
        case TokenType::ENDCASE:                return "ENDCASE";
        case TokenType::DEFAULT:                return "DEFAULT";
        case TokenType::FOR:                    return "FOR";
        case TokenType::WHILE:                  return "WHILE";
        case TokenType::REPEAT:                 return "REPEAT";
        case TokenType::FOREVER:                return "FOREVER";
        case TokenType::DISABLE:                return "DISABLE";
        case TokenType::FORK:                   return "FORK";
        case TokenType::JOIN:                   return "JOIN";
        case TokenType::POSEDGE:                return "POSEDGE";
        case TokenType::NEGEDGE:                return "NEGEDGE";
        case TokenType::ASSIGN:                 return "ASSIGN";
        case TokenType::DEASSIGN:               return "DEASSIGN";
        case TokenType::FORCE:                  return "FORCE";
        case TokenType::RELEASE:                return "RELEASE";
        case TokenType::AND:                    return "AND";
        case TokenType::OR:                     return "OR";
        case TokenType::NOT:                    return "NOT";
        case TokenType::NAND:                   return "NAND";
        case TokenType::NOR:                    return "NOR";
        case TokenType::XOR:                    return "XOR";
        case TokenType::XNOR:                   return "XNOR";
        case TokenType::BUF:                    return "BUF";
        case TokenType::BUFIF0:                 return "BUFIF0";
        case TokenType::BUFIF1:                 return "BUFIF1";
        case TokenType::NOTIF0:                 return "NOTIF0";
        case TokenType::NOTIF1:                 return "NOTIF1";
        case TokenType::FUNCTION:               return "FUNCTION";
        case TokenType::ENDFUNCTION:            return "ENDFUNCTION";
        case TokenType::TASK:                   return "TASK";
        case TokenType::ENDTASK:                return "ENDTASK";
        case TokenType::AUTOMATIC:              return "AUTOMATIC";
        case TokenType::RETURN:                 return "RETURN";
        case TokenType::GENERATE:               return "GENERATE";
        case TokenType::ENDGENERATE:            return "ENDGENERATE";
        case TokenType::GENVAR:                 return "GENVAR";
        case TokenType::STRONG0:                return "STRONG0";
        case TokenType::STRONG1:                return "STRONG1";
        case TokenType::WEAK0:                  return "WEAK0";
        case TokenType::WEAK1:                  return "WEAK1";
        case TokenType::HIGHZ0:                 return "HIGHZ0";
        case TokenType::HIGHZ1:                 return "HIGHZ1";
        case TokenType::PULL0:                  return "PULL0";
        case TokenType::PULL1:                  return "PULL1";
        case TokenType::SIGNED:                 return "SIGNED";
        case TokenType::UNSIGNED:               return "UNSIGNED";
        case TokenType::SPECIFY:                return "SPECIFY";
        case TokenType::ENDSPECIFY:             return "ENDSPECIFY";
        case TokenType::SPECPARAM:              return "SPECPARAM";
        case TokenType::TYPEDEF:                return "TYPEDEF";
        case TokenType::ENUM:                   return "ENUM";
        case TokenType::STRUCT:                 return "STRUCT";
        case TokenType::UNION:                  return "UNION";
        case TokenType::PACKED:                 return "PACKED";
        case TokenType::PLUS:                   return "PLUS";
        case TokenType::MINUS:                  return "MINUS";
        case TokenType::STAR:                   return "STAR";
        case TokenType::SLASH:                  return "SLASH";
        case TokenType::PERCENT:                return "PERCENT";
        case TokenType::STAR_STAR:              return "STAR_STAR";
        case TokenType::BANG:                   return "BANG";
        case TokenType::AMP_AMP:                return "AMP_AMP";
        case TokenType::PIPE_PIPE:              return "PIPE_PIPE";
        case TokenType::EQ_EQ:                  return "EQ_EQ";
        case TokenType::BANG_EQ:                return "BANG_EQ";
        case TokenType::EQ_EQ_EQ:               return "EQ_EQ_EQ";
        case TokenType::BANG_EQ_EQ:             return "BANG_EQ_EQ";
        case TokenType::LESS:                   return "LESS";
        case TokenType::LESS_EQ:                return "LESS_EQ";
        case TokenType::GREATER:                return "GREATER";
        case TokenType::GREATER_EQ:             return "GREATER_EQ";
        case TokenType::TILDE:                  return "TILDE";
        case TokenType::AMP:                    return "AMP";
        case TokenType::PIPE:                   return "PIPE";
        case TokenType::CARET:                  return "CARET";
        case TokenType::TILDE_AMP:              return "TILDE_AMP";
        case TokenType::TILDE_PIPE:             return "TILDE_PIPE";
        case TokenType::TILDE_CARET:            return "TILDE_CARET";
        case TokenType::LESS_LESS:              return "LESS_LESS";
        case TokenType::GREATER_GREATER:        return "GREATER_GREATER";
        case TokenType::LESS_LESS_LESS:         return "LESS_LESS_LESS";
        case TokenType::GREATER_GREATER_GREATER: return "GREATER_GREATER_GREATER";
        case TokenType::PLUS_COLON:             return "PLUS_COLON";
        case TokenType::MINUS_COLON:            return "MINUS_COLON";
        case TokenType::EQ_EQ_QUESTION:         return "EQ_EQ_QUESTION";
        case TokenType::BANG_EQ_QUESTION:       return "BANG_EQ_QUESTION";
        case TokenType::EQ:                     return "EQ";
        case TokenType::QUESTION:               return "QUESTION";
        case TokenType::COLON:                  return "COLON";
        case TokenType::COLON_COLON:            return "COLON_COLON";
        case TokenType::SEMICOLON:              return "SEMICOLON";
        case TokenType::COMMA:                  return "COMMA";
        case TokenType::DOT:                    return "DOT";
        case TokenType::HASH:                   return "HASH";
        case TokenType::AT:                     return "AT";
        case TokenType::APOSTROPHE:             return "APOSTROPHE";
        case TokenType::LPAREN:                 return "LPAREN";
        case TokenType::RPAREN:                 return "RPAREN";
        case TokenType::LBRACKET:               return "LBRACKET";
        case TokenType::RBRACKET:               return "RBRACKET";
        case TokenType::LBRACE:                 return "LBRACE";
        case TokenType::RBRACE:                 return "RBRACE";
        case TokenType::END_OF_FILE:            return "END_OF_FILE";
        case TokenType::UNKNOWN:                return "UNKNOWN";
        default:                                return "UNKNOWN";
    }
}

struct Token {
    TokenType   type;
    std::string lexeme;  // raw text from source
    int         line;
    int         column;

    std::string toString() const {
        return ::toString(type) + "('" + lexeme + "' " +
               std::to_string(line) + ":" + std::to_string(column) + ")";
    }
};
