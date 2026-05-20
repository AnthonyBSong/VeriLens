#pragma once
#include <string>
#include <memory>
#include "Node.h"
#include "PortWidth.h"
#include "Expression.h"

enum class NetType { WIRE, REG, LOGIC, TRI, WAND, WOR, SUPPLY0, SUPPLY1 };

class NetDecl : public Node {
public:
    NetType     net_type;
    PortWidth   width;
    std::string name;
    ExprPtr     init;  // optional initializer: reg a = expr; nullptr if absent

    NetDecl(NetType net_type, const PortWidth& width,
            const std::string& name, int line, int column,
            ExprPtr init = nullptr)
        : Node(NodeKind::NET_DECL, line, column),
          net_type(net_type), width(width), name(name), init(std::move(init)) {}

    // Judgment: width non-inverted; init expression well-typed; registers self in ctx.
    void validate(ValidationContext& ctx) const override {
        ctx.net_map[name] = this;
        if (!width.scalar && width.msb < width.lsb)
            ctx.error("net '" + name + "': msb (" + std::to_string(width.msb) +
                      ") < lsb (" + std::to_string(width.lsb) + ")", line, column);
        if (init) init->validate(ctx);
    }
};
