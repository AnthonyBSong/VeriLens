#include "Parser.h"

// Constructor
Parser::Parser(const std::vector<Token>& tokens)
    : tokens_(tokens), pos_(0) {}

// Pass 1 : scan tokens_ & return IDENTIFIER of {MODULE : IDENTIFIER} pairs
std::unordered_set<std::string> Parser::collectModuleNames() {
    std::unordered_set<std::string> module_names_;
    for (size_t i = 0; i + 1 < tokens_.size(); i++) {
        if (tokens_[i].type == TokenType::MODULE &&
            tokens_[i + 1].type == TokenType::IDENTIFIER) {
                module_names_.insert(tokens_[i + 1].lexeme);
        }
    }
    return module_names_;
}

// Pass 2
std::vector<Module> Parser::parse() {
    // TODO: call collectModuleNames(), then loop calling parseModule() until EOF
    std::unordered_set<std::string> module_names_ = collectModuleNames();

    return {};
}

// Token stream helpers
const Token& Parser::current() const {
    if (pos_ >= tokens_.size()) {
        throw std::out_of_range("current(): past end of token stream");
    }
    return tokens_[pos_];
}

const Token& Parser::peek(int offset) const {
    if (pos_ + offset >= tokens_.size()) {
        throw std::out_of_range("peek(): past end of token stream");
    }
    return tokens_[pos_ + offset];
}

Token Parser::consume() {
    return tokens_[pos_++];
}

bool Parser::check(TokenType type) const {
    return current().type == type;
}

Token Parser::expect(TokenType type) {
    if (!check(type)) {
        throw ParseError(
            "expected " + ::toString(type) +
            ", got '"   + current().lexeme + "'",
            current()
        );
    }
    return consume();
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        return true;
    }
    return false;
}

// Error recovery
void Parser::skipToSemicolon() {
    // TODO: advance until SEMICOLON or END_OF_FILE
}

void Parser::skipToEndmodule() {
    // TODO: advance until ENDMODULE or END_OF_FILE
}

void Parser::skipBlock() {
    // TODO: consume tokens tracking BEGIN/END nesting until the matched END
}

// Top level
Module Parser::parseModule() {
    // TODO: expect MODULE, call parseModuleHeader, loop parseModule body items
    //       until ENDMODULE
    return Module("", 0, 0);
}

// Module header
void Parser::parseModuleHeader(Module& mod) {
    // TODO: consume module name into mod.name
    //       if HASH, call parseParameterList
    //       call parsePortList
    //       expect SEMICOLON
}

void Parser::parseParameterList(Module& mod) {
    // TODO: consume LPAREN, parse comma-separated parameter declarations,
    //       consume RPAREN
}

void Parser::parsePortList(Module& mod) {
    // TODO: handle both port-name-only list (a, b, c) and ANSI inline
    //       port declarations (input wire [7:0] a, ...)
}

// Module body items
Port Parser::parsePortDeclaration() {
    // TODO: consume direction keyword, optional type keyword, optional width,
    //       consume name, expect SEMICOLON
    return Port(PortDirection::INPUT, PortType::UNSPECIFIED, PortWidth(), "", 0, 0);
}

NetDecl Parser::parseNetDeclaration() {
    // TODO: consume net type keyword, optional width, consume name, expect SEMICOLON
    return NetDecl(NetType::WIRE, PortWidth(), "", 0, 0);
}

Parameter Parser::parseParameterDeclaration() {
    // TODO: consume PARAMETER or LOCALPARAM, consume name,
    //       if EQ consume default value expression as raw text
    return Parameter("");
}

Instance Parser::parseInstance() {
    // TODO: consume module name, optional #() parameter override list,
    //       consume instance name, consume port connection list
    return Instance("", "", 0, 0);
}

Assign Parser::parseContinuousAssign() {
    // TODO: consume ASSIGN, call parseLValue, expect EQ,
    //       call parseExpression, expect SEMICOLON
    return Assign(nullptr, nullptr, 0, 0);
}

AlwaysBlock Parser::parseAlwaysBlock() {
    // TODO: consume ALWAYS, call parseSensitivityList, call parseStatement
    return AlwaysBlock("", nullptr, 0, 0);
}

// Always block internals
std::string Parser::parseSensitivityList() {
    // TODO: consume AT, consume LPAREN, capture tokens until matching RPAREN
    return "";
}

StmtPtr Parser::parseStatement() {
    // TODO: dispatch on current token type:
    //   BEGIN         -> parseSeqBlock()
    //   IF            -> parseIfStatement()
    //   CASE/CASEX/Z  -> parseCaseStatement()
    //   IDENTIFIER    -> parseLValue(), then check EQ vs LESS_EQ
    return nullptr;
}

StmtPtr Parser::parseSeqBlock() {
    // TODO: consume BEGIN, loop parseStatement until END
    return nullptr;
}

StmtPtr Parser::parseIfStatement() {
    // TODO: consume IF, expect LPAREN, parseExpression, expect RPAREN
    //       parseStatement for then_branch
    //       if ELSE, parseStatement for else_branch
    return nullptr;
}

StmtPtr Parser::parseCaseStatement() {
    // TODO: consume CASE/CASEX/CASEZ, expect LPAREN, parseExpression, expect RPAREN
    //       loop parseCaseItem until ENDCASE
    return nullptr;
}

std::unique_ptr<CaseItem> Parser::parseCaseItem() {
    // TODO: if DEFAULT consume it (empty patterns)
    //       else parse comma-separated expressions as patterns
    //       expect COLON, parseStatement as body
    return nullptr;
}

StmtPtr Parser::parseBlockingAssign(ExprPtr lhs) {
    // TODO: expect EQ, parseExpression for rhs, expect SEMICOLON
    return nullptr;
}

StmtPtr Parser::parseNonBlockingAssign(ExprPtr lhs) {
    // TODO: expect LESS_EQ, parseExpression for rhs, expect SEMICOLON
    return nullptr;
}

// Expression parsing
ExprPtr Parser::parseExpression() {
    // TODO: recursive descent with operator precedence
    //   lowest:  ternary   ? :
    //   then:    ||
    //   then:    &&
    //   then:    | ^ &  (bitwise)
    //   then:    == != === !==
    //   then:    < <= > >=
    //   then:    << >> <<< >>>
    //   then:    + -
    //   then:    * / %
    //   highest: unary ~ ! - &(reduction) |(reduction)
    //            then parseAtom() for literals, identifiers, parens, concat
    return nullptr;
}

ExprPtr Parser::parseLValue() {
    // TODO: consume IDENTIFIER, then optionally [expr] or [expr:expr]
    return nullptr;
}

PortWidth Parser::parseWidth() {
    // TODO: expect LBRACKET, parse msb expr, expect COLON, parse lsb expr,
    //       expect RBRACKET — return PortWidth(msb, lsb)
    return PortWidth();
}
