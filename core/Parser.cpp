#include "Parser.h"
#include <algorithm>

using json = nlohmann::json;

Parser::Parser(const std::vector<Token>& tokens)
    : tokens_(tokens), pos_(0) {}

// pass 1: collect module names for instance disambiguation
std::unordered_set<std::string> Parser::collectModuleNames() {
    module_names_.clear();
    for (size_t i = 0; i + 1 < tokens_.size(); i++) {
        if (tokens_[i].type == TokenType::MODULE &&
            tokens_[i + 1].type == TokenType::IDENTIFIER) {
            module_names_.insert(tokens_[i + 1].lexeme);
        }
    }
    return module_names_;
}

// pass 2: full parse
std::vector<Module> Parser::parse() {
    collectModuleNames();
    std::vector<Module> modules;
    while (!check(TokenType::END_OF_FILE)) {
        if (check(TokenType::MODULE))
            modules.push_back(parseModule());
        else
            consume(); // skip compiler directives, stray tokens, etc.
    }
    return modules;
}

// token helpers

const Token& Parser::current() const {
    if (pos_ >= tokens_.size())
        return tokens_.back(); // END_OF_FILE equivalent
    return tokens_[pos_];
}

const Token& Parser::peek(int offset) const {
    size_t idx = pos_ + offset;
    if (idx >= tokens_.size())
        return tokens_.back();
    return tokens_[idx];
}

Token Parser::consume() {
    Token t = current();
    if (pos_ < tokens_.size()) pos_++;
    return t;
}

bool Parser::check(TokenType type) const {
    return current().type == type;
}

Token Parser::expect(TokenType type) {
    if (!check(type))
        throw ParseError(
            "expected " + ::toString(type) + ", got '" + current().lexeme + "'",
            current());
    return consume();
}

bool Parser::match(TokenType type) {
    if (check(type)) { consume(); return true; }
    return false;
}

// error recovery

void Parser::skipToSemicolon() {
    while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE))
        consume();
}

void Parser::skipToEndmodule() {
    while (!check(TokenType::ENDMODULE) && !check(TokenType::END_OF_FILE))
        consume();
}

void Parser::skipBlock() {
    // tracks BEGIN/END nesting — call when current token is BEGIN
    int depth = 0;
    do {
        if (check(TokenType::BEGIN))    depth++;
        else if (check(TokenType::END)) depth--;
        consume();
    } while (depth > 0 && !check(TokenType::END_OF_FILE));
}

// top level

Module Parser::parseModule() {
    int line = current().line, col = current().column;
    expect(TokenType::MODULE);
    Module mod("", line, col);
    parseModuleHeader(mod);

    while (!check(TokenType::ENDMODULE) && !check(TokenType::END_OF_FILE)) {
        if (check(TokenType::INPUT) || check(TokenType::OUTPUT) || check(TokenType::INOUT)) {
            Port decl = parsePortDeclaration();
            // Non-ANSI style: port list creates stubs, body declarations refine them.
            // Update the stub in-place rather than appending a duplicate.
            auto it = std::find_if(mod.ports.begin(), mod.ports.end(),
                [&](const Port& p){ return p.name == decl.name; });
            if (it != mod.ports.end()) *it = decl;
            else                       mod.ports.push_back(std::move(decl));
        } else if (check(TokenType::WIRE)    || check(TokenType::REG)    ||
                   check(TokenType::LOGIC)   || check(TokenType::TRI)    ||
                   check(TokenType::WAND)    || check(TokenType::WOR)    ||
                   check(TokenType::SUPPLY0) || check(TokenType::SUPPLY1)) {
            for (auto& d : parseNetDeclaration())
                mod.net_decls.push_back(std::move(d));
        } else if (check(TokenType::PARAMETER) || check(TokenType::LOCALPARAM)) {
            mod.parameters.push_back(parseParameterDeclaration());
        } else if (check(TokenType::ASSIGN)) {
            for (auto& a : parseContinuousAssign())
                mod.assigns.push_back(std::move(a));
        } else if (check(TokenType::ALWAYS)) {
            mod.always_blocks.push_back(parseAlwaysBlock());
        } else if (check(TokenType::INITIAL)) {
            consume();
            if (check(TokenType::BEGIN)) skipBlock();
            else { skipToSemicolon(); match(TokenType::SEMICOLON); }
        } else if (check(TokenType::IDENTIFIER)) {
            // used to disambiguate instance vs unknown declaration
            bool is_instance = module_names_.count(current().lexeme)
                || peek(1).type == TokenType::HASH
                || (peek(1).type == TokenType::IDENTIFIER
                    && peek(2).type == TokenType::LPAREN);
            if (is_instance)
                mod.instances.push_back(parseInstance());
            else { skipToSemicolon(); match(TokenType::SEMICOLON); }
        } else {
            consume();
        }
    }

    match(TokenType::ENDMODULE);
    return mod;
}

