#pragma once
#include <string>
#include <vector>
#include "Node.h"
#include "Expression.h"

class PortConnection : public Node {
public:
    std::string port_name;   // named: .clk(sys_clk) -> "clk"; positional: empty
    std::string signal;      // connected signal or expression, raw text
    ExprPtr     expr;        // parsed form of `signal`; nullptr if it did not parse as an expression

    PortConnection(const std::string& port_name, const std::string& signal, int line, int column,
                   ExprPtr expr = nullptr)
        : Node(NodeKind::PORT_CONNECTION, line, column), port_name(port_name), signal(signal),
          expr(std::move(expr)) {}
};

// Parameter override in an instantiation: `.WIDTH(32)` -> {"WIDTH","32"};
// positional `#(32)` -> {"", "32"}. Values are raw text.
struct ParamOverride {
    std::string name;
    std::string value;
};

class Instance : public Node {
public:
    std::string                  module_name;
    std::string                  instance_name;
    std::vector<ParamOverride>   parameters;   // parameter overrides in source order
    std::vector<PortConnection>  connections;
    bool                         resolved = false; // set by Linker::link()
    bool                         wildcard = false; // true if `.*` wildcard was used
    std::string                  array;            // range text of an instance array: `c u[3:0] (...)` -> "3:0"

    Instance(const std::string& module_name, const std::string& instance_name, int line, int column)
        : Node(NodeKind::INSTANCE, line, column),
          module_name(module_name), instance_name(instance_name) {}

    // Judgment: an instance can only judge whether it resolved at link time.
    // Port-connection checking requires the target Module definition and is
    // handled one level up in Module::validate(), which has both in scope.
    void validate(ValidationContext& ctx) const override {
        if (!resolved)
            ctx.warn("instance '" + instance_name + "' of '" + module_name +
                     "' is unresolved (module not in project)", line, column);
    }
};
