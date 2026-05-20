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

private:
    std::vector<Module>                           modules_;
    std::unordered_map<std::string, std::size_t>  symbol_table_; // name -> index into modules_
};
