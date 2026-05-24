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
    // Stop at SEMICOLON (the normal terminator) or at module/file boundaries so
    // a malformed declaration cannot accidentally swallow the rest of a module.
    while (!check(TokenType::SEMICOLON)  &&
           !check(TokenType::ENDMODULE)  &&
           !check(TokenType::END_OF_FILE))
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

// Consume tokens until the matching end token. Used to skip function/task bodies.
void Parser::skipUntil(TokenType end) {
    while (!check(end) && !check(TokenType::END_OF_FILE)) consume();
    match(end);
}

// Skip a balanced parenthesised group. Caller positions cursor at LPAREN.
void Parser::skipParens() {
    if (!check(TokenType::LPAREN)) return;
    consume(); // (
    int depth = 1;
    while (depth > 0 && !check(TokenType::END_OF_FILE)) {
        if      (check(TokenType::LPAREN)) depth++;
        else if (check(TokenType::RPAREN)) depth--;
        consume();
    }
}

// Skip one procedural statement (used for for/while/repeat/forever bodies that
// we don't model). Honours begin/end nesting and falls back to skipping to ';'.
void Parser::skipStatement() {
    if (check(TokenType::BEGIN)) { skipBlock(); return; }
    if (check(TokenType::IF)) {
        consume();
        skipParens();
        skipStatement();
        if (match(TokenType::ELSE)) skipStatement();
        return;
    }
    if (check(TokenType::CASE) || check(TokenType::CASEX) || check(TokenType::CASEZ)) {
        consume();
        skipParens();
        skipUntil(TokenType::ENDCASE);
        return;
    }
    if (check(TokenType::FOR) || check(TokenType::WHILE) || check(TokenType::REPEAT)) {
        consume();
        skipParens();
        skipStatement();
        return;
    }
    if (check(TokenType::FOREVER)) {
        consume();
        skipStatement();
        return;
    }
    skipToSemicolon();
    match(TokenType::SEMICOLON);
}

// typedef [enum [...] [{...}]] name; — consume from TYPEDEF through trailing ';'
// while honouring brace nesting (typedef enum { A, B } state_t;).
void Parser::skipTypedef() {
    int braces = 0;
    while (!check(TokenType::END_OF_FILE)) {
        if      (check(TokenType::LBRACE)) braces++;
        else if (check(TokenType::RBRACE)) braces--;
        else if (braces == 0 && check(TokenType::SEMICOLON)) {
            consume();
            return;
        }
        consume();
    }
}

// top level

Module Parser::parseModule() {
    int line = current().line, col = current().column;
    expect(TokenType::MODULE);
    Module mod("", line, col);
    parseModuleHeader(mod);

    while (!check(TokenType::ENDMODULE) && !check(TokenType::END_OF_FILE))
        parseModuleBodyItem(mod);

    match(TokenType::ENDMODULE);
    return mod;
}

