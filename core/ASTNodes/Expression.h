#pragma once
#include <string>
#include <vector>
#include <memory>

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

struct Expression {
    ExprKind kind;
    explicit Expression(ExprKind kind) : kind(kind) {}
    virtual ~Expression() = default;
};

using ExprPtr = std::unique_ptr<Expression>;

// a, my_wire
struct IdentifierExpr : Expression {
    std::string name;
    explicit IdentifierExpr(const std::string& name)
        : Expression(ExprKind::IDENTIFIER), name(name) {}
};

// 4'b1010, 32, 3.14, "hello"
struct LiteralExpr : Expression {
    std::string value;  // raw lexeme
    explicit LiteralExpr(const std::string& value)
        : Expression(ExprKind::LITERAL), value(value) {}
};

// ~a, !a, -a, &a (reduction)
struct UnaryOpExpr : Expression {
    std::string op;
    ExprPtr     operand;
    UnaryOpExpr(const std::string& op, ExprPtr operand)
        : Expression(ExprKind::UNARY_OP), op(op), operand(std::move(operand)) {}
};

// a + b, a & b, a == b, a << b, etc.
struct BinaryOpExpr : Expression {
    std::string op;
    ExprPtr     lhs;
    ExprPtr     rhs;
    BinaryOpExpr(const std::string& op, ExprPtr lhs, ExprPtr rhs)
        : Expression(ExprKind::BINARY_OP), op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
};

// cond ? then_ : else_
struct ConditionalExpr : Expression {
    ExprPtr cond;
    ExprPtr then_;
    ExprPtr else_;
    ConditionalExpr(ExprPtr cond, ExprPtr then_, ExprPtr else_)
        : Expression(ExprKind::CONDITIONAL),
          cond(std::move(cond)), then_(std::move(then_)), else_(std::move(else_)) {}
};

// {a, b, c}
struct ConcatExpr : Expression {
    std::vector<ExprPtr> parts;
    ConcatExpr() : Expression(ExprKind::CONCAT) {}
};

// {4{a}}
struct ReplicationExpr : Expression {
    ExprPtr count;
    ExprPtr value;
    ReplicationExpr(ExprPtr count, ExprPtr value)
        : Expression(ExprKind::REPLICATION), count(std::move(count)), value(std::move(value)) {}
};

// a[i]
struct BitSelectExpr : Expression {
    ExprPtr base;
    ExprPtr index;
    BitSelectExpr(ExprPtr base, ExprPtr index)
        : Expression(ExprKind::BIT_SELECT), base(std::move(base)), index(std::move(index)) {}
};

// a[msb:lsb]
struct PartSelectExpr : Expression {
    ExprPtr base;
    ExprPtr msb;
    ExprPtr lsb;
    PartSelectExpr(ExprPtr base, ExprPtr msb, ExprPtr lsb)
        : Expression(ExprKind::PART_SELECT),
          base(std::move(base)), msb(std::move(msb)), lsb(std::move(lsb)) {}
};
