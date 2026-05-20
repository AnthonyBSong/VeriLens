#pragma once
#include <string>
#include <memory>
#include "Node.h"
#include "Statement.h"

struct AlwaysBlock : Node {
    std::string sensitivity;  // raw text e.g. "posedge clk, negedge rst"
    StmtPtr     body;         // parsed statement tree (SeqBlock, IfStatement, etc.)

    AlwaysBlock(const std::string& sensitivity, StmtPtr body, int line, int column)
        : Node(NodeKind::ALWAYS_BLOCK, line, column),
          sensitivity(sensitivity), body(std::move(body)) {}
};
