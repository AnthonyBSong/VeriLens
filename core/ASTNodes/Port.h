#pragma once
#include <string>

enum class PortDirection { INPUT, OUTPUT, INOUT };
enum class PortType { WIRE, REG, LOGIC, UNSPECIFIED };

struct Port {
    PortDirection direction;
    PortType      type;
    std::string   width;  // raw text e.g. "[7:0]", empty if scalar
    std::string   name;

    Port(PortDirection direction, PortType type, const std::string& width, const std::string& name)
        : direction(direction), type(type), width(width), name(name) {}
};
