#pragma once
#include <string>

enum class NetKind { WIRE, REG, LOGIC, TRI, WAND, WOR, SUPPLY0, SUPPLY1 };

struct NetDecl {
    NetKind     kind;
    std::string width;  // raw text e.g. "[7:0]", empty if scalar
    std::string name;

    NetDecl(NetKind kind, const std::string& width, const std::string& name)
        : kind(kind), width(width), name(name) {}
};
