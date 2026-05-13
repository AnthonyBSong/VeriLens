#pragma once
#include <string>

// Continuous assignment: assign lhs = rhs;
// rhs is stored as raw text — we don't evaluate it, just need lhs to know what signal is driven.
struct Assign {
    std::string lhs;  // signal being driven
    std::string rhs;  // expression, opaque

    Assign(const std::string& lhs, const std::string& rhs)
        : lhs(lhs), rhs(rhs) {}
};
