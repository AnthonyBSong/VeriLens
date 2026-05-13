#pragma once
#include <string>
#include <vector>

struct PortConnection {
    std::string port_name;   // named: .clk(sys_clk) -> "clk"; positional: empty
    std::string signal;      // connected signal or expression, raw text

    PortConnection(const std::string& port_name, const std::string& signal)
        : port_name(port_name), signal(signal) {}
};

struct Instance {
    std::string                  module_name;
    std::string                  instance_name;
    std::vector<std::string>     parameters;    // raw parameter overrides e.g. ".WIDTH(8)"
    std::vector<PortConnection>  connections;

    Instance(const std::string& module_name, const std::string& instance_name)
        : module_name(module_name), instance_name(instance_name) {}
};
