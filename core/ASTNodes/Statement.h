#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Expression.h"

enum class StatementKind {
    SEQ_BLOCK,
    BLOCKING_ASSIGN,
    NONBLOCKING_ASSIGN,
    IF_STATEMENT,
    CASE_STATEMENT,
};

struct Statement {
    StatementKind kind;
    int           line;
    int           column;

    Statement(StatementKind kind, int line, int column)
        : kind(kind), line(line), column(column) {}

    virtual ~Statement() = default;
};

using StmtPtr = std::unique_ptr<Statement>;

// begin ... end
struct SeqBlock : Statement {
    std::vector<StmtPtr> body;

    SeqBlock(int line, int column)
        : Statement(StatementKind::SEQ_BLOCK, line, column) {}
};

// lhs = rhs
struct BlockingAssign : Statement {
    ExprPtr lhs;
    ExprPtr rhs;

    BlockingAssign(ExprPtr lhs, ExprPtr rhs, int line, int column)
        : Statement(StatementKind::BLOCKING_ASSIGN, line, column),
          lhs(std::move(lhs)), rhs(std::move(rhs)) {}
};

// lhs <= rhs
struct NonBlockingAssign : Statement {
    ExprPtr lhs;
    ExprPtr rhs;

    NonBlockingAssign(ExprPtr lhs, ExprPtr rhs, int line, int column)
        : Statement(StatementKind::NONBLOCKING_ASSIGN, line, column),
          lhs(std::move(lhs)), rhs(std::move(rhs)) {}
};

// if (cond) then_branch [else else_branch]
struct IfStatement : Statement {
    ExprPtr cond;
    StmtPtr then_branch;
    StmtPtr else_branch;  // nullptr if no else

    IfStatement(ExprPtr cond, StmtPtr then_branch, StmtPtr else_branch,
                int line, int column)
        : Statement(StatementKind::IF_STATEMENT, line, column),
          cond(std::move(cond)),
          then_branch(std::move(then_branch)),
          else_branch(std::move(else_branch)) {}
};

// One arm of a case statement: <patterns>: <body>
// Empty patterns means `default`.
struct CaseItem {
    std::vector<ExprPtr> patterns;
    StmtPtr              body;
};

// case/casex/casez (expr) ... endcase
struct CaseStatement : Statement {
    std::string            variant;  // "case", "casex", or "casez"
    ExprPtr                expr;
    std::vector<CaseItem>  items;

    CaseStatement(const std::string& variant, ExprPtr expr, int line, int column)
        : Statement(StatementKind::CASE_STATEMENT, line, column),
          variant(variant), expr(std::move(expr)) {}
};
