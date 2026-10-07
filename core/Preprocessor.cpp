#include "Preprocessor.h"
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace {

struct Macro {
    bool                     has_params = false;
    std::vector<std::string> params;
    std::vector<Token>       body;
};

// Directives whose arguments live on the same line and are not part of the model.
const std::unordered_set<std::string> LINE_DIRECTIVES = {
    "`include", "`timescale", "`default_nettype", "`resetall", "`celldefine",
    "`endcelldefine", "`line", "`pragma", "`unconnected_drive", "`nounconnected_drive",
    "`begin_keywords", "`end_keywords", "`undefineall",
};

bool isEof(const Token& t) { return t.type == TokenType::END_OF_FILE; }

// Tokens up to the end of the current line. A trailing `\` continues onto the next line.
std::vector<Token> restOfLine(const std::vector<Token>& in, size_t& i) {
    std::vector<Token> out;
    int line = i < in.size() ? in[i].line : 0;
    while (i < in.size() && !isEof(in[i]) && in[i].line == line) {
        if (in[i].type == TokenType::UNKNOWN && in[i].lexeme == "\\") { line = in[i].line + 1; i++; continue; }
        out.push_back(in[i++]);
    }
    return out;
}

// Balanced argument list starting at LPAREN; splits on top-level commas. Advances i past ')'.
std::vector<std::vector<Token>> readArgs(const std::vector<Token>& in, size_t& i) {
    std::vector<std::vector<Token>> args(1);
    int depth = 0;
    for (; i < in.size() && !isEof(in[i]); i++) {
        const Token& t = in[i];
        if (t.type == TokenType::LPAREN || t.type == TokenType::LBRACKET || t.type == TokenType::LBRACE) {
            if (depth++ == 0) continue; // opening paren of the call
        } else if (t.type == TokenType::RPAREN || t.type == TokenType::RBRACKET || t.type == TokenType::RBRACE) {
            if (--depth == 0) { i++; break; }
        } else if (t.type == TokenType::COMMA && depth == 1) {
            args.emplace_back();
            continue;
        }
        args.back().push_back(t);
    }
    if (args.size() == 1 && args[0].empty()) args.clear();
    return args;
}

} // namespace

std::vector<Token> preprocess(const std::vector<Token>& input) {
    std::vector<Token> in = input;
    std::vector<Token> out;
    std::unordered_map<std::string, Macro> macros;
    struct Cond { bool parent_active; bool taken; bool active; };
    std::vector<Cond> conds;
    auto active = [&] { return conds.empty() || conds.back().active; };
    int expansions = 0; // guard against self-referential macros

    size_t i = 0;
    while (i < in.size()) {
        const Token t = in[i];
        if (t.type != TokenType::COMPILER_DIRECTIVE) {
            if (active() || isEof(t)) out.push_back(t);
            i++;
            continue;
        }
        const std::string& d = t.lexeme;
        if (d == "`ifdef" || d == "`ifndef") {
            i++;
            bool defined = i < in.size() && macros.count(in[i].lexeme);
            if (i < in.size()) i++;
            bool take = (d == "`ifdef") == defined;
            conds.push_back({active(), take, active() && take});
            continue;
        }
        if (d == "`elsif") {
            i++;
            bool defined = i < in.size() && macros.count(in[i].lexeme);
            if (i < in.size()) i++;
            if (!conds.empty()) {
                Cond& c = conds.back();
                c.active = c.parent_active && !c.taken && defined;
                c.taken = c.taken || defined;
            }
            continue;
        }
        if (d == "`else") {
            i++;
            if (!conds.empty()) {
                Cond& c = conds.back();
                c.active = c.parent_active && !c.taken;
                c.taken = true;
            }
            continue;
        }
        if (d == "`endif") { i++; if (!conds.empty()) conds.pop_back(); continue; }
        if (!active()) { i++; continue; }

        if (d == "`define") {
            i++;
            if (i >= in.size() || isEof(in[i])) continue;
            Token name = in[i++];
            Macro m;
            // Parameters only when '(' touches the name: `define ADD(a,b) vs `define X (a)
            if (i < in.size() && in[i].type == TokenType::LPAREN && in[i].line == name.line &&
                in[i].column == name.column + (int)name.lexeme.size()) {
                m.has_params = true;
                i++;
                while (i < in.size() && !isEof(in[i]) && in[i].type != TokenType::RPAREN) {
                    if (in[i].type != TokenType::COMMA) m.params.push_back(in[i].lexeme);
                    i++;
                }
                if (i < in.size()) i++; // )
            }
            if (i < in.size() && in[i].line == name.line) m.body = restOfLine(in, i);
            macros[name.lexeme] = std::move(m);
            continue;
        }
        if (d == "`undef") {
            i++;
            if (i < in.size() && !isEof(in[i])) macros.erase(in[i++].lexeme);
            continue;
        }
        if (LINE_DIRECTIVES.count(d)) { restOfLine(in, i); continue; }

        auto it = macros.find(d.substr(1));
        if (it == macros.end() || ++expansions > 100000) { out.push_back(t); i++; continue; }

        // Expand in place so the body is rescanned for nested macros.
        const Macro& m = it->second;
        size_t start = i++;
        std::vector<std::vector<Token>> args;
        if (m.has_params && i < in.size() && in[i].type == TokenType::LPAREN) args = readArgs(in, i);
        std::vector<Token> body;
        for (const Token& b : m.body) {
            size_t pi = 0;
            for (; pi < m.params.size(); pi++) if (b.lexeme == m.params[pi]) break;
            if (m.has_params && pi < m.params.size() && pi < args.size()) {
                for (Token a : args[pi]) { a.line = t.line; a.column = t.column; body.push_back(a); }
            } else {
                Token c = b; c.line = t.line; c.column = t.column; body.push_back(c);
            }
        }
        in.erase(in.begin() + start, in.begin() + i);
        in.insert(in.begin() + start, body.begin(), body.end());
        i = start;
    }
    if (out.empty() || !isEof(out.back())) out.push_back({TokenType::END_OF_FILE, "", 0, 0});
    return out;
}
