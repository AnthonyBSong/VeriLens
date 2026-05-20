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

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);

    // Pass 1 — collect all module names for disambiguation
    std::unordered_set<std::string> collectModuleNames();

    // Pass 2 — full parse, returns every module found in the token stream
    std::vector<Module> parse();

    // Serialize the parsed AST to a JSON string (single-file, no linker)
    std::string toAST();

private:
    // Token streaming helpers
    const Token& current() const;
    const Token& peek(int offset = 1) const;
    Token        consume();
    bool         check(TokenType type) const;
    Token        expect(TokenType type);   // consume or throw on mismatch
    bool         match(TokenType type);    // consume and return true if matches


    // Error recovery
    void skipToSemicolon();    // panic: advance until `;`
    void skipToEndmodule();    // panic: advance until `endmodule`
    void skipBlock();          // skip a matched begin/end block

    // Top level
    Module parseModule();


    // Module header
    void parseModuleHeader(Module& mod);   // name + optional #() + port list
    void parseParameterList(Module& mod);  // #( parameter P = val, ... )
    void parsePortList(Module& mod);       // (a, b, c) or ANSI port list


    // Module body items
    Port        parsePortDeclaration();       // input / output / inout ...
    NetDecl     parseNetDeclaration();        // wire / reg / logic ...
    Parameter   parseParameterDeclaration();  // parameter / localparam
    Instance    parseInstance();              // ModName #() instName (...)
    std::vector<Assign> parseContinuousAssign();  // assign a = x [, b = y, ...];
    AlwaysBlock parseAlwaysBlock();           // always @(...) ...


    // Always block internals  (needed for FSM detection)
    std::string parseSensitivityList();  // @( posedge clk, negedge rst )

    StmtPtr parseStatement();            // dispatches to helpers below
    StmtPtr parseSeqBlock();             // begin ... end
    StmtPtr parseIfStatement();          // if (...) ... [else ...]
    StmtPtr parseCaseStatement();        // case / casex / casez
    std::unique_ptr<CaseItem> parseCaseItem();

    // called after lhs already parsed — caller checked = vs <=
    StmtPtr parseBlockingAssign(ExprPtr lhs);     // lhs = rhs;
    StmtPtr parseNonBlockingAssign(ExprPtr lhs);  // lhs <= rhs;


    // Expression parsing
    ExprPtr parseExpression();          // entry: handles ternary then delegates
    ExprPtr parseBinary(int minPrec);   // precedence climbing for binary ops
    ExprPtr parseUnary();               // ~, !, -, reduction operators
    ExprPtr parseAtom();                // literal, identifier, (expr), {concat}
    ExprPtr parseLValue();              // assignment target: id / id[i] / id[a:b]
    PortWidth parseWidth();             // [msb:lsb]

    static int binaryPrecedence(TokenType t);


    // State
    std::vector<Token>              tokens_;
    size_t                          pos_;
    std::unordered_set<std::string> module_names_;
};
