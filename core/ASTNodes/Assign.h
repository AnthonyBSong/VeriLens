#pragma once
#include <memory>
#include "Node.h"
#include "Expression.h"

// Continuous assignment: assign lhs = rhs;
class Assign : public Node {
public:
    ExprPtr lhs;  // usually IdentifierExpr or BitSelectExpr
    ExprPtr rhs;  // full expression tree

    Assign(ExprPtr lhs, ExprPtr rhs, int line, int column)
        : Node(NodeKind::ASSIGN, line, column),
          lhs(std::move(lhs)), rhs(std::move(rhs)) {}

    // Judgment: continuous assign lhs must be an l-value (wire, not expression result).
    void validate(ValidationContext& ctx) const override {
        if (!isLValue(lhs))
            ctx.error("assign: left-hand side is not an l-value", line, column);
        if (lhs) lhs->validate(ctx);
        if (rhs) rhs->validate(ctx);
    }
};