// Single dispatch for a module body item. Shared with parseGenerateBlock so
// instances/assigns/always blocks inside `generate ... endgenerate` end up
// in the module's vectors rather than being silently dropped.
void Parser::parseModuleBodyItem(Module& mod) {
    if (check(TokenType::INPUT) || check(TokenType::OUTPUT) || check(TokenType::INOUT)) {
        // Non-ANSI style: port list creates stubs, body declarations refine them.
        // One declaration may name several ports (output reg [3:0] q1, q2, q3;)
        // so we update or insert each name independently.
        for (auto& decl : parsePortDeclaration()) {
            auto it = std::find_if(mod.ports.begin(), mod.ports.end(),
                [&](const Port& p){ return p.name == decl.name; });
            if (it != mod.ports.end()) *it = decl;
            else                       mod.ports.push_back(std::move(decl));
        }
        return;
    }
    if (check(TokenType::WIRE)    || check(TokenType::REG)    ||
        check(TokenType::LOGIC)   || check(TokenType::TRI)    ||
        check(TokenType::TRI0)    || check(TokenType::TRI1)   ||
        check(TokenType::WAND)    || check(TokenType::WOR)    ||
        check(TokenType::SUPPLY0) || check(TokenType::SUPPLY1)) {
        for (auto& d : parseNetDeclaration())
            mod.net_decls.push_back(std::move(d));
        return;
    }
    if (check(TokenType::PARAMETER) || check(TokenType::LOCALPARAM)) {
        for (auto& p : parseParameterDeclaration())
            mod.parameters.push_back(std::move(p));
        return;
    }
    if (check(TokenType::ASSIGN)) {
        for (auto& a : parseContinuousAssign())
            mod.assigns.push_back(std::move(a));
        return;
    }
    if (check(TokenType::ALWAYS)       || check(TokenType::ALWAYS_FF) ||
        check(TokenType::ALWAYS_COMB)  || check(TokenType::ALWAYS_LATCH)) {
        mod.always_blocks.push_back(parseAlwaysBlock());
        return;
    }
    if (check(TokenType::INITIAL)) {
        consume();
        if (check(TokenType::BEGIN)) skipBlock();
        else                         skipStatement();
        return;
    }
    if (check(TokenType::AND)    || check(TokenType::OR)     ||
        check(TokenType::NOT)    || check(TokenType::NAND)   ||
        check(TokenType::NOR)    || check(TokenType::XOR)    ||
        check(TokenType::XNOR)   || check(TokenType::BUF)    ||
        check(TokenType::BUFIF0) || check(TokenType::BUFIF1) ||
        check(TokenType::NOTIF0) || check(TokenType::NOTIF1)) {
        for (auto& g : parseGatePrimitive())
            mod.gate_primitives.push_back(std::move(g));
        return;
    }
    if (check(TokenType::GENERATE)) {
        parseGenerateBlock(mod);
        return;
    }
    if (check(TokenType::GENVAR)) {
        skipToSemicolon();
        match(TokenType::SEMICOLON);
        return;
    }
    if (check(TokenType::FUNCTION)) {
        consume();
        skipUntil(TokenType::ENDFUNCTION);
        return;
    }
    if (check(TokenType::TASK)) {
        consume();
        skipUntil(TokenType::ENDTASK);
        return;
    }
    if (check(TokenType::SPECIFY)) {
        consume();
        skipUntil(TokenType::ENDSPECIFY);
        return;
    }
    if (check(TokenType::TYPEDEF)) {
        skipTypedef();
        return;
    }
    if (check(TokenType::DEFPARAM)) {
        skipToSemicolon();
        match(TokenType::SEMICOLON);
        return;
    }
    if (check(TokenType::INTEGER) || check(TokenType::REAL) ||
        check(TokenType::TIME)    || check(TokenType::REALTIME)) {
        // Procedural-only declarations — not part of the structural model.
        skipToSemicolon();
        match(TokenType::SEMICOLON);
        return;
    }
    if (check(TokenType::IDENTIFIER)) {
        // Disambiguate instance vs typedef'd net declaration.
        bool is_instance = module_names_.count(current().lexeme)
            || peek(1).type == TokenType::HASH
            || (peek(1).type == TokenType::IDENTIFIER
                && peek(2).type == TokenType::LPAREN);
        if (is_instance)
            mod.instances.push_back(parseInstance());
        else { skipToSemicolon(); match(TokenType::SEMICOLON); }
        return;
    }
    // Unrecognised token: drop it to make progress instead of looping forever.
    consume();
}

