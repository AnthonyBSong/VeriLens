#include "Linker.h"

void Linker::addModules(std::vector<Module> modules, const std::string& source_file) {
    for (auto& mod : modules) {
        mod.source_file = source_file;
        auto it = symbol_table_.find(mod.name);
        if (it != symbol_table_.end()) {
            // Duplicate module definition. Keep the first one as canonical so
            // instance resolution is deterministic, but record a warning that
            // names a both source files so users can find and reconcile them.
            const Module& prev = modules_[it->second];
            diagnostics_.push_back({
                ValidationError::Severity::WARNING,
                mod.name,
                "duplicate module definition: also defined in '" + prev.source_file + "'",
                mod.line, mod.column
            });
        } else {
            symbol_table_[mod.name] = modules_.size();
        }
        modules_.push_back(std::move(mod));
    }
}

void Linker::link() {
    for (auto& mod : modules_)
        for (auto& inst : mod.instances)
            inst.resolved = symbol_table_.count(inst.module_name) > 0;

    // Build pointer-based symbol table once, after all push_backs are done
    // and modules_ will no longer reallocate. Use the canonical (first) def
    // when duplicates exist.
    symbol_table_ptr_.clear();
    for (const auto& [name, idx] : symbol_table_)
        symbol_table_ptr_[name] = &modules_[idx];
}

