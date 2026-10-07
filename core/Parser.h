#pragma once
#include <vector>
#include <memory>
#include <string>
#include <unordered_set>
#include "Tokens/Token.h"
#include "ASTNodes/ASTNodes.h"
#include "ParseError.h"
#include <stdexcept>
#include <nlohmann/json.hpp>


// Serialize a module list to a JSON string (used by gen_ast after linking).
std::string modulesToJSON(const std::vector<Module>& modules);

// Attach `// verilens: <word>` pragmas (from Lexer::pragmas()) to modules: a pragma
// inside a module body belongs to that module; one above a module belongs to the
// next module that starts after it.
void attachPragmas(std::vector<Module>& modules,
                   const std::vector<std::pair<int, std::string>>& pragmas);

class Parser {
public:
    // Runs the preprocessor (Preprocessor.h) on the token stream.
    explicit Parser(const std::vector<Token>& tokens);

    // Pass 1 — collect all module names for disambiguation
    std::unordered_set<std::string> collectModuleNames();

    // Pass 2 — full parse, returns every module found in the token stream.
    // A construct that fails to parse is dropped and recorded in Module::notes;
    // the rest of the module survives.
    std::vector<Module> parse();

    // Serialize the parsed AST to a JSON string (single-file, no linker)
    std::string toAST();

private:
    // Token streaming helpers
    const Token& current() const;
    const Token& peek(int offset = 1) const;
    Token        consume();
    bool         check(TokenType type) const;
    bool         checkWord(const std::string& w) const;   // IDENTIFIER with this lexeme
    Token        expect(TokenType type);   // consume or throw on mismatch
    bool         match(TokenType type);    // consume and return true if matches

    // Diagnostics (Module::notes of the module being parsed)
    void note(ValidationError::Severity sev, const std::string& msg, int line, int col);

    // Error recovery
    void recover();                          // after a ParseError: past the next ';', or stop at a block end
    void skipToSemicolon();                  // stop before ';' or a block/module end
    void skipBlock();
    void skipUntil(TokenType end);           // consume tokens up to and including `end`
    void skipUntilWord(const std::string& w);// consume tokens up to and including IDENTIFIER `w`
    void skipStatement();                    // skip one statement (recursive on begin/end)
    void skipParens();                       // current must be LPAREN; skips balanced parens
    void skipDelay();                        // #5, #(1,2,3)
    void skipStrength();                     // (strong1, weak0) when current is LPAREN + strength keyword
    void skipAttributes();                   // (* ... *)
    void skipEndLabel();                     // `: name` after begin/end/endfunction/...
    void skipUnpackedDims();                 // [0:N-1] [4] after a declarator
    std::string scanBalanced(bool stopAtSemicolon); // raw value text up to an unnested , ) ] } (or ;)
    void skipTypedef();                      // current must be TYPEDEF; consume through ';'

    // Top level
    Module parseModule();


    // Module header
    void parseModuleHeader(Module& mod);
    void parseParameterList(Module& mod);
    void parsePortList(Module& mod);       // (a, b, c) or ANSI port list


    // Module body items
    std::vector<Port>      parsePortDeclaration();       // input / output / inout ...
    void                   parseNetDeclaration(Module& mod);      // wire / reg / logic ... (+ implied assign for `wire b = a`)
    void                   parseTypedNetDeclaration(Module& mod); // int / bit / byte / integer / typedef'd / enum / struct
    std::vector<Parameter> parseParameterDeclaration();  // [parameter | localparam] ...
    Instance    parseInstance();              // ModName #() instName (...)
    ExprPtr     tryParseConnectionExpr(bool positional); // non-consuming; see Parser.cpp
    std::vector<GatePrimitive> parseGatePrimitive(); // and/or/not/... [name] (...);
    std::vector<Assign> parseContinuousAssign();  // assign a = x [, b = y, ...];
    AlwaysBlock parseAlwaysBlock();
    void        parseDefparam(Module& mod);         // defparam u.W = 4;
    void        parseGenerateBlock(Module& mod);    // generate ... endgenerate
    void        parseGenerateItem(Module& mod);     // for / if / case / begin at module or generate level
    void        parseGenerateBody(Module& mod);     // begin [: label] items end | single item
    void        parseModuleBodyItem(Module& mod);   // single body item dispatch


    // Always block internals  (needed for FSM detection)
    std::string parseSensitivityList();  // @( posedge clk, negedge rst )

    StmtPtr parseStatement();
    StmtPtr parseSeqBlock();
    StmtPtr parseIfStatement();
    StmtPtr parseCaseStatement();
    std::unique_ptr<CaseItem> parseCaseItem();

    // called after lhs already parsed
    StmtPtr parseBlockingAssign(ExprPtr lhs);
    StmtPtr parseNonBlockingAssign(ExprPtr lhs);


    // Expression parsing
    ExprPtr parseExpression();
    ExprPtr parseBinary(int minPrec);
    ExprPtr parseUnary();
    ExprPtr parseAtom();                // literal, identifier, (expr), {concat}, call, cast, '{pattern}
    ExprPtr parseLValue();              // assignment target: id / id[i] / id[a:b] / {a, b}
    ExprPtr parseSelects(ExprPtr base); // chained [i], [m:l], [b+:w] after a name
    ExprPtr parseRangeOrExpr();         // [lo:hi] (inside lists, case inside) or an expression
    std::string parseScopedName();      // a, a.b, p::a
    PortWidth parseWidth();             // [msb:lsb]
    PortWidth parsePackedDims();        // one or more [msb:lsb]

    static int binaryPrecedence(TokenType t);


    // State
    std::vector<Token>              tokens_;
    size_t                          pos_;
    std::unordered_set<std::string> module_names_;
    Module*                         cur_mod_ = nullptr;   // receives notes
    std::vector<Module>             nested_;              // modules declared inside another module
    struct PendingDefparam { std::string inst, name, value; int line, column; };
    std::vector<PendingDefparam>    defparams_;           // applied at endmodule
};
