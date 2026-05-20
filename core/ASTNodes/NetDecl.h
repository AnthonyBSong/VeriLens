#pragma once
#include <string>
#include "Node.h"
#include "PortWidth.h"

enum class NetType { WIRE, REG, LOGIC, TRI, WAND, WOR, SUPPLY0, SUPPLY1 };

struct NetDecl : Node {
    NetType     net_type;
    PortWidth   width;
    std::string name;

    NetDecl(NetType net_type, const PortWidth& width,
            const std::string& name, int line, int column)
        : Node(NodeKind::NET_DECL, line, column),
          net_type(net_type), width(width), name(name) {}
};
