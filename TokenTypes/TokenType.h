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

    // --- Assignment ---
    EQ,                 // = 

    // --- Ternary & Misc ---
    QUESTION,           // ?
    COLON,              // :
    COLON_COLON,        // :: (scope resolution, SystemVerilog)

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

struct Token {
    TokenType   type;
    std::string lexeme;  // raw text from source
    int         line;
    int         column;
};