// module header

void Parser::parseModuleHeader(Module& mod) {
    mod.name = expect(TokenType::IDENTIFIER).lexeme;
    if (match(TokenType::HASH))
        parseParameterList(mod);
    parsePortList(mod);
    expect(TokenType::SEMICOLON);
}

void Parser::parseParameterList(Module& mod) {
    expect(TokenType::LPAREN);
    if (!check(TokenType::RPAREN)) {
        do {
            if (check(TokenType::PARAMETER) || check(TokenType::LOCALPARAM)) {
                mod.parameters.push_back(parseParameterDeclaration());
            } else {
                std::string val;
                while (!check(TokenType::COMMA) && !check(TokenType::RPAREN) &&
                       !check(TokenType::END_OF_FILE))
                    val += consume().lexeme;
                mod.parameters.push_back(Parameter("", val));
            }
        } while (match(TokenType::COMMA));
    }
    expect(TokenType::RPAREN);
}

void Parser::parsePortList(Module& mod) {
    expect(TokenType::LPAREN);
    if (check(TokenType::RPAREN)) { consume(); return; }

    do {
        if (check(TokenType::RPAREN)) break;
        int line = current().line, col = current().column;

        if (check(TokenType::INPUT) || check(TokenType::OUTPUT) || check(TokenType::INOUT)) {
            // ANSI inline port
            PortDirection dir = PortDirection::INPUT;
            if      (check(TokenType::INPUT))  { dir = PortDirection::INPUT;  consume(); }
            else if (check(TokenType::OUTPUT)) { dir = PortDirection::OUTPUT; consume(); }
            else if (check(TokenType::INOUT))  { dir = PortDirection::INOUT;  consume(); }

            PortType ptype = PortType::UNSPECIFIED;
            if      (check(TokenType::WIRE))  { ptype = PortType::WIRE;  consume(); }
            else if (check(TokenType::REG))   { ptype = PortType::REG;   consume(); }
            else if (check(TokenType::LOGIC)) { ptype = PortType::LOGIC; consume(); }

            if (check(TokenType::SIGNED)) consume();

            // user-defined type: "output mem_req_4B_t portname" — first IDENTIFIER is the type
            if (check(TokenType::IDENTIFIER) && peek(1).type == TokenType::IDENTIFIER)
                consume(); // discard the typedef name

            PortWidth width;
            if (check(TokenType::LBRACKET)) width = parseWidth();

            std::string name = expect(TokenType::IDENTIFIER).lexeme;
            mod.ports.push_back(Port(dir, ptype, width, name, line, col));
        } else if (check(TokenType::IDENTIFIER)) {
            // non-ANSI: name only, direction declared later in body
            std::string name = consume().lexeme;
            mod.ports.push_back(Port(PortDirection::INPUT, PortType::UNSPECIFIED,
                                     PortWidth(), name, line, col));
        } else {
            consume();
        }
    } while (match(TokenType::COMMA));

    expect(TokenType::RPAREN);
}

// module body items

Port Parser::parsePortDeclaration() {
    int line = current().line, col = current().column;

    PortDirection dir = PortDirection::INPUT;
    if      (check(TokenType::INPUT))  { dir = PortDirection::INPUT;  consume(); }
    else if (check(TokenType::OUTPUT)) { dir = PortDirection::OUTPUT; consume(); }
    else if (check(TokenType::INOUT))  { dir = PortDirection::INOUT;  consume(); }

    PortType ptype = PortType::UNSPECIFIED;
    if      (check(TokenType::WIRE))  { ptype = PortType::WIRE;  consume(); }
    else if (check(TokenType::REG))   { ptype = PortType::REG;   consume(); }
    else if (check(TokenType::LOGIC)) { ptype = PortType::LOGIC; consume(); }

    if (check(TokenType::SIGNED)) consume();

    PortWidth width;
    if (check(TokenType::LBRACKET)) width = parseWidth();

    std::string name = expect(TokenType::IDENTIFIER).lexeme;
    expect(TokenType::SEMICOLON);
    return Port(dir, ptype, width, name, line, col);
}

