#include "Linker.h"

void Linker::addModules(std::vector<Module> modules, const std::string& source_file) {
    for (auto& mod : modules) {
        mod.source_file = source_file;
        symbol_table_[mod.name] = modules_.size();
        modules_.push_back(std::move(mod));
    }
}

void Linker::link() {
    for (auto& mod : modules_)
        for (auto& inst : mod.instances)
            inst.resolved = symbol_table_.count(inst.module_name) > 0;

    // Build pointer-based symbol table once, after all push_backs are done
    // and modules_ will no longer reallocate.
    symbol_table_ptr_.clear();
    for (const auto& mod : modules_)
        symbol_table_ptr_[mod.name] = &mod;
}

const Module* Linker::lookup(const std::string& module_name) const {
    auto it = symbol_table_.find(module_name);
    if (it == symbol_table_.end()) return nullptr;
    return &modules_[it->second];
}
