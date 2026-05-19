#pragma once
#include <memory>
#include "Node.h"
#include "Expression.h"

// Continuous assignment: assign lhs = rhs;
struct Assign : Node {
    ExprPtr lhs;  // usually IdentifierExpr or BitSelectExpr
    ExprPtr rhs;  // full expression tree

    Assign(ExprPtr lhs, ExprPtr rhs, int line, int column)
        : Node(NodeKind::ASSIGN, line, column),
          lhs(std::move(lhs)), rhs(std::move(rhs)) {}
};