// generate ... endgenerate — extract assigns/instances/always blocks from the
// body, shallow-skipping for/if/case headers that we don't elaborate.
void Parser::parseGenerateBlock(Module& mod) {
    expect(TokenType::GENERATE);
    while (!check(TokenType::ENDGENERATE) && !check(TokenType::END_OF_FILE)) {
        // for (...) begin [: label] ... end
        if (check(TokenType::FOR) || check(TokenType::WHILE)) {
            consume();
            skipParens();
            if (check(TokenType::BEGIN)) {
                consume();
                if (match(TokenType::COLON)) match(TokenType::IDENTIFIER); // : label
                while (!check(TokenType::END) && !check(TokenType::END_OF_FILE))
                    parseModuleBodyItem(mod);
                match(TokenType::END);
            } else {
                parseModuleBodyItem(mod);
            }
            continue;
        }
        // if (...) begin ... end [else ...]
        if (check(TokenType::IF)) {
            consume();
            skipParens();
            if (check(TokenType::BEGIN)) {
                consume();
                if (match(TokenType::COLON)) match(TokenType::IDENTIFIER);
                while (!check(TokenType::END) && !check(TokenType::END_OF_FILE))
                    parseModuleBodyItem(mod);
                match(TokenType::END);
            } else {
                parseModuleBodyItem(mod);
            }
            if (match(TokenType::ELSE)) {
                if (check(TokenType::BEGIN)) {
                    consume();
                    if (match(TokenType::COLON)) match(TokenType::IDENTIFIER);
                    while (!check(TokenType::END) && !check(TokenType::END_OF_FILE))
                        parseModuleBodyItem(mod);
                    match(TokenType::END);
                } else {
                    parseModuleBodyItem(mod);
                }
            }
            continue;
        }
        // case (...) <expr>: begin ... end ... endcase
        if (check(TokenType::CASE) || check(TokenType::CASEX) || check(TokenType::CASEZ)) {
            consume();
            skipParens();
            skipUntil(TokenType::ENDCASE);
            continue;
        }
        // bare begin block (e.g. generate begin ... end)
        if (check(TokenType::BEGIN)) {
            consume();
            if (match(TokenType::COLON)) match(TokenType::IDENTIFIER);
            while (!check(TokenType::END) && !check(TokenType::END_OF_FILE))
                parseModuleBodyItem(mod);
            match(TokenType::END);
            continue;
        }
        parseModuleBodyItem(mod);
    }
    match(TokenType::ENDGENERATE);
}

// module header

void Parser::parseModuleHeader(Module& mod) {
    mod.name = expect(TokenType::IDENTIFIER).lexeme;
    if (match(TokenType::HASH))
        parseParameterList(mod);
    // Port list is optional: both `module foo;` and `module foo(a,b);` are legal.
    // Without parens the module has no external ports.
    if (check(TokenType::LPAREN))
        parsePortList(mod);
    expect(TokenType::SEMICOLON);
}