std::vector<NetDecl> Parser::parseNetDeclaration() {
    int line = current().line, col = current().column;

    NetType ntype = NetType::WIRE;
    if      (check(TokenType::WIRE))    { ntype = NetType::WIRE;    consume(); }
    else if (check(TokenType::REG))     { ntype = NetType::REG;     consume(); }
    else if (check(TokenType::LOGIC))   { ntype = NetType::LOGIC;   consume(); }
    else if (check(TokenType::TRI))     { ntype = NetType::TRI;     consume(); }
    else if (check(TokenType::WAND))    { ntype = NetType::WAND;    consume(); }
    else if (check(TokenType::WOR))     { ntype = NetType::WOR;     consume(); }
    else if (check(TokenType::SUPPLY0)) { ntype = NetType::SUPPLY0; consume(); }
    else if (check(TokenType::SUPPLY1)) { ntype = NetType::SUPPLY1; consume(); }

    if (check(TokenType::SIGNED)) consume();

    PortWidth width;
    if (check(TokenType::LBRACKET)) width = parseWidth();

    std::vector<NetDecl> decls;
    do {
        std::string name = expect(TokenType::IDENTIFIER).lexeme;
        // skip unpacked array dimensions: logic [31:0] mem [0:N-1]
        while (check(TokenType::LBRACKET)) {
            consume();
            while (!check(TokenType::RBRACKET) && !check(TokenType::END_OF_FILE))
                consume();
            match(TokenType::RBRACKET);
        }
        ExprPtr init = nullptr;
        if (match(TokenType::EQ))
            init = parseExpression();
        decls.push_back(NetDecl(ntype, width, name, line, col, std::move(init)));
    } while (match(TokenType::COMMA));

    expect(TokenType::SEMICOLON);
    return decls;
}

Parameter Parser::parseParameterDeclaration() {
    consume(); // PARAMETER or LOCALPARAM
    std::string name = expect(TokenType::IDENTIFIER).lexeme;
    std::string default_val;
    if (match(TokenType::EQ)) {
        int depth = 0;
        while (!check(TokenType::END_OF_FILE)) {
            if (check(TokenType::LPAREN)) {
                depth++;
            } else if (check(TokenType::RPAREN)) {
                if (depth == 0) break;
                depth--;
            } else if (depth == 0 && (check(TokenType::SEMICOLON) || check(TokenType::COMMA))) {
                break;
            }
            default_val += consume().lexeme;
        }
    }
    match(TokenType::SEMICOLON);
    return Parameter(name, default_val);
}

Instance Parser::parseInstance() {
    int line = current().line, col = current().column;
    std::string module_name = consume().lexeme;
    Instance inst(module_name, "", line, col);

    if (match(TokenType::HASH)) {
        expect(TokenType::LPAREN);
        while (!check(TokenType::RPAREN) && !check(TokenType::END_OF_FILE)) {
            std::string param;
            int depth = 0;
            while (!check(TokenType::END_OF_FILE)) {
                if (check(TokenType::LPAREN)) {
                    depth++;
                } else if (check(TokenType::RPAREN)) {
                    if (depth == 0) break;
                    depth--;
                } else if (depth == 0 && check(TokenType::COMMA)) {
                    break;
                }
                param += consume().lexeme;
            }
            inst.parameters.push_back(param);
            if (!match(TokenType::COMMA)) break;
        }
        expect(TokenType::RPAREN);
    }

    inst.instance_name = expect(TokenType::IDENTIFIER).lexeme;

    expect(TokenType::LPAREN);
    while (!check(TokenType::RPAREN) && !check(TokenType::END_OF_FILE)) {
        int pline = current().line, pcol = current().column;
        if (match(TokenType::DOT)) {
            if (match(TokenType::STAR)) {
                // .* wildcard — connect all matching ports implicitly, skip
            } else {
            std::string port_name = expect(TokenType::IDENTIFIER).lexeme;
            expect(TokenType::LPAREN);
            std::string signal;
            while (!check(TokenType::RPAREN) && !check(TokenType::END_OF_FILE))
                signal += consume().lexeme;
            expect(TokenType::RPAREN);
            inst.connections.push_back(PortConnection(port_name, signal, pline, pcol));
            }
        } else {
            std::string signal;
            while (!check(TokenType::COMMA) && !check(TokenType::RPAREN) &&
                   !check(TokenType::END_OF_FILE))
                signal += consume().lexeme;
            inst.connections.push_back(PortConnection("", signal, pline, pcol));
        }
        if (!match(TokenType::COMMA)) break;
    }
    expect(TokenType::RPAREN);
    expect(TokenType::SEMICOLON);
    return inst;
}

