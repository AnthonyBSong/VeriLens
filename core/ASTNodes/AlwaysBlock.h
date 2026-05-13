#pragma once
#include <string>
#include <vector>
#include "Node.h"

// Always block treated as an opaque logic node.
// We only track the sensitivity list and which signals the block drives —
// enough to represent it as a node in the diagram without parsing its internals.
struct AlwaysBlock : Node {
    std::string              sensitivity;    // raw text e.g. "posedge clk, negedge rst"
    std::vector<std::string> driven_signals; // signals assigned inside the block

    AlwaysBlock(const std::string& sensitivity, int line, int column)
        : Node(NodeKind::ALWAYS_BLOCK, line, column), sensitivity(sensitivity) {}
};
