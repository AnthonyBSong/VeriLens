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
#include "GatePrimitive.h"

class Module : public Node {
public:
    std::string                name;
    std::string                source_file;  // set by gen_ast; empty when parsed via Parser::toAST()
    std::vector<Parameter>     parameters;
    std::vector<Port>          ports;
    std::vector<NetDecl>       net_decls;
    std::vector<Instance>      instances;
    std::vector<GatePrimitive> gate_primitives;
    std::vector<Assign>        assigns;
    std::vector<AlwaysBlock>   always_blocks;

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
        // Skip width comparisons for parametric ranges we couldn't reduce to ints.
        auto isKnownWidth = [](const PortWidth& w) {
            return !w.unknown;
        };
        // Look up a signal name in the current module's port/net table.
        auto signalWidth = [&](const std::string& sig) -> const PortWidth* {
            if (auto it = ctx.port_map.find(sig); it != ctx.port_map.end()) return &it->second->width;
            if (auto it = ctx.net_map.find(sig);  it != ctx.net_map.end()) return &it->second->width;
            return nullptr;
        };

        for (const auto& i : instances) {
            if (!seen.insert(i.instance_name).second)
                ctx.error("duplicate instance '" + i.instance_name + "'", i.line, i.column);
            i.validate(ctx); // warns if unresolved

            // All remaining judgments require both the instance and the target Module.
            if (!i.resolved) continue;
            const Module* target = ctx.symbols.at(i.module_name);

            // Index target ports by name for O(1) lookup below.
            std::unordered_map<std::string, const Port*> tports;
            for (const auto& p : target->ports) tports[p.name] = &p;

            std::unordered_set<std::string> connected;
            for (const auto& conn : i.connections) {
                if (conn.port_name.empty()) continue;

                auto pit = tports.find(conn.port_name);
                if (pit == tports.end()) {
                    ctx.error("instance '" + i.instance_name + "': '." + conn.port_name +
                              "' is not a port of '" + i.module_name + "'", i.line, i.column);
                    continue;
                }
                if (!connected.insert(conn.port_name).second) {
                    ctx.error("instance '" + i.instance_name + "': '." + conn.port_name +
                              "' connected more than once", i.line, i.column);
                    continue;
                }

                // Judgment: width of connected signal must match the port's declared width.
                const PortWidth& tp = pit->second->width;
                if (const PortWidth* sp = signalWidth(conn.signal)) {
                    if (isKnownWidth(tp) && isKnownWidth(*sp) && tp.width() != sp->width())
                        ctx.warn("instance '" + i.instance_name + "': port '." + conn.port_name +
                                 "' is " + std::to_string(tp.width()) + " bit(s) but '" +
                                 conn.signal + "' is " + std::to_string(sp->width()) +
                                 " bit(s)", i.line, i.column);
                }
            }

            // Judgment: every output port of the target module must be connected.
            // A floating output means data is silently discarded — misleading in a diagram.
            // .* wildcard implicitly connects matching ports, so skip the check.
            if (!i.wildcard) {
                for (const auto& [pname, pptr] : tports) {
                    if (pptr->direction == PortDirection::OUTPUT && !connected.count(pname))
                        ctx.warn("instance '" + i.instance_name + "': output port '." + pname +
                                 "' is not connected", i.line, i.column);
                }
            }
        }
        for (const auto& a  : assigns)       a.validate(ctx);
        for (const auto& ab : always_blocks) ab.validate(ctx);

        // Gate primitive instance names share the same namespace as module instances.
        for (const auto& g : gate_primitives) {
            if (!g.instance_name.empty() && !seen.insert(g.instance_name).second)
                ctx.error("duplicate gate instance '" + g.instance_name + "'",
                          g.line, g.column);
        }

        return errors;
    }
};