std::vector<Assign> Parser::parseContinuousAssign() {
    expect(TokenType::ASSIGN);
    std::vector<Assign> assigns;
    do {
        int line = current().line, col = current().column;
        ExprPtr lhs = parseLValue();
        expect(TokenType::EQ);
        ExprPtr rhs = parseExpression();
        assigns.push_back(Assign(std::move(lhs), std::move(rhs), line, col));
    } while (match(TokenType::COMMA));
    expect(TokenType::SEMICOLON);
    return assigns;
}

AlwaysBlock Parser::parseAlwaysBlock() {
    int line = current().line, col = current().column;
    expect(TokenType::ALWAYS);
    std::string sensitivity = parseSensitivityList();
    StmtPtr body = parseStatement();
    return AlwaysBlock(sensitivity, std::move(body), line, col);
}

// always block internals

std::string Parser::parseSensitivityList() {
    if (!check(TokenType::AT)) return "";
    expect(TokenType::AT);
    if (check(TokenType::STAR)) { consume(); return "*"; } // @*
    expect(TokenType::LPAREN);
    if (check(TokenType::STAR)) { consume(); expect(TokenType::RPAREN); return "*"; } // @(*)
    std::string sens;
    while (!check(TokenType::RPAREN) && !check(TokenType::END_OF_FILE)) {
        if (!sens.empty()) sens += " ";
        sens += consume().lexeme;
    }
    expect(TokenType::RPAREN);
    return sens;
}

StmtPtr Parser::parseStatement() {
    if (check(TokenType::BEGIN)) return parseSeqBlock();
    if (check(TokenType::IF))    return parseIfStatement();
    if (check(TokenType::CASE) || check(TokenType::CASEX) || check(TokenType::CASEZ))
        return parseCaseStatement();
    if (check(TokenType::IDENTIFIER)) {
        ExprPtr lhs = parseLValue();
        if (check(TokenType::EQ))      return parseBlockingAssign(std::move(lhs));
        if (check(TokenType::LESS_EQ)) return parseNonBlockingAssign(std::move(lhs));
    }
    skipToSemicolon();
    match(TokenType::SEMICOLON);
    return nullptr;
}

StmtPtr Parser::parseSeqBlock() {
    int line = current().line, col = current().column;
    expect(TokenType::BEGIN);
    auto block = std::make_unique<SeqBlock>(line, col);
    while (!check(TokenType::END) && !check(TokenType::END_OF_FILE)) {
        StmtPtr stmt = parseStatement();
        if (stmt) block->body.push_back(std::move(stmt));
    }
    expect(TokenType::END);
    return block;
}

StmtPtr Parser::parseIfStatement() {
    int line = current().line, col = current().column;
    expect(TokenType::IF);
    expect(TokenType::LPAREN);
    ExprPtr cond = parseExpression();
    expect(TokenType::RPAREN);
    StmtPtr then_branch = parseStatement();
    StmtPtr else_branch = nullptr;
    if (match(TokenType::ELSE))
        else_branch = parseStatement();
    return std::make_unique<IfStatement>(
        std::move(cond), std::move(then_branch), std::move(else_branch), line, col);
}

