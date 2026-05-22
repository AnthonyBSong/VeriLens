#pragma once
#include "ValidationError.h"

enum class NodeKind {
    MODULE,
    PORT,
    NET_DECL,
    INSTANCE,
    PORT_CONNECTION,
    ASSIGN,
    ALWAYS_BLOCK,
    GATE_PRIMITIVE,
};

class Node {
public:
    NodeKind kind;
    int      line;
    int      column;

    Node(NodeKind kind, int line, int column)
        : kind(kind), line(line), column(column) {}

    virtual ~Node() = default;
    virtual void validate(ValidationContext& ctx) const {}
};
