#pragma once
#include <string>
#include <vector>
#include "Node.h"
#include "Port.h"
#include "Parameter.h"
#include "NetDecl.h"
#include "Instance.h"
#include "Assign.h"
#include "AlwaysBlock.h"

struct Module : Node {
    std::string              name;
    std::vector<Parameter>   parameters;
    std::vector<Port>        ports;
    std::vector<NetDecl>     net_decls;
    std::vector<Instance>    instances;
    std::vector<Assign>      assigns;
    std::vector<AlwaysBlock> always_blocks;

    Module(const std::string& name, int line, int column)
        : Node(NodeKind::MODULE, line, column), name(name) {}
};