StmtPtr Parser::parseCaseStatement() {
    int line = current().line, col = current().column;
    std::string variant = current().lexeme;
    consume();
    expect(TokenType::LPAREN);
    ExprPtr expr = parseExpression();
    expect(TokenType::RPAREN);
    auto stmt = std::make_unique<CaseStatement>(variant, std::move(expr), line, col);
    while (!check(TokenType::ENDCASE) && !check(TokenType::END_OF_FILE)) {
        auto item = parseCaseItem();
        if (item) stmt->items.push_back(std::move(*item));
    }
    expect(TokenType::ENDCASE);
    return stmt;
}

std::unique_ptr<CaseItem> Parser::parseCaseItem() {
    auto item = std::make_unique<CaseItem>();
    if (check(TokenType::DEFAULT)) {
        consume();
        match(TokenType::COLON);
    } else {
        do {
            item->patterns.push_back(parseExpression());
        } while (match(TokenType::COMMA) && !check(TokenType::COLON));
        expect(TokenType::COLON);
    }
    item->body = parseStatement();
    return item;
}

StmtPtr Parser::parseBlockingAssign(ExprPtr lhs) {
    int line = current().line, col = current().column;
    expect(TokenType::EQ);
    ExprPtr rhs = parseExpression();
    expect(TokenType::SEMICOLON);
    return std::make_unique<BlockingAssign>(std::move(lhs), std::move(rhs), line, col);
}

StmtPtr Parser::parseNonBlockingAssign(ExprPtr lhs) {
    int line = current().line, col = current().column;
    expect(TokenType::LESS_EQ);
    ExprPtr rhs = parseExpression();
    expect(TokenType::SEMICOLON);
    return std::make_unique<NonBlockingAssign>(std::move(lhs), std::move(rhs), line, col);
}

// expression parsing

int Parser::binaryPrecedence(TokenType t) {
    switch (t) {
        case TokenType::PIPE_PIPE:                              return 2;
        case TokenType::AMP_AMP:                                return 3;
        case TokenType::PIPE:                                   return 4;
        case TokenType::CARET:                                  return 5;
        case TokenType::AMP:                                    return 6;
        case TokenType::EQ_EQ:     case TokenType::BANG_EQ:
        case TokenType::EQ_EQ_EQ:  case TokenType::BANG_EQ_EQ:  return 7;
        case TokenType::LESS:      case TokenType::LESS_EQ:
        case TokenType::GREATER:   case TokenType::GREATER_EQ:  return 8;
        case TokenType::LESS_LESS: case TokenType::GREATER_GREATER:
        case TokenType::LESS_LESS_LESS:
        case TokenType::GREATER_GREATER_GREATER:                return 9;
        case TokenType::PLUS:      case TokenType::MINUS:       return 10;
        case TokenType::STAR:      case TokenType::SLASH:
        case TokenType::PERCENT:                                return 11;
        default: return -1;
    }
}

ExprPtr Parser::parseExpression() {
    ExprPtr lhs = parseBinary(1);
    // ternary is lowest precedence and right-associative
    if (match(TokenType::QUESTION)) {
        ExprPtr then_ = parseExpression();
        expect(TokenType::COLON);
        ExprPtr else_ = parseExpression();
        return std::make_unique<ConditionalExpr>(
            std::move(lhs), std::move(then_), std::move(else_));
    }
    return lhs;
}

