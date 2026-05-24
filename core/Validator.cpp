#include "Validator.h"
#include <string>

Validator::Validator(const Linker& linker) : linker_(linker) {}

std::vector<ValidationError> Validator::run() const {
    const auto& symbols = linker_.symbolTable();
    std::vector<ValidationError> all = linker_.diagnostics();
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
