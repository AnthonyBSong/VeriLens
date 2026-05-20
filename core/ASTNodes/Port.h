#pragma once
#include <string>
#include "Node.h"
#include "PortWidth.h"

enum class PortDirection { INPUT, OUTPUT, INOUT };
enum class PortType { WIRE, REG, LOGIC, UNSPECIFIED };

struct Port : Node {
    PortDirection direction;
    PortType      type;
    PortWidth     width;
    std::string   name;

    Port(PortDirection direction, PortType type, const PortWidth& width,
         const std::string& name, int line, int column)
        : Node(NodeKind::PORT, line, column),
          direction(direction), type(type), width(width), name(name) {}
};
