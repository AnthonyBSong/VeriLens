#pragma once
#include <string>
#include <vector>
#include <memory>
#include "ValidationError.h"

enum class ExprKind {
    IDENTIFIER,    // a, my_wire
    LITERAL,       // 4'b1010, 32, 3.14
    UNARY_OP,      // ~a, !a, -a
    BINARY_OP,     // a + b, a & b, a == b
    CONDITIONAL,   // cond ? then_ : else_
    CONCAT,        // {a, b, c}
    REPLICATION,   // {4{a}}
    BIT_SELECT,    // a[i]
    PART_SELECT,   // a[msb:lsb]
};

class Expression {
public:
    ExprKind kind;
    explicit Expression(ExprKind kind) : kind(kind) {}
    virtual ~Expression() = default;
    virtual void validate(ValidationContext& ctx) const {}
};

using ExprPtr = std::unique_ptr<Expression>;

// a, my_wire
class IdentifierExpr : public Expression {
public:
    std::string name;
    explicit IdentifierExpr(const std::string& name)
        : Expression(ExprKind::IDENTIFIER), name(name) {}
    // Judgment: identifier must be declared in enclosing module scope.
    //  - Names beginning with '$' are system functions/tasks (e.g. $signed,
    //    $clog2) and are never declared locally.
    //  - Dotted names (`pkg::name`, `inst.port`, `msg.type_`) refer to
    //    package, hierarchical, or struct-member symbols whose tail is not
    //    in the local signal table; we can only judge them once full type
    //    information is wired in.
    void validate(ValidationContext& ctx) const override {
        if (name.empty())                              return;
        if (name[0] == '$')                            return;
        if (name.find('.') != std::string::npos)       return;
        if (name.find("::") != std::string::npos)      return;
        if (!ctx.hasSignal(name))
            ctx.warn("identifier '" + name + "' not declared in module scope", 0, 0);
    }
};

// 4'b1010, 32, 3.14, "hello"  — literals are always well-typed.
class LiteralExpr : public Expression {
public:
    std::string value;  // raw lexeme
    explicit LiteralExpr(const std::string& value)
        : Expression(ExprKind::LITERAL), value(value) {}
};

// ~a, !a, -a, &a (reduction)
class UnaryOpExpr : public Expression {
public:
    std::string op;
    ExprPtr     operand;
    UnaryOpExpr(const std::string& op, ExprPtr operand)
        : Expression(ExprKind::UNARY_OP), op(op), operand(std::move(operand)) {}
    void validate(ValidationContext& ctx) const override {
        if (operand) operand->validate(ctx);
    }
};

// a + b, a & b, a == b, a << b, etc.
class BinaryOpExpr : public Expression {
public:
    std::string op;
    ExprPtr     lhs;
    ExprPtr     rhs;
    BinaryOpExpr(const std::string& op, ExprPtr lhs, ExprPtr rhs)
        : Expression(ExprKind::BINARY_OP), op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
    void validate(ValidationContext& ctx) const override {
        if (lhs) lhs->validate(ctx);
        if (rhs) rhs->validate(ctx);
    }
};

// cond ? then_ : else_
class ConditionalExpr : public Expression {
public:
    ExprPtr cond;
    ExprPtr then_;
    ExprPtr else_;
    ConditionalExpr(ExprPtr cond, ExprPtr then_, ExprPtr else_)
        : Expression(ExprKind::CONDITIONAL),
          cond(std::move(cond)), then_(std::move(then_)), else_(std::move(else_)) {}
    void validate(ValidationContext& ctx) const override {
        if (cond)  cond->validate(ctx);
        if (then_) then_->validate(ctx);
        if (else_) else_->validate(ctx);
    }
};

// {a, b, c}
class ConcatExpr : public Expression {
public:
    std::vector<ExprPtr> parts;
    ConcatExpr() : Expression(ExprKind::CONCAT) {}
    void validate(ValidationContext& ctx) const override {
        for (const auto& p : parts) if (p) p->validate(ctx);
    }
};

// {4{a}}
class ReplicationExpr : public Expression {
public:
    ExprPtr count;
    ExprPtr value;
    ReplicationExpr(ExprPtr count, ExprPtr value)
        : Expression(ExprKind::REPLICATION), count(std::move(count)), value(std::move(value)) {}
    void validate(ValidationContext& ctx) const override {
        if (count) count->validate(ctx);
        if (value) value->validate(ctx);
    }
};

// a[i]
class BitSelectExpr : public Expression {
public:
    ExprPtr base;
    ExprPtr index;
    BitSelectExpr(ExprPtr base, ExprPtr index)
        : Expression(ExprKind::BIT_SELECT), base(std::move(base)), index(std::move(index)) {}
    void validate(ValidationContext& ctx) const override {
        if (base)  base->validate(ctx);
        if (index) index->validate(ctx);
    }
};

// a[msb:lsb]
class PartSelectExpr : public Expression {
public:
    ExprPtr base;
    ExprPtr msb;
    ExprPtr lsb;
    PartSelectExpr(ExprPtr base, ExprPtr msb, ExprPtr lsb)
        : Expression(ExprKind::PART_SELECT),
          base(std::move(base)), msb(std::move(msb)), lsb(std::move(lsb)) {}
    void validate(ValidationContext& ctx) const override {
        if (base) base->validate(ctx);
        if (msb)  msb->validate(ctx);
        if (lsb)  lsb->validate(ctx);
    }
};

// Shared judgment: valid assignment targets are identifiers, selects, or concatenations.
inline bool isLValue(const ExprPtr& e) {
    return e && (e->kind == ExprKind::IDENTIFIER
              || e->kind == ExprKind::BIT_SELECT
              || e->kind == ExprKind::PART_SELECT
              || e->kind == ExprKind::CONCAT);
}
