#include "Parser.h"
#include "Preprocessor.h"
#include <algorithm>
#include <unordered_map>

using json = nlohmann::json;

Parser::Parser(const std::vector<Token>& tokens)
    : tokens_(preprocess(tokens)), pos_(0) {}

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
        if (check(TokenType::MODULE)) {
            modules.push_back(parseModule());
            for (auto& n : nested_) modules.push_back(std::move(n));
            nested_.clear();
        } else {
            consume(); // packages, interfaces, classes, binds, ...: not modelled
        }
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

bool Parser::checkWord(const std::string& w) const {
    return current().type == TokenType::IDENTIFIER && current().lexeme == w;
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

void Parser::note(ValidationError::Severity sev, const std::string& msg, int line, int col) {
    if (cur_mod_) cur_mod_->notes.push_back({sev, cur_mod_->name, msg, line, col});
}

// error recovery

static bool isBlockEnd(TokenType t) {
    return t == TokenType::END || t == TokenType::ENDCASE || t == TokenType::ENDGENERATE ||
           t == TokenType::ENDMODULE || t == TokenType::END_OF_FILE;
}

// After a ParseError: resynchronise past the next ';'. A block or module end is
// left in place for the enclosing loop; callers guarantee progress themselves.
void Parser::recover() {
    while (!check(TokenType::SEMICOLON) && !isBlockEnd(current().type)) consume();
    match(TokenType::SEMICOLON);
}

void Parser::skipToSemicolon() {
    // Stop at SEMICOLON (the normal terminator) or at block/module boundaries so
    // a malformed statement cannot swallow the rest of its block.
    while (!check(TokenType::SEMICOLON) && !isBlockEnd(current().type)) consume();
}

void Parser::skipBlock() {
    // tracks BEGIN/END nesting — call when current token is BEGIN
    int depth = 0;
    do {
        if (check(TokenType::BEGIN))    depth++;
        else if (check(TokenType::END)) depth--;
        consume();
    } while (depth > 0 && !check(TokenType::END_OF_FILE));
    skipEndLabel();
}

// Raw text of a value up to an unnested `,` or closing bracket (and `;` when
// asked), crossing balanced (), [] and {} so a concatenation such as
// `.MASK({32'h1, 32'h2})` or a range in a default stays in one piece.
std::string Parser::scanBalanced(bool stopAtSemicolon) {
    std::string text;
    int depth = 0;
    while (!check(TokenType::END_OF_FILE)) {
        TokenType t = current().type;
        if (t == TokenType::LPAREN || t == TokenType::LBRACKET || t == TokenType::LBRACE) {
            depth++;
        } else if (t == TokenType::RPAREN || t == TokenType::RBRACKET || t == TokenType::RBRACE) {
            if (depth == 0) break;
            depth--;
        } else if (depth == 0 && (t == TokenType::COMMA || (stopAtSemicolon && t == TokenType::SEMICOLON))) {
            break;
        }
        text += consume().lexeme;
    }
    return text;
}

void Parser::skipUntil(TokenType end) {
    while (!check(end) && !check(TokenType::END_OF_FILE)) consume();
    match(end);
}

void Parser::skipUntilWord(const std::string& w) {
    while (!checkWord(w) && !check(TokenType::END_OF_FILE)) consume();
    if (checkWord(w)) consume();
    skipEndLabel();
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

// #5, #(1,2,3), #(a:b:c)
void Parser::skipDelay() {
    if (!match(TokenType::HASH)) return;
    if (check(TokenType::LPAREN)) skipParens();
    else                          consume();
}

// (strong1, weak0) — only when the token after '(' is a strength keyword, so a
// gate's port list is never mistaken for a strength.
void Parser::skipStrength() {
    if (!check(TokenType::LPAREN)) return;
    TokenType next = peek(1).type;
    if (next == TokenType::STRONG0 || next == TokenType::STRONG1 ||
        next == TokenType::WEAK0   || next == TokenType::WEAK1   ||
        next == TokenType::HIGHZ0  || next == TokenType::HIGHZ1  ||
        next == TokenType::PULL0   || next == TokenType::PULL1)
        skipParens();
}

// (* keep = "true" *) — the lexer emits LPAREN STAR ... STAR RPAREN.
void Parser::skipAttributes() {
    while (check(TokenType::LPAREN) && peek(1).type == TokenType::STAR) {
        consume(); consume();
        while (!check(TokenType::END_OF_FILE) &&
               !(check(TokenType::STAR) && peek(1).type == TokenType::RPAREN))
            consume();
        match(TokenType::STAR);
        match(TokenType::RPAREN);
    }
}

// `: name` after begin / end / endfunction / endmodule ...
void Parser::skipEndLabel() {
    if (check(TokenType::COLON) && peek(1).type == TokenType::IDENTIFIER) { consume(); consume(); }
}

// unpacked dimensions after a declarator: logic [31:0] mem [0:N-1]; logic a [4];
void Parser::skipUnpackedDims() {
    while (check(TokenType::LBRACKET)) {
        consume();
        scanBalanced(/*stopAtSemicolon=*/false);
        match(TokenType::RBRACKET);
    }
}

// Skip one procedural statement (used for initial/final bodies that we don't
// model). Honours begin/end nesting and falls back to skipping to ';'.
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
    Module* saved_mod = cur_mod_;
    auto    saved_dp  = std::move(defparams_);
    cur_mod_ = &mod;
    defparams_.clear();

    try {
        parseModuleHeader(mod);
    } catch (const ParseError& e) {
        note(ValidationError::Severity::ERROR, "module header: " + e.msg, e.line, e.column);
        skipToSemicolon();
        match(TokenType::SEMICOLON);
    }

    while (!check(TokenType::ENDMODULE) && !check(TokenType::END_OF_FILE)) {
        size_t before = pos_;
        try {
            parseModuleBodyItem(mod);
        } catch (const ParseError& e) {
            note(ValidationError::Severity::ERROR, "dropped: " + e.msg, e.line, e.column);
            recover();
        }
        if (pos_ == before) consume(); // never loop without progress
    }

    for (const auto& dp : defparams_) {
        auto it = std::find_if(mod.instances.begin(), mod.instances.end(),
            [&](const Instance& i){ return i.instance_name == dp.inst; });
        if (it == mod.instances.end()) {
            note(ValidationError::Severity::WARNING,
                 "defparam: instance '" + dp.inst + "' not found in this module", dp.line, dp.column);
            continue;
        }
        auto po = std::find_if(it->parameters.begin(), it->parameters.end(),
            [&](const ParamOverride& p){ return p.name == dp.name; });
        if (po != it->parameters.end()) po->value = dp.value;
        else                            it->parameters.push_back({dp.name, dp.value});
    }

    mod.end_line = current().line;
    match(TokenType::ENDMODULE);
    skipEndLabel();
    cur_mod_   = saved_mod;
    defparams_ = std::move(saved_dp);
    return mod;
}

void attachPragmas(std::vector<Module>& modules,
                   const std::vector<std::pair<int, std::string>>& pragmas) {
    for (const auto& [pline, word] : pragmas) {
        Module* target = nullptr;
        for (auto& m : modules) {
            if (m.line <= pline && pline <= m.end_line) { target = &m; break; }
            if (m.line > pline && (!target || m.line < target->line)) target = &m;
        }
        if (target) target->pragmas.push_back(word);
    }
}

// Single dispatch for a module body item. Shared with the generate handling so
// instances/assigns/always blocks inside generate constructs end up in the
// module's vectors rather than being silently dropped.
void Parser::parseModuleBodyItem(Module& mod) {
    skipAttributes();
    const Token& tok = current();
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
        parseNetDeclaration(mod);
        return;
    }
    if (check(TokenType::INTEGER) || check(TokenType::ENUM) ||
        check(TokenType::STRUCT)  || check(TokenType::UNION)) {
        parseTypedNetDeclaration(mod);
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
    if (check(TokenType::INITIAL) || checkWord("final")) {
        note(ValidationError::Severity::WARNING, "skipped: " + tok.lexeme + " block (not structural)", tok.line, tok.column);
        consume();
        skipStatement();
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
    if (check(TokenType::FOR)  || check(TokenType::IF)    || check(TokenType::CASE) ||
        check(TokenType::CASEX) || check(TokenType::CASEZ) || check(TokenType::BEGIN)) {
        parseGenerateItem(mod); // generate construct without the `generate` keyword
        return;
    }
    if (check(TokenType::GENVAR)) {
        skipToSemicolon();
        match(TokenType::SEMICOLON);
        return;
    }
    if (check(TokenType::FUNCTION) || check(TokenType::TASK)) {
        bool fn = check(TokenType::FUNCTION);
        consume();
        // name: the IDENTIFIER right before '(' or ';'
        std::string name;
        for (size_t k = 0; peek(k).type != TokenType::END_OF_FILE; k++) {
            if (peek(k).type == TokenType::LPAREN || peek(k).type == TokenType::SEMICOLON) break;
            if (peek(k).type == TokenType::IDENTIFIER) name = peek(k).lexeme;
        }
        note(ValidationError::Severity::WARNING,
             std::string("skipped: ") + (fn ? "function" : "task") + " '" + name + "' body (calls are shown as a cell)",
             tok.line, tok.column);
        skipUntil(fn ? TokenType::ENDFUNCTION : TokenType::ENDTASK);
        skipEndLabel();
        return;
    }
    if (check(TokenType::SPECIFY)) {
        note(ValidationError::Severity::WARNING, "skipped: specify block", tok.line, tok.column);
        consume();
        skipUntil(TokenType::ENDSPECIFY);
        return;
    }
    if (check(TokenType::TYPEDEF)) {
        skipTypedef();
        return;
    }
    if (check(TokenType::DEFPARAM)) {
        parseDefparam(mod);
        return;
    }
    if (check(TokenType::REAL) || check(TokenType::TIME) || check(TokenType::REALTIME)) {
        note(ValidationError::Severity::WARNING, "skipped: " + tok.lexeme + " declaration (not structural)", tok.line, tok.column);
        skipToSemicolon();
        match(TokenType::SEMICOLON);
        return;
    }
    if (check(TokenType::DEFAULT)) {
        // default clocking cb @(posedge clk); endclocking  |  default disable iff (rst);
        consume();
        if (checkWord("clocking")) skipUntilWord("endclocking");
        else { skipToSemicolon(); match(TokenType::SEMICOLON); }
        return;
    }
    if (check(TokenType::MODULE)) {
        nested_.push_back(parseModule());
        return;
    }
    if (check(TokenType::COMPILER_DIRECTIVE)) {
        note(ValidationError::Severity::WARNING, "undefined macro '" + tok.lexeme + "' ignored", tok.line, tok.column);
        consume();
        return;
    }
    if (check(TokenType::SYSTEM_TASK)) {
        note(ValidationError::Severity::WARNING, "skipped: " + tok.lexeme + " call (not structural)", tok.line, tok.column);
        skipToSemicolon();
        match(TokenType::SEMICOLON);
        return;
    }
    if (check(TokenType::IDENTIFIER)) {
        const std::string w = tok.lexeme;
        if (peek(1).type == TokenType::COLON) { // labelled item: ap: assert property (...);
            consume(); consume();
            parseModuleBodyItem(mod);
            return;
        }
        // SystemVerilog items we do not model. Those ending in ';' are skipped to it.
        if (w == "assert" || w == "assume" || w == "cover" || w == "restrict" ||
            w == "let"    || w == "import" || w == "export" || w == "bind" ||
            w == "timeunit" || w == "timeprecision") {
            skipToSemicolon();
            match(TokenType::SEMICOLON);
            return;
        }
        if (w == "property")   { skipUntilWord("endproperty"); return; }
        if (w == "sequence")   { skipUntilWord("endsequence"); return; }
        if (w == "clocking")   { skipUntilWord("endclocking"); return; }
        if (w == "checker")    { skipUntilWord("endchecker");  return; }
        if (w == "covergroup") {
            note(ValidationError::Severity::WARNING, "skipped: covergroup", tok.line, tok.column);
            skipUntilWord("endgroup");
            return;
        }
        if (w == "event") {
            note(ValidationError::Severity::WARNING, "skipped: event declaration", tok.line, tok.column);
            skipToSemicolon();
            match(TokenType::SEMICOLON);
            return;
        }
        if (w == "var") { consume(); parseModuleBodyItem(mod); return; }

        // Instance vs typed declaration:
        //   known_module ...            instance
        //   name #(...) ...             instance
        //   name inst (                 instance
        //   name inst [range] (         instance array
        //   type name ; = [ ,           declaration of a typedef'd / SV built-in type
        //   pkg::type name              declaration
        bool is_instance = module_names_.count(w) || peek(1).type == TokenType::HASH;
        if (!is_instance && peek(1).type == TokenType::IDENTIFIER) {
            size_t k = pos_ + 2;
            while (k < tokens_.size() && tokens_[k].type == TokenType::LBRACKET) {
                int depth = 0;
                do {
                    if (tokens_[k].type == TokenType::LBRACKET) depth++;
                    else if (tokens_[k].type == TokenType::RBRACKET) depth--;
                    k++;
                } while (depth > 0 && k < tokens_.size());
            }
            is_instance = k < tokens_.size() && tokens_[k].type == TokenType::LPAREN;
        }
        if (is_instance) mod.instances.push_back(parseInstance());
        else             parseTypedNetDeclaration(mod);
        return;
    }
    // Anything else cannot start a module item.
    note(ValidationError::Severity::WARNING, "unexpected '" + tok.lexeme + "' ignored", tok.line, tok.column);
    consume();
}

// generate ... endgenerate
void Parser::parseGenerateBlock(Module& mod) {
    expect(TokenType::GENERATE);
    while (!check(TokenType::ENDGENERATE) && !check(TokenType::END_OF_FILE)) {
        size_t before = pos_;
        parseModuleBodyItem(mod);
        if (pos_ == before) consume();
    }
    match(TokenType::ENDGENERATE);
}

// `begin [: label] items end [: label]` or a single item.
void Parser::parseGenerateBody(Module& mod) {
    if (!check(TokenType::BEGIN)) { parseModuleBodyItem(mod); return; }
    consume();
    skipEndLabel();
    while (!check(TokenType::END) && !check(TokenType::END_OF_FILE)) {
        size_t before = pos_;
        parseModuleBodyItem(mod);
        if (pos_ == before) consume();
    }
    match(TokenType::END);
    skipEndLabel();
}

// for / if / case / begin at module or generate level. Headers are not
// elaborated: every branch's contents end up in the module (flattening).
void Parser::parseGenerateItem(Module& mod) {
    if (check(TokenType::FOR) || check(TokenType::WHILE)) {
        consume();
        skipParens();
        parseGenerateBody(mod);
        return;
    }
    if (check(TokenType::IF)) {
        consume();
        skipParens();
        parseGenerateBody(mod);
        if (match(TokenType::ELSE)) {
            if (check(TokenType::IF)) parseGenerateItem(mod);
            else                      parseGenerateBody(mod);
        }
        return;
    }
    if (check(TokenType::CASE) || check(TokenType::CASEX) || check(TokenType::CASEZ)) {
        consume();
        skipParens();
        while (!check(TokenType::ENDCASE) && !check(TokenType::END_OF_FILE)) {
            size_t before = pos_;
            if (check(TokenType::DEFAULT)) consume();
            else while (!check(TokenType::COLON) && !check(TokenType::ENDCASE) &&
                        !check(TokenType::END_OF_FILE)) consume();
            match(TokenType::COLON);
            if (!check(TokenType::ENDCASE)) parseGenerateBody(mod);
            if (pos_ == before) consume();
        }
        match(TokenType::ENDCASE);
        return;
    }
    // Bare begin ... end. A generate region when its items parse; otherwise it is
    // procedural code left behind by an unexpanded macro (`VC_TRACE_BEGIN style),
    // which is skipped as one unit so it does not cascade into dozens of notes.
    size_t start = pos_;
    size_t n_nets = mod.net_decls.size(), n_inst = mod.instances.size(), n_gates = mod.gate_primitives.size(),
           n_asg = mod.assigns.size(), n_alw = mod.always_blocks.size(), n_par = mod.parameters.size(),
           n_notes = mod.notes.size();
    try {
        parseGenerateBody(mod);
    } catch (const ParseError&) {
        mod.net_decls.erase(mod.net_decls.begin() + n_nets, mod.net_decls.end());
        mod.instances.erase(mod.instances.begin() + n_inst, mod.instances.end());
        mod.gate_primitives.erase(mod.gate_primitives.begin() + n_gates, mod.gate_primitives.end());
        mod.assigns.erase(mod.assigns.begin() + n_asg, mod.assigns.end());
        mod.always_blocks.erase(mod.always_blocks.begin() + n_alw, mod.always_blocks.end());
        mod.parameters.erase(mod.parameters.begin() + n_par, mod.parameters.end());
        mod.notes.erase(mod.notes.begin() + n_notes, mod.notes.end());
        pos_ = start;
        note(ValidationError::Severity::WARNING,
             "skipped: begin/end block at module level (procedural code, not structural)",
             current().line, current().column);
        skipBlock();
    }
}

// module header

void Parser::parseModuleHeader(Module& mod) {
    match(TokenType::AUTOMATIC);
    mod.name = expect(TokenType::IDENTIFIER).lexeme;
    while (checkWord("import")) { skipToSemicolon(); match(TokenType::SEMICOLON); }
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
            // `parameter` may be omitted after the first one: #(parameter A = 1, B = 2, int C = 3)
            for (auto& p : parseParameterDeclaration())
                mod.parameters.push_back(std::move(p));
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
        skipAttributes();

        int line = current().line, col = current().column;

        if (check(TokenType::INPUT) || check(TokenType::OUTPUT) || check(TokenType::INOUT)) {
            // New direction keyword — update context for this group.
            if      (check(TokenType::INPUT))  { curDir = PortDirection::INPUT;  consume(); }
            else if (check(TokenType::OUTPUT)) { curDir = PortDirection::OUTPUT; consume(); }
            else                               { curDir = PortDirection::INOUT;  consume(); }

            if (checkWord("var")) consume();
            curType = PortType::UNSPECIFIED;
            if      (check(TokenType::WIRE))  { curType = PortType::WIRE;  consume(); }
            else if (check(TokenType::REG))   { curType = PortType::REG;   consume(); }
            else if (check(TokenType::LOGIC)) { curType = PortType::LOGIC; consume(); }
            else if (check(TokenType::TRI) || check(TokenType::INTEGER)) consume();

            if (check(TokenType::SIGNED) || check(TokenType::UNSIGNED)) consume();

            // User-defined or SV built-in type:  IDENT name | IDENT [..] name | pkg::IDENT name
            if (check(TokenType::IDENTIFIER) && peek(1).type == TokenType::COLON_COLON &&
                peek(2).type == TokenType::IDENTIFIER) {
                consume(); consume(); consume();
            } else if (check(TokenType::IDENTIFIER) &&
                       (peek(1).type == TokenType::IDENTIFIER || peek(1).type == TokenType::LBRACKET)) {
                consume();
            }
            if (check(TokenType::SIGNED) || check(TokenType::UNSIGNED)) consume();

            curWidth = PortWidth();
            if (check(TokenType::LBRACKET)) curWidth = parsePackedDims();

            hasAnsiDir = true;

            if (check(TokenType::IDENTIFIER)) {
                std::string name = consume().lexeme;
                mod.ports.push_back(Port(curDir, curType, curWidth, name, line, col));
                skipUnpackedDims();
                if (match(TokenType::EQ)) parseExpression(); // port default value
            }
        } else if (check(TokenType::IDENTIFIER)) {
            // interface port: `bus_if.master m` or `bus_if m`
            if (peek(1).type == TokenType::DOT && peek(2).type == TokenType::IDENTIFIER &&
                peek(3).type == TokenType::IDENTIFIER) {
                std::string type = consume().lexeme; consume();
                type += "." + consume().lexeme;
                std::string name = consume().lexeme;
                mod.ports.push_back(Port(PortDirection::INOUT, PortType::UNSPECIFIED,
                                         PortWidth::Unknown(type), name, line, col));
                continue;
            }
            if (peek(1).type == TokenType::IDENTIFIER) {
                std::string type = consume().lexeme;
                std::string name = consume().lexeme;
                mod.ports.push_back(Port(PortDirection::INOUT, PortType::UNSPECIFIED,
                                         PortWidth::Unknown(type), name, line, col));
                skipUnpackedDims();
                continue;
            }
            std::string name = consume().lexeme;
            if (hasAnsiDir) {
                // Continuation of current direction group (e.g. "b, cin" after "input a").
                mod.ports.push_back(Port(curDir, curType, curWidth, name, line, col));
            } else {
                // Non-ANSI style: bare name, direction declared in module body.
                mod.ports.push_back(Port(PortDirection::INPUT, PortType::UNSPECIFIED,
                                         PortWidth(), name, line, col));
            }
            skipUnpackedDims();
            if (match(TokenType::EQ)) parseExpression();
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

    if (checkWord("var")) consume();
    PortType ptype = PortType::UNSPECIFIED;
    if      (check(TokenType::WIRE))  { ptype = PortType::WIRE;  consume(); }
    else if (check(TokenType::REG))   { ptype = PortType::REG;   consume(); }
    else if (check(TokenType::LOGIC)) { ptype = PortType::LOGIC; consume(); }
    else if (check(TokenType::TRI) || check(TokenType::INTEGER)) consume();

    if (check(TokenType::SIGNED) || check(TokenType::UNSIGNED)) consume();
    if (check(TokenType::IDENTIFIER) && peek(1).type == TokenType::COLON_COLON &&
        peek(2).type == TokenType::IDENTIFIER) {
        consume(); consume(); consume();
    } else if (check(TokenType::IDENTIFIER) &&
               (peek(1).type == TokenType::IDENTIFIER || peek(1).type == TokenType::LBRACKET)) {
        consume(); // typedef'd / built-in type name
    }

    PortWidth width;
    if (check(TokenType::LBRACKET)) width = parsePackedDims();

    // One direction/type/width can declare multiple comma-separated ports:
    //   output reg [3:0] q1, q2, q3;
    std::vector<Port> ports;
    do {
        int pline = current().line, pcol = current().column;
        std::string name = expect(TokenType::IDENTIFIER).lexeme;
        ports.push_back(Port(dir, ptype, width, name, pline, pcol));
        skipUnpackedDims();
        if (match(TokenType::EQ)) parseExpression(); // output reg q = 0;
    } while (match(TokenType::COMMA));

    expect(TokenType::SEMICOLON);
    return ports;
}

void Parser::parseNetDeclaration(Module& mod) {
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

    if (check(TokenType::LOGIC)) consume(); // wire logic a;
    if (check(TokenType::SIGNED) || check(TokenType::UNSIGNED)) consume();

    PortWidth width;
    if (check(TokenType::LBRACKET)) width = parsePackedDims();
    skipDelay(); // wire #2 w; wire [3:0] #(1,2) v;

    bool is_var = ntype == NetType::REG || ntype == NetType::LOGIC;
    do {
        int nline = current().line, ncol = current().column;
        std::string name = expect(TokenType::IDENTIFIER).lexeme;
        skipUnpackedDims();
        ExprPtr init = nullptr;
        if (match(TokenType::EQ)) init = parseExpression();
        if (init && !is_var) {
            // `wire b = a;` is a net declaration plus a continuous assignment.
            mod.assigns.push_back(Assign(std::make_unique<IdentifierExpr>(name), std::move(init), nline, ncol));
            init = nullptr;
        } else if (init) {
            note(ValidationError::Severity::WARNING,
                 "initial value of '" + name + "' is not a driver (variable initializer)", nline, ncol);
        }
        mod.net_decls.push_back(NetDecl(ntype, width, name, line, col, std::move(init)));
    } while (match(TokenType::COMMA));

    expect(TokenType::SEMICOLON);
}

static std::string widthText(const PortWidth& w) {
    if (w.unknown) return w.expr;
    if (w.scalar)  return "";
    return std::to_string(w.msb) + ":" + std::to_string(w.lsb);
}

// Total width of a packed array: [3:0][7:0] -> 32 bits.
static PortWidth combineWidth(const PortWidth& a, const PortWidth& b) {
    if (a.scalar) return b;
    if (!a.unknown && !b.unknown) return PortWidth(a.width() * b.width() - 1, 0);
    return PortWidth::Unknown(widthText(a) + "][" + widthText(b));
}

// Declarations whose type is not a net keyword:
//   int i; bit [3:0] c; byte b [4]; integer n; my_t x, y; p::t_t z;
//   enum logic [1:0] {A, B} st; struct packed { ... } pk;
void Parser::parseTypedNetDeclaration(Module& mod) {
    int line = current().line, col = current().column;
    static const std::unordered_map<std::string, int> BUILTIN = {
        {"bit", 1}, {"byte", 8}, {"shortint", 16}, {"int", 32}, {"longint", 64},
        {"integer", 32}, {"trireg", 1}, {"uwire", 1},
    };
    NetType   ntype = NetType::LOGIC;
    PortWidth width;
    if (check(TokenType::ENUM) || check(TokenType::STRUCT) || check(TokenType::UNION)) {
        std::string tname = consume().lexeme;
        while (!check(TokenType::LBRACE) && !check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) consume();
        if (check(TokenType::LBRACE)) {
            int depth = 0;
            do {
                if      (check(TokenType::LBRACE)) depth++;
                else if (check(TokenType::RBRACE)) depth--;
                consume();
            } while (depth > 0 && !check(TokenType::END_OF_FILE));
        }
        width = PortWidth::Unknown(tname);
    } else {
        std::string tname;
        if (check(TokenType::INTEGER)) tname = consume().lexeme;
        else                           tname = parseScopedName();
        auto b = BUILTIN.find(tname);
        if (b != BUILTIN.end()) {
            if (b->second > 1) width = PortWidth(b->second - 1, 0);
            if (tname == "trireg" || tname == "uwire") ntype = NetType::TRI;
            else if (tname != "bit")                   ntype = NetType::REG;
        } else {
            width = PortWidth::Unknown(tname);
        }
        if (check(TokenType::SIGNED) || check(TokenType::UNSIGNED)) consume();
    }
    if (check(TokenType::LBRACKET)) width = combineWidth(width, parsePackedDims());

    do {
        int nline = current().line, ncol = current().column;
        std::string name = expect(TokenType::IDENTIFIER).lexeme;
        skipUnpackedDims();
        ExprPtr init = nullptr;
        if (match(TokenType::EQ)) {
            init = parseExpression();
            note(ValidationError::Severity::WARNING,
                 "initial value of '" + name + "' is not a driver (variable initializer)", nline, ncol);
        }
        mod.net_decls.push_back(NetDecl(ntype, width, name, line, col, std::move(init)));
    } while (match(TokenType::COMMA));
    expect(TokenType::SEMICOLON);
}

std::vector<Parameter> Parser::parseParameterDeclaration() {
    if (check(TokenType::PARAMETER) || check(TokenType::LOCALPARAM)) consume();

    // Skip optional type prefix.  Verilog/SV allow several shapes between the
    // `parameter` keyword and the name:
    //   parameter signed NAME = ...
    //   parameter int unsigned NAME = ...  (SV built-in types lex as IDENTIFIER)
    //   parameter logic [WIDTH-1:0] NAME = ...
    //   parameter MY_TYPE_T NAME = ...     (user typedef)
    //   parameter type T = logic [7:0]
    // The actual name is the IDENTIFIER directly followed by `=`, `,`, `;`, or `)`.
    auto isTypeKeyword = [](TokenType t) {
        return t == TokenType::SIGNED   || t == TokenType::UNSIGNED ||
               t == TokenType::LOGIC    || t == TokenType::REG      ||
               t == TokenType::WIRE     || t == TokenType::INTEGER  ||
               t == TokenType::REAL     || t == TokenType::TIME     ||
               t == TokenType::REALTIME;
    };
    while (true) {
        if (isTypeKeyword(current().type)) { consume(); continue; }
        if (check(TokenType::IDENTIFIER) && peek(1).type == TokenType::COLON_COLON &&
            peek(2).type == TokenType::IDENTIFIER && peek(3).type == TokenType::IDENTIFIER) {
            consume(); consume(); consume();
            continue;
        }
        if (check(TokenType::IDENTIFIER) &&
            (peek(1).type == TokenType::IDENTIFIER || peek(1).type == TokenType::SIGNED ||
             peek(1).type == TokenType::UNSIGNED   || peek(1).type == TokenType::LBRACKET)) {
            consume();
            continue;
        }
        break;
    }
    // Optional packed range [msb:lsb] — possibly multiple dimensions.
    skipUnpackedDims();

    // One `parameter` keyword can introduce several comma-separated names:
    //   parameter A = 1, B = 2, C = 3;
    // But in a header parameter list  #(parameter A = 1, parameter B = 2)
    // the comma belongs to the outer list, not to us — so we only continue
    // when the comma is followed by a bare IDENTIFIER (not a typed declaration).
    std::vector<Parameter> params;
    while (true) {
        std::string name = expect(TokenType::IDENTIFIER).lexeme;
        skipUnpackedDims();
        std::string default_val;
        if (match(TokenType::EQ)) default_val = scanBalanced(/*stopAtSemicolon=*/true);
        params.emplace_back(name, default_val);

        if (!check(TokenType::COMMA))                       break;
        if (peek(1).type != TokenType::IDENTIFIER)          break;
        TokenType after = peek(2).type;
        if (after != TokenType::EQ && after != TokenType::COMMA &&
            after != TokenType::RPAREN && after != TokenType::SEMICOLON) break;
        consume(); // ,
    }

    match(TokenType::SEMICOLON);
    return params;
}

// defparam u.W = 4, u.D = 2;  — applied to the instance at endmodule.
void Parser::parseDefparam(Module&) {
    int line = current().line, col = current().column;
    expect(TokenType::DEFPARAM);
    do {
        std::string path = parseScopedName();
        expect(TokenType::EQ);
        std::string value = scanBalanced(/*stopAtSemicolon=*/true);
        size_t dot = path.rfind('.');
        if (dot == std::string::npos || path.find('.') != dot)
            note(ValidationError::Severity::WARNING,
                 "defparam '" + path + "' not applied (only inst.PARAM is supported)", line, col);
        else
            defparams_.push_back({path.substr(0, dot), path.substr(dot + 1), value, line, col});
    } while (match(TokenType::COMMA));
    expect(TokenType::SEMICOLON);
}

// Parse the signal of a port connection as an expression without consuming it:
// pos_ is always restored so the caller can still collect the raw text. Returns
// nullptr when the text is not a plain expression or the expression does not
// span the whole connection.
ExprPtr Parser::tryParseConnectionExpr(bool positional) {
    size_t start = pos_;
    ExprPtr expr;
    if (!check(TokenType::RPAREN) && !(positional && check(TokenType::COMMA))) {
        try {
            expr = parseExpression();
            bool at_end = check(TokenType::RPAREN) || (positional && check(TokenType::COMMA));
            if (!at_end) expr = nullptr;
        } catch (const std::exception&) {
            expr = nullptr;
        }
    }
    pos_ = start;
    return expr;
}

Instance Parser::parseInstance() {
    int line = current().line, col = current().column;
    std::string module_name = consume().lexeme;
    Instance inst(module_name, "", line, col);

    if (match(TokenType::HASH)) {
        expect(TokenType::LPAREN);
        while (!check(TokenType::RPAREN) && !check(TokenType::END_OF_FILE)) {
            ParamOverride po;
            // named form: .NAME( value )
            bool named = check(TokenType::DOT) && peek(1).type == TokenType::IDENTIFIER &&
                         peek(2).type == TokenType::LPAREN;
            if (named) { consume(); po.name = consume().lexeme; consume(); }
            po.value = scanBalanced(/*stopAtSemicolon=*/false);
            if (named) expect(TokenType::RPAREN);
            inst.parameters.push_back(po);
            if (!match(TokenType::COMMA)) break;
        }
        expect(TokenType::RPAREN);
    }

    inst.instance_name = expect(TokenType::IDENTIFIER).lexeme;
    if (match(TokenType::LBRACKET)) {           // instance array: c u[3:0] (...)
        inst.array = scanBalanced(/*stopAtSemicolon=*/false);
        expect(TokenType::RBRACKET);
    }

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
                if (!check(TokenType::LPAREN)) {
                    // `.clk` — implicit named connection to the net of the same name
                    inst.connections.push_back(PortConnection(port_name, port_name, pline, pcol,
                                                              std::make_unique<IdentifierExpr>(port_name)));
                } else {
                    consume(); // (
                    ExprPtr expr = tryParseConnectionExpr(/*positional=*/false);
                    std::string signal = scanBalanced(/*stopAtSemicolon=*/false);
                    expect(TokenType::RPAREN);
                    inst.connections.push_back(PortConnection(port_name, signal, pline, pcol, std::move(expr)));
                }
            }
        } else {
            ExprPtr expr = tryParseConnectionExpr(/*positional=*/true);
            std::string signal = scanBalanced(/*stopAtSemicolon=*/false);
            inst.connections.push_back(PortConnection("", signal, pline, pcol, std::move(expr)));
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

    skipStrength(); // (strong0, weak1)
    skipDelay();    // #5 or #(1,2,3)

    // Parse one or more gate instances: [name [range]] (port, port, ...)
    std::vector<GatePrimitive> gates;
    do {
        // Optional instance name — present when current token is IDENTIFIER
        // and next is LPAREN or a range (not another IDENTIFIER, which would be a net).
        std::string inst_name, array;
        if (check(TokenType::IDENTIFIER) &&
            (peek(1).type == TokenType::LPAREN || peek(1).type == TokenType::LBRACKET)) {
            inst_name = consume().lexeme;
            if (match(TokenType::LBRACKET)) {
                array = scanBalanced(/*stopAtSemicolon=*/false);
                expect(TokenType::RBRACKET);
            }
        }

        expect(TokenType::LPAREN);
        std::vector<std::string> ports;
        while (!check(TokenType::RPAREN) && !check(TokenType::END_OF_FILE)) {
            // Collect one port expression as raw text (handles bit-selects, etc.)
            std::string sig = scanBalanced(/*stopAtSemicolon=*/false);
            // trim leading/trailing whitespace
            auto s = sig.find_first_not_of(' ');
            auto e = sig.find_last_not_of(' ');
            if (s != std::string::npos) sig = sig.substr(s, e - s + 1);
            ports.push_back(sig);
            if (!match(TokenType::COMMA)) break;
        }
        expect(TokenType::RPAREN);
        gates.emplace_back(gate_type, inst_name, std::move(ports), line, col);
        gates.back().array = array;
    } while (match(TokenType::COMMA));

    expect(TokenType::SEMICOLON);
    return gates;
}

std::vector<Assign> Parser::parseContinuousAssign() {
    expect(TokenType::ASSIGN);
    skipStrength(); // assign (strong1, weak0) y = a;
    skipDelay();    // assign #1 y = a;
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
    skipAttributes();
    if (check(TokenType::IDENTIFIER)) {
        const std::string& w = current().lexeme;
        TokenType next = peek(1).type;
        if ((w == "unique" || w == "unique0" || w == "priority") &&
            (next == TokenType::CASE || next == TokenType::CASEX || next == TokenType::CASEZ || next == TokenType::IF)) {
            consume(); // qualifier
        } else if (w == "foreach") {
            consume();
            skipParens();
            return parseStatement();
        } else if (w == "do") {
            consume();
            StmtPtr body = parseStatement();
            if (match(TokenType::WHILE)) { skipParens(); match(TokenType::SEMICOLON); }
            return body;
        } else if (next == TokenType::COLON && peek(2).type != TokenType::COLON) {
            consume(); consume(); // statement label `lbl: begin ...`
            return parseStatement();
        }
    }
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
        skipDelay();
        return parseStatement();
    }
    if (check(TokenType::IDENTIFIER) || check(TokenType::LBRACE)) {
        size_t start = pos_;
        ExprPtr lhs = parseLValue();
        if (check(TokenType::EQ))      return parseBlockingAssign(std::move(lhs));
        if (check(TokenType::LESS_EQ)) return parseNonBlockingAssign(std::move(lhs));
        // Compound assignment (a += b, a <<= 1) and increment / decrement (a++):
        // modelled as a blocking assign whose rhs reads the target.
        TokenType t = current().type, n = peek(1).type;
        bool compound = n == TokenType::EQ &&
            (t == TokenType::PLUS || t == TokenType::MINUS || t == TokenType::STAR ||
             t == TokenType::SLASH || t == TokenType::PERCENT || t == TokenType::AMP ||
             t == TokenType::PIPE || t == TokenType::CARET || t == TokenType::LESS_LESS ||
             t == TokenType::GREATER_GREATER || t == TokenType::LESS_LESS_LESS ||
             t == TokenType::GREATER_GREATER_GREATER);
        bool incdec = (t == TokenType::PLUS && n == TokenType::PLUS) ||
                      (t == TokenType::MINUS && n == TokenType::MINUS);
        if (compound || incdec) {
            int line = current().line, col = current().column;
            pos_ = start;
            ExprPtr self = parseLValue(); // second copy of the target, read on the rhs
            std::string op = consume().lexeme;
            consume(); // '=' or the second '+' / '-'
            ExprPtr rhs = incdec ? std::make_unique<LiteralExpr>("1") : parseExpression();
            expect(TokenType::SEMICOLON);
            return std::make_unique<BlockingAssign>(
                std::move(lhs), std::make_unique<BinaryOpExpr>(op, std::move(self), std::move(rhs)), line, col);
        }
        // task call, void'(...), break, return, ...: not structural
    }
    if (isBlockEnd(current().type) || check(TokenType::ELSE) || check(TokenType::DEFAULT))
        throw ParseError("unexpected '" + current().lexeme + "'", current());
    skipToSemicolon();
    match(TokenType::SEMICOLON);
    return nullptr;
}

StmtPtr Parser::parseSeqBlock() {
    int line = current().line, col = current().column;
    expect(TokenType::BEGIN);
    skipEndLabel(); // begin : name
    auto block = std::make_unique<SeqBlock>(line, col);
    while (!check(TokenType::END) && !check(TokenType::END_OF_FILE)) {
        size_t before = pos_;
        try {
            StmtPtr stmt = parseStatement();
            if (stmt) block->body.push_back(std::move(stmt));
        } catch (const ParseError& e) {
            note(ValidationError::Severity::ERROR, "dropped: " + e.msg, e.line, e.column);
            recover();
        }
        if (pos_ == before) consume();
    }
    expect(TokenType::END);
    skipEndLabel(); // end : name
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
    if (checkWord("inside") || checkWord("matches")) consume();
    auto stmt = std::make_unique<CaseStatement>(variant, std::move(expr), line, col);
    while (!check(TokenType::ENDCASE) && !check(TokenType::END_OF_FILE)) {
        size_t before = pos_;
        try {
            auto item = parseCaseItem();
            if (item) stmt->items.push_back(std::move(*item));
        } catch (const ParseError& e) {
            note(ValidationError::Severity::ERROR, "dropped: " + e.msg, e.line, e.column);
            recover();
        }
        if (pos_ == before) consume();
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
            item->patterns.push_back(parseRangeOrExpr());
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
        case TokenType::CARET:     case TokenType::TILDE_CARET: return 5;
        case TokenType::AMP:                                    return 6;
        case TokenType::EQ_EQ:     case TokenType::BANG_EQ:
        case TokenType::EQ_EQ_EQ:  case TokenType::BANG_EQ_EQ:
        case TokenType::EQ_EQ_QUESTION: case TokenType::BANG_EQ_QUESTION: return 7;
        case TokenType::LESS:      case TokenType::LESS_EQ:
        case TokenType::GREATER:   case TokenType::GREATER_EQ:  return 8;
        case TokenType::LESS_LESS: case TokenType::GREATER_GREATER:
        case TokenType::LESS_LESS_LESS:
        case TokenType::GREATER_GREATER_GREATER:                return 9;
        case TokenType::PLUS:      case TokenType::MINUS:       return 10;
        case TokenType::STAR:      case TokenType::SLASH:
        case TokenType::PERCENT:                                return 11;
        case TokenType::STAR_STAR:                              return 12;
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
        if (prec < 0 && checkWord("inside")) prec = 7; // a inside {1, [4:7]}
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

// a, a.b, p::a — hierarchical and package-scoped names stay one identifier
std::string Parser::parseScopedName() {
    std::string name = expect(TokenType::IDENTIFIER).lexeme;
    while (true) {
        if (check(TokenType::COLON_COLON) && peek(1).type == TokenType::IDENTIFIER) {
            consume();
            name += "::" + consume().lexeme;
        } else if (check(TokenType::DOT) && peek(1).type == TokenType::IDENTIFIER) {
            consume();
            name += "." + consume().lexeme;
        } else {
            break;
        }
    }
    return name;
}

// Plain decimal literal, else -1.
static int litInt(const ExprPtr& e) {
    if (!e || e->kind != ExprKind::LITERAL) return -1;
    const std::string& v = static_cast<const LiteralExpr*>(e.get())->value;
    if (v.empty() || v.size() > 9) return -1;
    for (char c : v) if (!isdigit((unsigned char)c)) return -1;
    return std::stoi(v);
}

// Constant-fold literal arithmetic in a range expression ([`W-1:0] after macro
// expansion, [8*2-1:0]); -1 when anything is not a plain decimal literal.
static long foldInt(const ExprPtr& e) {
    if (!e) return -1;
    if (e->kind == ExprKind::LITERAL) return litInt(e);
    if (e->kind != ExprKind::BINARY_OP) return -1;
    auto* n = static_cast<const BinaryOpExpr*>(e.get());
    long a = foldInt(n->lhs), b = foldInt(n->rhs);
    if (a < 0 || b < 0) return -1;
    if (n->op == "+") return a + b;
    if (n->op == "-") return a - b;
    if (n->op == "*") return a * b;
    if (n->op == "/") return b ? a / b : -1;
    if (n->op == "<<") return b < 31 ? a << b : -1;
    if (n->op == ">>") return a >> b;
    return -1;
}

// Chained selects after a name: a[i], a[m:l], a[b+:w], a[b-:w], mem[i][j], a[i].f
ExprPtr Parser::parseSelects(ExprPtr base) {
    while (true) {
        if (check(TokenType::DOT) && peek(1).type == TokenType::IDENTIFIER) {
            consume(); consume(); // member of an array element: the array is the net
            continue;
        }
        if (!match(TokenType::LBRACKET)) break;
        ExprPtr idx = parseExpression();
        if (match(TokenType::COLON)) {
            ExprPtr lsb = parseExpression();
            expect(TokenType::RBRACKET);
            base = std::make_unique<PartSelectExpr>(std::move(base), std::move(idx), std::move(lsb));
        } else if (check(TokenType::PLUS_COLON) || check(TokenType::MINUS_COLON)) {
            std::string op = consume().lexeme;
            ExprPtr width = parseExpression();
            expect(TokenType::RBRACKET);
            int b = litInt(idx), n = litInt(width);
            if (b >= 0 && n > 0 && (op == "+:" || b - n + 1 >= 0)) {
                // constant indexed part-select folds to a plain one
                int msb = op == "+:" ? b + n - 1 : b;
                int lsb = op == "+:" ? b : b - n + 1;
                base = std::make_unique<PartSelectExpr>(std::move(base),
                    std::make_unique<LiteralExpr>(std::to_string(msb)),
                    std::make_unique<LiteralExpr>(std::to_string(lsb)));
            } else {
                // variable base: msb holds `idx +: width`, lsb is absent
                base = std::make_unique<PartSelectExpr>(std::move(base),
                    std::make_unique<BinaryOpExpr>(op, std::move(idx), std::move(width)), nullptr);
            }
        } else {
            expect(TokenType::RBRACKET);
            base = std::make_unique<BitSelectExpr>(std::move(base), std::move(idx));
        }
    }
    return base;
}

// [lo:hi] in `inside {...}` lists and `case ... inside` items, else an expression
ExprPtr Parser::parseRangeOrExpr() {
    if (!match(TokenType::LBRACKET)) return parseExpression();
    ExprPtr lo = parseExpression();
    expect(TokenType::COLON);
    ExprPtr hi = parseExpression();
    expect(TokenType::RBRACKET);
    return std::make_unique<BinaryOpExpr>(":", std::move(lo), std::move(hi));
}

ExprPtr Parser::parseAtom() {
    // parenthesised expression
    if (match(TokenType::LPAREN)) {
        ExprPtr inner = parseExpression();
        expect(TokenType::RPAREN);
        return parseSelects(std::move(inner));
    }

    // concatenation or replication: { ... }
    if (check(TokenType::LBRACE)) {
        consume();
        ExprPtr first = parseRangeOrExpr();
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
            concat->parts.push_back(parseRangeOrExpr());
        expect(TokenType::RBRACE);
        return concat;
    }

    // assignment pattern '{a, b} / '{default: 0} / '{f: 1}: kept as a concatenation
    if (check(TokenType::APOSTROPHE) && peek(1).type == TokenType::LBRACE) {
        consume(); consume();
        auto concat = std::make_unique<ConcatExpr>();
        while (!check(TokenType::RBRACE) && !check(TokenType::END_OF_FILE)) {
            if ((check(TokenType::IDENTIFIER) || check(TokenType::DEFAULT)) && peek(1).type == TokenType::COLON) {
                consume(); consume();
            }
            concat->parts.push_back(parseRangeOrExpr());
            if (!match(TokenType::COMMA)) break;
        }
        expect(TokenType::RBRACE);
        return concat;
    }

    // casts are transparent for connectivity: 8'(x), int'(x), signed'(x)
    auto castBody = [&]() -> ExprPtr {
        consume(); consume(); // ' (
        ExprPtr inner = parseExpression();
        expect(TokenType::RPAREN);
        return inner;
    };
    if ((check(TokenType::SIGNED) || check(TokenType::UNSIGNED)) &&
        peek(1).type == TokenType::APOSTROPHE && peek(2).type == TokenType::LPAREN) {
        consume();
        return castBody();
    }

    // literals
    if (check(TokenType::INTEGER_LITERAL) || check(TokenType::REAL_LITERAL) ||
        check(TokenType::STRING_LITERAL)  || check(TokenType::TIME_LITERAL)) {
        std::string value = consume().lexeme;
        if (check(TokenType::APOSTROPHE) && peek(1).type == TokenType::LPAREN) return castBody();
        return parseSelects(std::make_unique<LiteralExpr>(value));
    }

    // identifier: name, pkg::name, a.b, call(args), type'(cast), selects
    if (check(TokenType::IDENTIFIER)) {
        std::string name = parseScopedName();
        if (check(TokenType::APOSTROPHE) && peek(1).type == TokenType::LPAREN) return castBody();
        if (match(TokenType::LPAREN)) {
            auto args = std::make_unique<ConcatExpr>();
            if (!check(TokenType::RPAREN))
                args->parts.push_back(parseExpression());
            while (match(TokenType::COMMA))
                args->parts.push_back(parseExpression());
            expect(TokenType::RPAREN);
            return std::make_unique<BinaryOpExpr>("call", std::make_unique<IdentifierExpr>(name), std::move(args));
        }
        return parseSelects(std::make_unique<IdentifierExpr>(name));
    }

    // system function as expression: $clog2(N), $bits(a)
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

    if (isBlockEnd(current().type) || check(TokenType::SEMICOLON) || check(TokenType::RPAREN) ||
        check(TokenType::RBRACKET) || check(TokenType::RBRACE) || check(TokenType::COMMA))
        throw ParseError("expected an expression, got '" + current().lexeme + "'", current());
    return std::make_unique<LiteralExpr>(consume().lexeme); // fallback (undefined macro, ...)
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
    return parseSelects(std::make_unique<IdentifierExpr>(parseScopedName()));
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
            if (n->op == "call") {
                std::string args;
                for (const auto& p : static_cast<const ConcatExpr*>(n->rhs.get())->parts)
                    args += (args.empty() ? "" : ",") + exprText(p);
                return exprText(n->lhs) + "(" + args + ")";
            }
            return exprText(n->lhs) + n->op + exprText(n->rhs);
        }
        case ExprKind::CONDITIONAL: {
            auto* n = static_cast<const ConditionalExpr*>(e.get());
            return exprText(n->cond) + "?" + exprText(n->then_) + ":" + exprText(n->else_);
        }
        case ExprKind::BIT_SELECT: {
            auto* n = static_cast<const BitSelectExpr*>(e.get());
            return exprText(n->base) + "[" + exprText(n->index) + "]";
        }
        case ExprKind::PART_SELECT: {
            auto* n = static_cast<const PartSelectExpr*>(e.get());
            return exprText(n->base) + "[" + exprText(n->msb) + ":" + exprText(n->lsb) + "]";
        }
        default: return "?";
    }
}

PortWidth Parser::parseWidth() {
    expect(TokenType::LBRACKET);
    ExprPtr msb_expr = parseExpression();
    expect(TokenType::COLON);
    ExprPtr lsb_expr = parseExpression();
    expect(TokenType::RBRACKET);

    long msb = foldInt(msb_expr);
    long lsb = foldInt(lsb_expr);
    if (msb >= 0 && lsb >= 0 && msb < (1 << 20) && lsb < (1 << 20)) return PortWidth((int)msb, (int)lsb);
    // Parametric/complex range (e.g. [N-1:0], [$clog2(N)-1:0]). Preserve raw
    // text and flag as unknown so the validator does not compare widths.
    std::string raw = exprText(msb_expr) + ":" + exprText(lsb_expr);
    return PortWidth::Unknown(raw);
}

// [3:0][7:0] -> one PortWidth covering all packed dimensions
PortWidth Parser::parsePackedDims() {
    PortWidth w = parseWidth();
    while (check(TokenType::LBRACKET)) w = combineWidth(w, parseWidth());
    return w;
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
            {"line",      p.line},
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
            {"line", n.line},
            {"type", netTypeStr(n.net_type)}
        };
        nd.update(widthToJson(n.width));
        if (n.init) nd["init"] = exprToJson(n.init);
        net_decls.push_back(nd);
    }

    json instances = json::array();
    for (const auto& inst : mod.instances) {
        json params = json::array();
        for (const auto& po : inst.parameters)
            params.push_back({ {"name", po.name}, {"value", po.value} });
        json connections = json::array();
        for (const auto& c : inst.connections)
            connections.push_back({ {"port", c.port_name}, {"signal", c.signal},
                                    {"expr", exprToJson(c.expr)} });
        json ij = {
            {"module",      inst.module_name},
            {"instance",    inst.instance_name},
            {"line",        inst.line},
            {"resolved",    inst.resolved},
            {"wildcard",    inst.wildcard},
            {"parameters",  params},
            {"connections", connections}
        };
        if (!inst.array.empty()) ij["array"] = inst.array;
        instances.push_back(std::move(ij));
    }

    json gate_primitives = json::array();
    for (const auto& g : mod.gate_primitives) {
        json gj = {
            {"type",     g.gate_type},
            {"instance", g.instance_name},
            {"line",     g.line},
            {"ports",    g.ports}
        };
        if (!g.array.empty()) gj["array"] = g.array;
        gate_primitives.push_back(std::move(gj));
    }

    json assigns = json::array();
    for (const auto& a : mod.assigns)
        assigns.push_back({ {"line", a.line}, {"lhs", exprToJson(a.lhs)}, {"rhs", exprToJson(a.rhs)} });

    json always_blocks = json::array();
    for (const auto& ab : mod.always_blocks)
        always_blocks.push_back({
            {"line",        ab.line},
            {"sensitivity", ab.sensitivity},
            {"body",        stmtToJson(ab.body)}
        });

    json notes = json::array();
    for (const auto& n : mod.notes)
        notes.push_back({
            {"severity", n.severity == ValidationError::Severity::ERROR ? "error" : "warning"},
            {"line",     n.line},
            {"message",  n.message}
        });

    return {
        {"name",             mod.name},
        {"source_file",      mod.source_file},
        {"line",             mod.line},
        {"end_line",         mod.end_line},
        {"pragmas",          mod.pragmas},
        {"parameters",       parameters},
        {"ports",            ports},
        {"net_decls",        net_decls},
        {"instances",        instances},
        {"gate_primitives",  gate_primitives},
        {"assigns",          assigns},
        {"always_blocks",    always_blocks},
        {"notes",            notes}
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
