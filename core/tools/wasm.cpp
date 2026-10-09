// Browser entry point: the same pipeline as gen_ast, over in-memory sources.
//   parse('[{"name":"a.sv","text":"module a; endmodule"}]') -> '{"ast":[...],"diagnostics":"..."}'
#include "Lexer.h"
#include "Parser.h"
#include "Linker.h"
#include "Validator.h"
#include <emscripten/bind.h>
#include <nlohmann/json.hpp>
#include <sstream>

static std::string parse(std::string files_json) {
    auto files = nlohmann::json::parse(files_json);
    Linker linker;
    std::ostringstream diag;
    for (const auto& f : files) {
        std::string name = f.at("name"), src = f.at("text");
        try {
            Lexer lexer(src);
            Parser parser(lexer.tokenize());
            auto mods = parser.parse();
            attachPragmas(mods, lexer.pragmas());
            linker.addModules(std::move(mods), name);
        } catch (const std::exception& e) {
            diag << "Error parsing " << name << ": " << e.what() << "\n";
        }
    }
    linker.link();
    for (const auto& mod : linker.modules())
        for (const auto& n : mod.notes) diag << Validator::format(n) << "\n";
    for (const auto& d : Validator(linker).run()) diag << Validator::format(d) << "\n";
    nlohmann::json out = { {"ast", nlohmann::json::parse(modulesToJSON(linker.modules()))}, {"diagnostics", diag.str()} };
    return out.dump();
}

EMSCRIPTEN_BINDINGS(verilens) { emscripten::function("parse", &parse); }
