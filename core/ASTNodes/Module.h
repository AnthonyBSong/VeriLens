#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "Node.h"
#include "Port.h"
#include "Parameter.h"
#include "NetDecl.h"
#include "Instance.h"
#include "Assign.h"
#include "AlwaysBlock.h"

class Module : public Node {
public:
    std::string              name;
    std::string              source_file;  // set by gen_ast; empty when parsed via Parser::toAST()
    std::vector<Parameter>   parameters;
    std::vector<Port>        ports;
    std::vector<NetDecl>     net_decls;
    std::vector<Instance>    instances;
    std::vector<Assign>      assigns;
    std::vector<AlwaysBlock> always_blocks;

    Module(const std::string& name, int line, int column)
        : Node(NodeKind::MODULE, line, column), name(name) {}

    // Judgment: no duplicate names; all children recursively type-checked.
    std::vector<ValidationError> validate(
        const ValidationContext::SymbolTable& symbols) const
    {
        std::vector<ValidationError> errors;
        ValidationContext ctx{symbols, name, {}, {}, errors};

        std::unordered_set<std::string> seen;
        for (const auto& p  : ports)     { if (!seen.insert(p.name).second)          ctx.error("duplicate port '" + p.name + "'", p.line, p.column);             p.validate(ctx);  }
        seen.clear();
        for (const auto& n  : net_decls) { if (!seen.insert(n.name).second)          ctx.error("duplicate net '" + n.name + "'", n.line, n.column);              n.validate(ctx);  }
        seen.clear();
        for (const auto& i  : instances) {
            if (!seen.insert(i.instance_name).second)
                ctx.error("duplicate instance '" + i.instance_name + "'", i.line, i.column);
            i.validate(ctx); // warns if unresolved

            // Port-connection judgment lives here: requires both the instance and
            // the target Module definition, which are both in scope at this level.
            if (i.resolved) {
                const Module* target = ctx.symbols.at(i.module_name);
                std::unordered_set<std::string> known;
                for (const auto& p : target->ports) known.insert(p.name);
                std::unordered_set<std::string> connected;
                for (const auto& conn : i.connections) {
                    if (conn.port_name.empty()) continue;
                    if (!known.count(conn.port_name))
                        ctx.error("instance '" + i.instance_name + "': '." + conn.port_name +
                                  "' is not a port of '" + i.module_name + "'", i.line, i.column);
                    if (!connected.insert(conn.port_name).second)
                        ctx.error("instance '" + i.instance_name + "': '." + conn.port_name +
                                  "' connected more than once", i.line, i.column);
                }
            }
        }
        for (const auto& a  : assigns)       a.validate(ctx);
        for (const auto& ab : always_blocks) ab.validate(ctx);
        return errors;
    }
};
