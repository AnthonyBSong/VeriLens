#pragma once
#include <string>
#include "Node.h"

enum class PortDirection { INPUT, OUTPUT, INOUT };
enum class PortType { WIRE, REG, LOGIC, UNSPECIFIED };

struct Port : Node {
    PortDirection direction;
    PortType      type;
    std::string   width;  // raw text e.g. "[7:0]", empty if scalar
    std::string   name;

    Port(PortDirection direction, PortType type, const std::string& width, const std::string& name, int line, int column)
        : Node(NodeKind::PORT, line, column), direction(direction), type(type), width(width), name(name) {}
};
