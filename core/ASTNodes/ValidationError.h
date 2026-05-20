#pragma once
#include <string>
#include <unordered_map>
#include <vector>

class Module;   // full def in Module.h
class Port;     // full def in Port.h
class NetDecl;  // full def in NetDecl.h

struct ValidationError {
    enum class Severity { WARNING, ERROR };
    Severity    severity;
    std::string module_name;  // which module the error is in
    std::string message;
    int         line, column;
};

// Passed down through every validate() call so errors accumulate in one place
// and all nodes share the same signal / symbol tables.
struct ValidationContext {
    using SymbolTable = std::unordered_map<std::string, const Module*>;

    const SymbolTable&                              symbols;    // project-level module defs
    std::string                                     module_name;
    std::unordered_map<std::string, const Port*>    port_map;  // current module's ports
    std::unordered_map<std::string, const NetDecl*> net_map;   // current module's nets
    std::vector<ValidationError>&                   errors;

    void error(const std::string& msg, int line, int col) {
        errors.push_back({ValidationError::Severity::ERROR, module_name, msg, line, col});
    }
    void warn(const std::string& msg, int line, int col) {
        errors.push_back({ValidationError::Severity::WARNING, module_name, msg, line, col});
    }

    // true if name is a declared port or net in the current module
    bool hasSignal(const std::string& name) const {
        return port_map.count(name) || net_map.count(name);
    }

};
