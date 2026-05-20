#pragma once
#include <string>
#include <memory>
#include "Node.h"
#include "Statement.h"

class AlwaysBlock : public Node {
public:
    std::string sensitivity;  // raw text e.g. "posedge clk, negedge rst"
    StmtPtr     body;         // parsed statement tree (SeqBlock, IfStatement, etc.)

    AlwaysBlock(const std::string& sensitivity, StmtPtr body, int line, int column)
        : Node(NodeKind::ALWAYS_BLOCK, line, column),
          sensitivity(sensitivity), body(std::move(body)) {}

    // Judgment: sensitivity list should be present; body is recursively checked.
    void validate(ValidationContext& ctx) const override {
        if (sensitivity.empty())
            ctx.warn("always block has no sensitivity list", line, column);
        if (body) body->validate(ctx);
    }
};
