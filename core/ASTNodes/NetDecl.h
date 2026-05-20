#pragma once
#include <string>
#include <memory>
#include "Node.h"
#include "PortWidth.h"
#include "Expression.h"

enum class NetType { WIRE, REG, LOGIC, TRI, WAND, WOR, SUPPLY0, SUPPLY1 };

struct NetDecl : Node {
    NetType     net_type;
    PortWidth   width;
    std::string name;
    ExprPtr     init;  // optional initializer: reg a = expr; nullptr if absent

    NetDecl(NetType net_type, const PortWidth& width,
            const std::string& name, int line, int column,
            ExprPtr init = nullptr)
        : Node(NodeKind::NET_DECL, line, column),
          net_type(net_type), width(width), name(name), init(std::move(init)) {}
};
