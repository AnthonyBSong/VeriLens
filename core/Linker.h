#pragma once
#include "ASTNodes/ASTNodes.h"
#include <string>
#include <unordered_map>
#include <vector>

// Project-level symbol table and link pass.

class Linker {
public:
    // Add modules parsed from one source file. Sets source_file on each module.
    void addModules(std::vector<Module> modules, const std::string& source_file);

    // Walk all instances; set resolved=true if module_name is in the symbol table.
    void link();

    const std::vector<Module>& modules() const { return modules_; }

    // Returns nullptr if not found.
    const Module* lookup(const std::string& module_name) const;

    // The pointer-based symbol table ready for ValidationContext.
    const ValidationContext::SymbolTable& symbolTable() const { return symbol_table_ptr_; }

    // Diagnostics collected during linking (e.g. duplicate module definitions).
    const std::vector<ValidationError>& diagnostics() const { return diagnostics_; }

private:
    std::vector<Module>                          modules_;
    std::unordered_map<std::string, std::size_t> symbol_table_;     // name -> index of canonical def
    ValidationContext::SymbolTable               symbol_table_ptr_; // name -> Module* (used by Validator)
    std::vector<ValidationError>                 diagnostics_;
};
