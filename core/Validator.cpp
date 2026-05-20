#include "Validator.h"
#include <string>

Validator::Validator(const Linker& linker) : linker_(linker) {}

std::vector<ValidationError> Validator::run() const {
    // Build a pointer-based symbol table from the linker's resolved module list.
    ValidationContext::SymbolTable symbols;
    for (const auto& mod : linker_.modules())
        symbols[mod.name] = &mod;

    // Each Module::validate() recursively applies the typing judgments defined
    // inline on every node in its subtree and appends to the shared error list.
    std::vector<ValidationError> all;
    for (const auto& mod : linker_.modules()) {
        auto errs = mod.validate(symbols);
        all.insert(all.end(), errs.begin(), errs.end());
    }
    return all;
}

std::string Validator::format(const ValidationError& e) {
    const char* sev = (e.severity == ValidationError::Severity::ERROR) ? "error" : "warning";
    std::string loc;
    if (e.line || e.column)
        loc = " at " + std::to_string(e.line) + ":" + std::to_string(e.column);
    return "[" + std::string(sev) + "] " + e.module_name + loc + ": " + e.message;
}
