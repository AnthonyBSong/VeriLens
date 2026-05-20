#pragma once
#include "ASTNodes/ASTNodes.h"
#include "Linker.h"

// Drives the type-checking pass over a fully-linked project.
class Validator {
public:
    explicit Validator(const Linker& linker);

    // Validate every module in the project. Returns all errors and warnings
    // collected by recursive validate() calls on each AST node.
    std::vector<ValidationError> run() const;

    // Human-readable one-line representation of a single diagnostic.
    static std::string format(const ValidationError& e);

private:
    const Linker& linker_;
};
