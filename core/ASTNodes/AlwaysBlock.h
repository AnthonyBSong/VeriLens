#pragma once
#include <string>
#include <vector>

// Always block treated as an opaque logic node.
// We only track the sensitivity list and which signals the block drives —
// enough to represent it as a node in the diagram without parsing its internals.
struct AlwaysBlock {
    std::string              sensitivity;    // raw text e.g. "posedge clk, negedge rst"
    std::vector<std::string> driven_signals; // signals assigned inside the block

    explicit AlwaysBlock(const std::string& sensitivity)
        : sensitivity(sensitivity) {}
};
