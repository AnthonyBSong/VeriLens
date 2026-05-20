#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Expression.h"
#include "ValidationError.h"

enum class StatementKind {
    SEQ_BLOCK,
    BLOCKING_ASSIGN,
    NONBLOCKING_ASSIGN,
    IF_STATEMENT,
    CASE_STATEMENT,
};

class Statement {
public:
    StatementKind kind;
    int           line;
    int           column;

    Statement(StatementKind kind, int line, int column)
        : kind(kind), line(line), column(column) {}

    virtual ~Statement() = default;
    virtual void validate(ValidationContext& ctx) const {}
};

using StmtPtr = std::unique_ptr<Statement>;

// begin ... end
class SeqBlock : public Statement {
public:
    std::vector<StmtPtr> body;

    SeqBlock(int line, int column)
        : Statement(StatementKind::SEQ_BLOCK, line, column) {}
    void validate(ValidationContext& ctx) const override {
        for (const auto& s : body) if (s) s->validate(ctx);
    }
};

// lhs = rhs  — lhs must be an l-value (blocking assigns target regs/wires directly)
class BlockingAssign : public Statement {
public:
    ExprPtr lhs;
    ExprPtr rhs;

    BlockingAssign(ExprPtr lhs, ExprPtr rhs, int line, int column)
        : Statement(StatementKind::BLOCKING_ASSIGN, line, column),
          lhs(std::move(lhs)), rhs(std::move(rhs)) {}
    void validate(ValidationContext& ctx) const override {
        if (!isLValue(lhs))
            ctx.error("blocking assign: left-hand side is not an l-value", line, column);
        if (lhs) lhs->validate(ctx);
        if (rhs) rhs->validate(ctx);
    }
};

// lhs <= rhs  — same l-value judgment, different scheduling semantics
class NonBlockingAssign : public Statement {
public:
    ExprPtr lhs;
    ExprPtr rhs;

    NonBlockingAssign(ExprPtr lhs, ExprPtr rhs, int line, int column)
        : Statement(StatementKind::NONBLOCKING_ASSIGN, line, column),
          lhs(std::move(lhs)), rhs(std::move(rhs)) {}
    void validate(ValidationContext& ctx) const override {
        if (!isLValue(lhs))
            ctx.error("non-blocking assign: left-hand side is not an l-value", line, column);
        if (lhs) lhs->validate(ctx);
        if (rhs) rhs->validate(ctx);
    }
};

// if (cond) then_branch [else else_branch]
class IfStatement : public Statement {
public:
    ExprPtr cond;
    StmtPtr then_branch;
    StmtPtr else_branch;  // nullptr if no else

    IfStatement(ExprPtr cond, StmtPtr then_branch, StmtPtr else_branch,
                int line, int column)
        : Statement(StatementKind::IF_STATEMENT, line, column),
          cond(std::move(cond)),
          then_branch(std::move(then_branch)),
          else_branch(std::move(else_branch)) {}
    void validate(ValidationContext& ctx) const override {
        if (cond)        cond->validate(ctx);
        if (then_branch) then_branch->validate(ctx);
        if (else_branch) else_branch->validate(ctx);
    }
};

// One arm of a case statement: <patterns>: <body>
// Empty patterns means `default`.
class CaseItem {
public:
    std::vector<ExprPtr> patterns;
    StmtPtr              body;
    void validate(ValidationContext& ctx) const {
        for (const auto& p : patterns) if (p) p->validate(ctx);
        if (body) body->validate(ctx);
    }
};

// case/casex/casez (expr) ... endcase
class CaseStatement : public Statement {
public:
    std::string            variant;  // "case", "casex", or "casez"
    ExprPtr                expr;
    std::vector<CaseItem>  items;

    CaseStatement(const std::string& variant, ExprPtr expr, int line, int column)
        : Statement(StatementKind::CASE_STATEMENT, line, column),
          variant(variant), expr(std::move(expr)) {}
    void validate(ValidationContext& ctx) const override {
        if (expr) expr->validate(ctx);
        for (const auto& item : items) item.validate(ctx);
    }
};
