#pragma once
#include <string>
#include <vector>
#include "Node.h"

// Verilog gate primitive instantiation:
//   and [name] (out, in1, in2, ...);
//   or g1 (out, a, b), g2 (out2, c, d);
//
// Port convention (matches LRM):
//   and/or/nand/nor/xor/xnor/buf : ports[0] = output, ports[1..] = inputs
//   not                           : ports[0] = output, ports[1]   = input
//   bufif0/bufif1/notif0/notif1   : ports[0] = output, ports[1] = input, ports[2] = control
class GatePrimitive : public Node {
public:
    std::string              gate_type;      // "and", "or", "not", "nand", etc.
    std::string              instance_name;  // may be empty (anonymous instance)
    std::vector<std::string> ports;          // raw signal expressions, in LRM order

    GatePrimitive(const std::string& gate_type,
                  const std::string& instance_name,
                  std::vector<std::string> ports,
                  int line, int col)
        : Node(NodeKind::GATE_PRIMITIVE, line, col),
          gate_type(gate_type),
          instance_name(instance_name),
          ports(std::move(ports)) {}
};
