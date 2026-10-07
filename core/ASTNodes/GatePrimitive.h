#pragma once
#include <string>
#include <vector>
#include "Node.h"

// Verilog gate primitive instantiation
class GatePrimitive : public Node {
public:
    std::string              gate_type;      // "and", "or", "not", "nand", etc.
    std::string              instance_name;  // may be empty (anonymous instance)
    std::vector<std::string> ports;          // raw signal expressions, in LRM order
    std::string              array;          // range text of a gate array: `buf b[1:0] (y, a)` -> "1:0"

    GatePrimitive(const std::string& gate_type,
                  const std::string& instance_name,
                  std::vector<std::string> ports,
                  int line, int col)
        : Node(NodeKind::GATE_PRIMITIVE, line, col),
          gate_type(gate_type),
          instance_name(instance_name),
          ports(std::move(ports)) {}
};
