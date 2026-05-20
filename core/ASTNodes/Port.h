#pragma once
#include <string>
#include "Node.h"
#include "PortWidth.h"

enum class PortDirection { INPUT, OUTPUT, INOUT };
enum class PortType { WIRE, REG, LOGIC, UNSPECIFIED };

class Port : public Node {
public:
    PortDirection direction;
    PortType      type;
    PortWidth     width;
    std::string   name;

    Port(PortDirection direction, PortType type, const PortWidth& width,
         const std::string& name, int line, int column)
        : Node(NodeKind::PORT, line, column),
          direction(direction), type(type), width(width), name(name) {}

    // Judgment: port width must be non-inverted; registers self in ctx for signal lookup.
    void validate(ValidationContext& ctx) const override {
        ctx.port_map[name] = this;
        if (!width.scalar && width.msb < width.lsb)
            ctx.error("port '" + name + "': msb (" + std::to_string(width.msb) +
                      ") < lsb (" + std::to_string(width.lsb) + ")", line, column);
    }
};
