#pragma once
#include <string>
#include <vector>
#include "Node.h"

struct PortConnection : Node {
    std::string port_name;   // named: .clk(sys_clk) -> "clk"; positional: empty
    std::string signal;      // connected signal or expression, raw text

    PortConnection(const std::string& port_name, const std::string& signal, int line, int column)
        : Node(NodeKind::PORT_CONNECTION, line, column), port_name(port_name), signal(signal) {}
};

struct Instance : Node {
    std::string                  module_name;
    std::string                  instance_name;
    std::vector<std::string>     parameters;    // raw parameter overrides e.g. ".WIDTH(8)"
    std::vector<PortConnection>  connections;
    bool                         resolved = false; // set by Linker::link() if module_name found in project

    Instance(const std::string& module_name, const std::string& instance_name, int line, int column)
        : Node(NodeKind::INSTANCE, line, column), module_name(module_name), instance_name(instance_name) {}
};
