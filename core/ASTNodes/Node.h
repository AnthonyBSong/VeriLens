#pragma once

enum class NodeKind {
    MODULE,
    PORT,
    NET_DECL,
    INSTANCE,
    PORT_CONNECTION,
    ASSIGN,
    ALWAYS_BLOCK,
};

struct Node {
    NodeKind kind;
    int      line;
    int      column;

    Node(NodeKind kind, int line, int column)
        : kind(kind), line(line), column(column) {}

    virtual ~Node() = default;
};
