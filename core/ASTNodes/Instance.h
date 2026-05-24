#pragma once
#include <string>
#include <vector>
#include "Node.h"

class PortConnection : public Node {
public:
    std::string port_name;   // named: .clk(sys_clk) -> "clk"; positional: empty
    std::string signal;      // connected signal or expression, raw text

    PortConnection(const std::string& port_name, const std::string& signal, int line, int column)
        : Node(NodeKind::PORT_CONNECTION, line, column), port_name(port_name), signal(signal) {}
};

class Instance : public Node {
public:
    std::string                  module_name;
    std::string                  instance_name;
    std::vector<std::string>     parameters;   // raw parameter overrides e.g. "NUM_ROWS"
    std::vector<PortConnection>  connections;
    bool                         resolved = false; // set by Linker::link()
    bool                         wildcard = false; // true if `.*` wildcard was used

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