ExprPtr Parser::parseBinary(int minPrec) {
    ExprPtr lhs = parseUnary();
    while (true) {
        int prec = binaryPrecedence(current().type);
        if (prec < minPrec) break;
        std::string op = consume().lexeme;
        ExprPtr rhs = parseBinary(prec + 1); // left-associative
        lhs = std::make_unique<BinaryOpExpr>(op, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

ExprPtr Parser::parseUnary() {
    if (check(TokenType::TILDE)      || check(TokenType::BANG)       ||
        check(TokenType::MINUS)      || check(TokenType::PLUS)       ||
        check(TokenType::AMP)        || check(TokenType::PIPE)       ||
        check(TokenType::CARET)      || check(TokenType::TILDE_AMP)  ||
        check(TokenType::TILDE_PIPE) || check(TokenType::TILDE_CARET)) {
        std::string op = consume().lexeme;
        return std::make_unique<UnaryOpExpr>(op, parseUnary());
    }
    return parseAtom();
}

ExprPtr Parser::parseAtom() {
    // parenthesised expression
    if (match(TokenType::LPAREN)) {
        ExprPtr inner = parseExpression();
        expect(TokenType::RPAREN);
        return inner;
    }

    // concatenation or replication: { ... }
    if (check(TokenType::LBRACE)) {
        consume();
        ExprPtr first = parseExpression();
        if (check(TokenType::LBRACE)) {
            // handling replication: {n{expr}}
            consume();
            ExprPtr value = parseExpression();
            expect(TokenType::RBRACE);
            expect(TokenType::RBRACE);
            return std::make_unique<ReplicationExpr>(std::move(first), std::move(value));
        }
        auto concat = std::make_unique<ConcatExpr>();
        concat->parts.push_back(std::move(first));
        while (match(TokenType::COMMA))
            concat->parts.push_back(parseExpression());
        expect(TokenType::RBRACE);
        return concat;
    }

    // literals
    if (check(TokenType::INTEGER_LITERAL) || check(TokenType::REAL_LITERAL) ||
        check(TokenType::STRING_LITERAL)  || check(TokenType::TIME_LITERAL))
        return std::make_unique<LiteralExpr>(consume().lexeme);

    // identifier with optional dot-access chain and bit/part select
    if (check(TokenType::IDENTIFIER)) {
        std::string name = consume().lexeme;
        while (check(TokenType::DOT) && peek(1).type == TokenType::IDENTIFIER) {
            consume(); // .
            name += "." + consume().lexeme;
        }
        ExprPtr base = std::make_unique<IdentifierExpr>(name);
        if (check(TokenType::LBRACKET)) {
            consume();
            ExprPtr idx = parseExpression();
            if (match(TokenType::COLON)) {
                ExprPtr lsb = parseExpression();
                expect(TokenType::RBRACKET);
                return std::make_unique<PartSelectExpr>(
                    std::move(base), std::move(idx), std::move(lsb));
            }
            expect(TokenType::RBRACKET);
            return std::make_unique<BitSelectExpr>(std::move(base), std::move(idx));
        }
        return base;
    }

    // system task as expression: $clog2(N)
    if (check(TokenType::SYSTEM_TASK)) {
        ExprPtr base = std::make_unique<IdentifierExpr>(consume().lexeme);
        if (match(TokenType::LPAREN)) {
            auto args = std::make_unique<ConcatExpr>();
            if (!check(TokenType::RPAREN))
                args->parts.push_back(parseExpression());
            while (match(TokenType::COMMA))
                args->parts.push_back(parseExpression());
            expect(TokenType::RPAREN);
            return std::make_unique<BinaryOpExpr>("call", std::move(base), std::move(args));
        }
        return base;
    }

    return std::make_unique<LiteralExpr>(consume().lexeme); // fallback
}

ExprPtr Parser::parseLValue() {
    if (check(TokenType::LBRACE)) {
        consume();
        auto concat = std::make_unique<ConcatExpr>();
        if (!check(TokenType::RBRACE))
            concat->parts.push_back(parseLValue());
        while (match(TokenType::COMMA))
            concat->parts.push_back(parseLValue());
        expect(TokenType::RBRACE);
        return concat;
    }
    std::string baseName = expect(TokenType::IDENTIFIER).lexeme;
    // struct/record member access: signal.field (e.g. msg.type_)
    while (check(TokenType::DOT) && peek(1).type == TokenType::IDENTIFIER) {
        consume(); // .
        baseName += "." + consume().lexeme;
    }
    ExprPtr base = std::make_unique<IdentifierExpr>(baseName);
    if (check(TokenType::LBRACKET)) {
        consume();
        ExprPtr idx = parseExpression();
        if (match(TokenType::COLON)) {
            ExprPtr lsb = parseExpression();
            expect(TokenType::RBRACKET);
            return std::make_unique<PartSelectExpr>(
                std::move(base), std::move(idx), std::move(lsb));
        }
        expect(TokenType::RBRACKET);
        return std::make_unique<BitSelectExpr>(std::move(base), std::move(idx));
    }
    return base;
}

PortWidth Parser::parseWidth() {
    expect(TokenType::LBRACKET);

    // extract int from a simple literal expression, -1 if complex
    auto tryInt = [](const ExprPtr& e) -> int {
        if (e && e->kind == ExprKind::LITERAL) {
            try { return std::stoi(static_cast<const LiteralExpr*>(e.get())->value); }
            catch (...) {}
        }
        return -1;
    };

    ExprPtr msb_expr = parseExpression();
    expect(TokenType::COLON);
    ExprPtr lsb_expr = parseExpression();
    expect(TokenType::RBRACKET);

    int msb = tryInt(msb_expr);
    int lsb = tryInt(lsb_expr);
    if (msb >= 0 && lsb >= 0) return PortWidth(msb, lsb);
    return PortWidth(0, 0); // fallback for complex widths e.g. [$clog2(N)-1:0]
}

// json serialization

static json exprToJson(const ExprPtr& e) {
    if (!e) return nullptr;
    switch (e->kind) {
        case ExprKind::IDENTIFIER:
            return { {"kind", "IDENTIFIER"}, {"name", static_cast<const IdentifierExpr*>(e.get())->name} };
        case ExprKind::LITERAL:
            return { {"kind", "LITERAL"}, {"value", static_cast<const LiteralExpr*>(e.get())->value} };
        case ExprKind::UNARY_OP: {
            auto* n = static_cast<const UnaryOpExpr*>(e.get());
            return { {"kind", "UNARY_OP"}, {"op", n->op}, {"operand", exprToJson(n->operand)} };
        }
        case ExprKind::BINARY_OP: {
            auto* n = static_cast<const BinaryOpExpr*>(e.get());
            return { {"kind", "BINARY_OP"}, {"op", n->op},
                     {"lhs", exprToJson(n->lhs)}, {"rhs", exprToJson(n->rhs)} };
        }
        case ExprKind::CONDITIONAL: {
            auto* n = static_cast<const ConditionalExpr*>(e.get());
            return { {"kind", "CONDITIONAL"}, {"cond", exprToJson(n->cond)},
                     {"then", exprToJson(n->then_)}, {"else", exprToJson(n->else_)} };
        }
        case ExprKind::CONCAT: {
            auto* n = static_cast<const ConcatExpr*>(e.get());
            json parts = json::array();
            for (const auto& p : n->parts) parts.push_back(exprToJson(p));
            return { {"kind", "CONCAT"}, {"parts", parts} };
        }
        case ExprKind::REPLICATION: {
            auto* n = static_cast<const ReplicationExpr*>(e.get());
            return { {"kind", "REPLICATION"}, {"count", exprToJson(n->count)},
                     {"value", exprToJson(n->value)} };
        }
        case ExprKind::BIT_SELECT: {
            auto* n = static_cast<const BitSelectExpr*>(e.get());
            return { {"kind", "BIT_SELECT"}, {"base", exprToJson(n->base)},
                     {"index", exprToJson(n->index)} };
        }
        case ExprKind::PART_SELECT: {
            auto* n = static_cast<const PartSelectExpr*>(e.get());
            return { {"kind", "PART_SELECT"}, {"base", exprToJson(n->base)},
                     {"msb", exprToJson(n->msb)}, {"lsb", exprToJson(n->lsb)} };
        }
        default: return nullptr;
    }
}

static json stmtToJson(const StmtPtr& s);

static json caseItemToJson(const CaseItem& item) {
    json patterns = json::array();
    for (const auto& p : item.patterns) patterns.push_back(exprToJson(p));
    return { {"patterns", patterns}, {"body", stmtToJson(item.body)} };
}

static json stmtToJson(const StmtPtr& s) {
    if (!s) return nullptr;
    switch (s->kind) {
        case StatementKind::SEQ_BLOCK: {
            auto* n = static_cast<const SeqBlock*>(s.get());
            json body = json::array();
            for (const auto& stmt : n->body) body.push_back(stmtToJson(stmt));
            return { {"kind", "SEQ_BLOCK"}, {"body", body} };
        }
        case StatementKind::BLOCKING_ASSIGN: {
            auto* n = static_cast<const BlockingAssign*>(s.get());
            return { {"kind", "BLOCKING_ASSIGN"},
                     {"lhs", exprToJson(n->lhs)}, {"rhs", exprToJson(n->rhs)} };
        }
        case StatementKind::NONBLOCKING_ASSIGN: {
            auto* n = static_cast<const NonBlockingAssign*>(s.get());
            return { {"kind", "NONBLOCKING_ASSIGN"},
                     {"lhs", exprToJson(n->lhs)}, {"rhs", exprToJson(n->rhs)} };
        }
        case StatementKind::IF_STATEMENT: {
            auto* n = static_cast<const IfStatement*>(s.get());
            return { {"kind", "IF_STATEMENT"}, {"cond", exprToJson(n->cond)},
                     {"then", stmtToJson(n->then_branch)},
                     {"else", stmtToJson(n->else_branch)} };
        }
        case StatementKind::CASE_STATEMENT: {
            auto* n = static_cast<const CaseStatement*>(s.get());
            json items = json::array();
            for (const auto& item : n->items) items.push_back(caseItemToJson(item));
            return { {"kind", "CASE_STATEMENT"}, {"variant", n->variant},
                     {"expr", exprToJson(n->expr)}, {"items", items} };
        }
        default: return nullptr;
    }
}

static std::string portDirectionStr(PortDirection d) {
    switch (d) {
        case PortDirection::INPUT:  return "input";
        case PortDirection::OUTPUT: return "output";
        case PortDirection::INOUT:  return "inout";
        default: return "unknown";
    }
}

static std::string portTypeStr(PortType t) {
    switch (t) {
        case PortType::WIRE:        return "wire";
        case PortType::REG:         return "reg";
        case PortType::LOGIC:       return "logic";
        case PortType::UNSPECIFIED: return "unspecified";
        default:                    return "unspecified";
    }
}

static std::string netTypeStr(NetType t) {
    switch (t) {
        case NetType::WIRE:    return "wire";
        case NetType::REG:     return "reg";
        case NetType::LOGIC:   return "logic";
        case NetType::TRI:     return "tri";
        case NetType::WAND:    return "wand";
        case NetType::WOR:     return "wor";
        case NetType::SUPPLY0: return "supply0";
        case NetType::SUPPLY1: return "supply1";
        default:               return "wire";
    }
}

static json moduleToJson(const Module& mod) {
    json ports = json::array();
    for (const auto& p : mod.ports)
        ports.push_back({
            {"name",      p.name},
            {"direction", portDirectionStr(p.direction)},
            {"type",      portTypeStr(p.type)},
            {"msb",       p.width.msb},
            {"lsb",       p.width.lsb},
            {"scalar",    p.width.scalar}
        });

    json parameters = json::array();
    for (const auto& p : mod.parameters)
        parameters.push_back({ {"name", p.name}, {"default", p.default_value} });

    json net_decls = json::array();
    for (const auto& n : mod.net_decls) {
        json nd = {
            {"name",   n.name},
            {"type",   netTypeStr(n.net_type)},
            {"msb",    n.width.msb},
            {"lsb",    n.width.lsb},
            {"scalar", n.width.scalar}
        };
        if (n.init) nd["init"] = exprToJson(n.init);
        net_decls.push_back(nd);
    }

    json instances = json::array();
    for (const auto& inst : mod.instances) {
        json connections = json::array();
        for (const auto& c : inst.connections)
            connections.push_back({ {"port", c.port_name}, {"signal", c.signal} });
        instances.push_back({
            {"module",      inst.module_name},
            {"instance",    inst.instance_name},
            {"resolved",    inst.resolved},
            {"parameters",  inst.parameters},
            {"connections", connections}
        });
    }

    json assigns = json::array();
    for (const auto& a : mod.assigns)
        assigns.push_back({ {"lhs", exprToJson(a.lhs)}, {"rhs", exprToJson(a.rhs)} });

    json always_blocks = json::array();
    for (const auto& ab : mod.always_blocks)
        always_blocks.push_back({
            {"sensitivity", ab.sensitivity},
            {"body",        stmtToJson(ab.body)}
        });

    return {
        {"name",          mod.name},
        {"source_file",   mod.source_file},
        {"parameters",    parameters},
        {"ports",         ports},
        {"net_decls",     net_decls},
        {"instances",     instances},
        {"assigns",       assigns},
        {"always_blocks", always_blocks}
    };
}

std::string modulesToJSON(const std::vector<Module>& modules) {
    json root = json::array();
    for (const auto& mod : modules)
        root.push_back(moduleToJson(mod));
    return root.dump(2);
}

std::string Parser::toAST() {
    return modulesToJSON(parse());
}