void Parser::parseParameterList(Module& mod) {
    expect(TokenType::LPAREN);
    if (!check(TokenType::RPAREN)) {
        do {
            if (check(TokenType::PARAMETER) || check(TokenType::LOCALPARAM)) {
                for (auto& p : parseParameterDeclaration())
                    mod.parameters.push_back(std::move(p));
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

    // Track current ANSI direction context so comma-separated names in the same
    // group all inherit it: "input a, b, cin, output sum, cout" works correctly.
    PortDirection curDir   = PortDirection::INPUT;
    PortType      curType  = PortType::UNSPECIFIED;
    PortWidth     curWidth;
    bool          hasAnsiDir = false;

    while (!check(TokenType::RPAREN) && !check(TokenType::END_OF_FILE)) {
        if (check(TokenType::COMMA)) { consume(); continue; }

        int line = current().line, col = current().column;

        if (check(TokenType::INPUT) || check(TokenType::OUTPUT) || check(TokenType::INOUT)) {
            // New direction keyword — update context for this group.
            if      (check(TokenType::INPUT))  { curDir = PortDirection::INPUT;  consume(); }
            else if (check(TokenType::OUTPUT)) { curDir = PortDirection::OUTPUT; consume(); }
            else                               { curDir = PortDirection::INOUT;  consume(); }

            curType = PortType::UNSPECIFIED;
            if      (check(TokenType::WIRE))  { curType = PortType::WIRE;  consume(); }
            else if (check(TokenType::REG))   { curType = PortType::REG;   consume(); }
            else if (check(TokenType::LOGIC)) { curType = PortType::LOGIC; consume(); }

            if (check(TokenType::SIGNED)) consume();

            // User-defined type. Two shapes:
            //   1) Plain typedef:        IDENTIFIER IDENTIFIER             (type name)
            //   2) Package-scoped type:  IDENTIFIER :: IDENTIFIER IDENTIFIER
            // In both cases the trailing IDENTIFIER is the port name.
            if (check(TokenType::IDENTIFIER)               &&
                peek(1).type == TokenType::COLON_COLON     &&
                peek(2).type == TokenType::IDENTIFIER      &&
                peek(3).type == TokenType::IDENTIFIER) {
                consume(); consume(); consume(); // pkg :: type
            } else if (check(TokenType::IDENTIFIER) && peek(1).type == TokenType::IDENTIFIER) {
                consume();
            }

            curWidth = PortWidth();
            if (check(TokenType::LBRACKET)) curWidth = parseWidth();

            hasAnsiDir = true;

            if (check(TokenType::IDENTIFIER)) {
                std::string name = consume().lexeme;
                mod.ports.push_back(Port(curDir, curType, curWidth, name, line, col));
            }
        } else if (check(TokenType::IDENTIFIER)) {
            std::string name = consume().lexeme;
            if (hasAnsiDir) {
                // Continuation of current direction group (e.g. "b, cin" after "input a").
                mod.ports.push_back(Port(curDir, curType, curWidth, name, line, col));
            } else {
                // Non-ANSI style: bare name, direction declared in module body.
                mod.ports.push_back(Port(PortDirection::INPUT, PortType::UNSPECIFIED,
                                         PortWidth(), name, line, col));
            }
        } else {
            consume();
        }
    }

    expect(TokenType::RPAREN);
}

// module body items

std::vector<Port> Parser::parsePortDeclaration() {
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

    // One direction/type/width can declare multiple comma-separated ports:
    //   output reg [3:0] q1, q2, q3;
    std::vector<Port> ports;
    do {
        int pline = current().line, pcol = current().column;
        std::string name = expect(TokenType::IDENTIFIER).lexeme;
        ports.push_back(Port(dir, ptype, width, name, pline, pcol));
    } while (match(TokenType::COMMA));

    expect(TokenType::SEMICOLON);
    return ports;
}

std::vector<NetDecl> Parser::parseNetDeclaration() {
    int line = current().line, col = current().column;

    NetType ntype = NetType::WIRE;
    if      (check(TokenType::WIRE))    { ntype = NetType::WIRE;    consume(); }
    else if (check(TokenType::REG))     { ntype = NetType::REG;     consume(); }
    else if (check(TokenType::LOGIC))   { ntype = NetType::LOGIC;   consume(); }
    else if (check(TokenType::TRI))     { ntype = NetType::TRI;     consume(); }
    else if (check(TokenType::TRI0))    { ntype = NetType::TRI;     consume(); }
    else if (check(TokenType::TRI1))    { ntype = NetType::TRI;     consume(); }
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

std::vector<Parameter> Parser::parseParameterDeclaration() {
    consume(); // PARAMETER or LOCALPARAM

    // Skip optional type prefix.  Verilog/SV allow several shapes between the
    // `parameter` keyword and the name:
    //   parameter signed NAME = ...
    //   parameter int NAME = ...           (SV built-in types — `int`, `bit`, etc. lex as IDENTIFIER)
    //   parameter logic [WIDTH-1:0] NAME = ...
    //   parameter MY_TYPE_T NAME = ...     (user typedef)
    // The actual name is the IDENTIFIER directly followed by `=`, `,`, `;`, or `)`.
    auto isTypeKeyword = [](TokenType t) {
        return t == TokenType::SIGNED   || t == TokenType::UNSIGNED ||
               t == TokenType::LOGIC    || t == TokenType::REG      ||
               t == TokenType::WIRE     || t == TokenType::INTEGER  ||
               t == TokenType::REAL     || t == TokenType::TIME     ||
               t == TokenType::REALTIME;
    };
    while (isTypeKeyword(current().type)) consume();
    // User typedef name: leading IDENTIFIER followed by another IDENTIFIER.
    if (current().type == TokenType::IDENTIFIER &&
        peek(1).type    == TokenType::IDENTIFIER)
        consume();
    // Optional packed range [msb:lsb] — possibly multiple dimensions.
    while (check(TokenType::LBRACKET)) {
        consume();
        int depth = 1;
        while (depth > 0 && !check(TokenType::END_OF_FILE)) {
            if      (check(TokenType::LBRACKET)) depth++;
            else if (check(TokenType::RBRACKET)) depth--;
            consume();
        }
    }

    // One `parameter` keyword can introduce several comma-separated names:
    //   parameter A = 1, B = 2, C = 3;
    // But in a header parameter list  #(parameter A = 1, parameter B = 2)
    // the comma belongs to the outer list, not to us — so we only continue
    // when the comma is followed by another IDENTIFIER (a bare continuation).
    std::vector<Parameter> params;
    while (true) {
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
                } else if (depth == 0 &&
                           (check(TokenType::SEMICOLON) || check(TokenType::COMMA))) {
                    break;
                }
                default_val += consume().lexeme;
            }
        }
        params.emplace_back(name, default_val);

        if (!check(TokenType::COMMA))                       break;
        if (peek(1).type != TokenType::IDENTIFIER)          break;
        consume(); // ,
    }

    match(TokenType::SEMICOLON);
    return params;
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
                // .* wildcard — connect all matching ports implicitly. Flag the
                // instance so the validator doesn't complain about every output
                // looking disconnected.
                inst.wildcard = true;
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

std::vector<GatePrimitive> Parser::parseGatePrimitive() {
    int line = current().line, col = current().column;
    std::string gate_type = consume().lexeme; // e.g. "and", "or", "not"

    // Skip optional drive strength: (strong0, weak1) etc.
    // Strength tokens appear as the first LPAREN followed by strength keywords.
    // We peek ahead: if the token after LPAREN is a strength keyword, skip the pair.
    if (check(TokenType::LPAREN)) {
        TokenType next = peek(1).type;
        if (next == TokenType::STRONG0 || next == TokenType::STRONG1 ||
            next == TokenType::WEAK0   || next == TokenType::WEAK1   ||
            next == TokenType::HIGHZ0  || next == TokenType::HIGHZ1  ||
            next == TokenType::PULL0   || next == TokenType::PULL1) {
            consume(); // (
            while (!check(TokenType::RPAREN) && !check(TokenType::END_OF_FILE))
                consume();
            match(TokenType::RPAREN);
        }
    }

    // Skip optional delay: #5 or #(1,2,3)
    if (match(TokenType::HASH)) {
        if (check(TokenType::LPAREN)) {
            consume(); // (
            int depth = 1;
            while (depth > 0 && !check(TokenType::END_OF_FILE)) {
                if      (check(TokenType::LPAREN)) depth++;
                else if (check(TokenType::RPAREN)) depth--;
                consume();
            }
        } else {
            consume(); // single delay value
        }
    }

    // Parse one or more gate instances: [name] (port, port, ...)
    std::vector<GatePrimitive> gates;
    do {
        // Optional instance name — present when current token is IDENTIFIER
        // and next is LPAREN (not another IDENTIFIER, which would be a net).
        std::string inst_name;
        if (check(TokenType::IDENTIFIER) && peek(1).type == TokenType::LPAREN) {
            inst_name = consume().lexeme;
        }

        expect(TokenType::LPAREN);
        std::vector<std::string> ports;
        while (!check(TokenType::RPAREN) && !check(TokenType::END_OF_FILE)) {
            // Collect one port expression as raw text (handles bit-selects, etc.)
            std::string sig;
            int depth = 0;
            while (!check(TokenType::END_OF_FILE)) {
                if      (check(TokenType::LPAREN)) { depth++; sig += consume().lexeme; }
                else if (check(TokenType::RPAREN)) { if (depth == 0) break; depth--; sig += consume().lexeme; }
                else if (depth == 0 && check(TokenType::COMMA)) break;
                else sig += consume().lexeme;
            }
            // trim leading/trailing whitespace
            auto s = sig.find_first_not_of(' ');
            auto e = sig.find_last_not_of(' ');
            if (s != std::string::npos) sig = sig.substr(s, e - s + 1);
            ports.push_back(sig);
            if (!match(TokenType::COMMA)) break;
        }
        expect(TokenType::RPAREN);
        gates.emplace_back(gate_type, inst_name, std::move(ports), line, col);
    } while (match(TokenType::COMMA));

    expect(TokenType::SEMICOLON);
    return gates;
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
    // Accept all four flavours: always, always_ff, always_comb, always_latch.
    // The sensitivity list is only legal/required for `always` and `always_ff`;
    // `always_comb` and `always_latch` have implicit @* sensitivity.
    bool implicit_sens = check(TokenType::ALWAYS_COMB) || check(TokenType::ALWAYS_LATCH);
    consume();
    std::string sensitivity = implicit_sens ? "*" : parseSensitivityList();
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
    // Procedural loops are not represented in the structural AST yet but we
    // must consume them or we corrupt the token stream for the rest of the
    // module. Skip the header and recurse on the body so nested assignments
    // do not leak.
    if (check(TokenType::FOR) || check(TokenType::WHILE) || check(TokenType::REPEAT)) {
        consume();
        skipParens();
        return parseStatement();
    }
    if (check(TokenType::FOREVER)) {
        consume();
        return parseStatement();
    }
    // Edge / delay control inside a statement (e.g. `@(posedge clk) q <= d;` or
    // `#5 a = b;`) — skip the control and parse the inner statement.
    if (check(TokenType::AT)) {
        parseSensitivityList();
        return parseStatement();
    }
    if (check(TokenType::HASH)) {
        consume();
        if (check(TokenType::LPAREN)) skipParens();
        else                          consume();
        return parseStatement();
    }
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

// Stringify an expression for the unknown-width raw form. Keeps the original
// shape readable in JSON without committing to a typed AST for widths.
static std::string exprText(const ExprPtr& e) {
    if (!e) return "";
    switch (e->kind) {
        case ExprKind::IDENTIFIER:
            return static_cast<const IdentifierExpr*>(e.get())->name;
        case ExprKind::LITERAL:
            return static_cast<const LiteralExpr*>(e.get())->value;
        case ExprKind::UNARY_OP: {
            auto* n = static_cast<const UnaryOpExpr*>(e.get());
            return n->op + exprText(n->operand);
        }
        case ExprKind::BINARY_OP: {
            auto* n = static_cast<const BinaryOpExpr*>(e.get());
            return exprText(n->lhs) + n->op + exprText(n->rhs);
        }
        default: return "?";
    }
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
    // Parametric/complex range (e.g. [N-1:0], [$clog2(N)-1:0]). Preserve raw
    // text and flag as unknown so the validator does not compare widths.
    std::string raw = exprText(msb_expr) + ":" + exprText(lsb_expr);
    return PortWidth::Unknown(raw);
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

// Encode a PortWidth uniformly: integer ranges expose msb/lsb/scalar; parametric
// ranges expose unknown=true plus the raw expression text.
static json widthToJson(const PortWidth& w) {
    json j = {
        {"msb",     w.msb},
        {"lsb",     w.lsb},
        {"scalar",  w.scalar}
    };
    if (w.unknown) {
        j["unknown"] = true;
        j["expr"]    = w.expr;
    }
    return j;
}

static json moduleToJson(const Module& mod) {
    json ports = json::array();
    for (const auto& p : mod.ports) {
        json pj = {
            {"name",      p.name},
            {"direction", portDirectionStr(p.direction)},
            {"type",      portTypeStr(p.type)}
        };
        pj.update(widthToJson(p.width));
        ports.push_back(std::move(pj));
    }

    json parameters = json::array();
    for (const auto& p : mod.parameters)
        parameters.push_back({ {"name", p.name}, {"default", p.default_value} });

    json net_decls = json::array();
    for (const auto& n : mod.net_decls) {
        json nd = {
            {"name", n.name},
            {"type", netTypeStr(n.net_type)}
        };
        nd.update(widthToJson(n.width));
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
            {"wildcard",    inst.wildcard},
            {"parameters",  inst.parameters},
            {"connections", connections}
        });
    }

    json gate_primitives = json::array();
    for (const auto& g : mod.gate_primitives)
        gate_primitives.push_back({
            {"type",     g.gate_type},
            {"instance", g.instance_name},
            {"ports",    g.ports}
        });

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
        {"name",             mod.name},
        {"source_file",      mod.source_file},
        {"parameters",       parameters},
        {"ports",            ports},
        {"net_decls",        net_decls},
        {"instances",        instances},
        {"gate_primitives",  gate_primitives},
        {"assigns",          assigns},
        {"always_blocks",    always_blocks}
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
