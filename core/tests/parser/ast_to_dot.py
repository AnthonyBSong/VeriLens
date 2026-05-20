#!/usr/bin/env python3
"""Convert a VeriLens .ast.json file to a Graphviz DOT diagram.

Usage:
    python3 ast_to_dot.py module.v.ast.json [output.dot]
    dot -Tpng output.dot -o output.png
    dot -Tsvg output.dot -o output.svg
"""

import json, sys, re
from pathlib import Path

_uid = 0
def uid():
    global _uid
    _uid += 1
    return f"n{_uid}"

def esc(s):
    return str(s).replace('"', '\\"').replace('\n', '\\n')

def safe(s):
    return re.sub(r'[^a-zA-Z0-9]', '_', str(s))

OP_LABELS = {
    "+": "ADD", "-": "SUB", "*": "MUL", "/": "DIV", "%": "MOD",
    "&": "AND", "|": "OR",  "^": "XOR", "~": "NOT",
    "~&": "NAND", "~|": "NOR", "~^": "XNOR",
    "&&": "&&",  "||": "||",
    "==": "==",  "!=": "!=", "===": "===", "!==": "!==",
    "<":  "<",   "<=": "<=", ">": ">",    ">=": ">=",
    "<<": "<<",  ">>": ">>", "<<<": "<<<", ">>>": ">>>",
    "**": "**",  "call": "CALL",
}

EXPR_COLOR  = "#aed6f1"   # blue-ish  — identifier / literal
OP_COLOR    = "#f9e79f"   # yellow   — operators
MUX_COLOR   = "#f0b27a"   # orange   — mux / ternary

STMT_COLORS = {
    "SEQ_BLOCK":          "#d5f5e3",
    "IF_STATEMENT":       "#e8daef",
    "CASE_STATEMENT":     "#d6eaf8",
    "BLOCKING_ASSIGN":    "#d5f5e3",
    "NONBLOCKING_ASSIGN": "#d5f5e3",
}

PORT_COLOR               = "#a9cce3"
NET_COLOR                = "#d7dbdd"
INSTANCE_COLOR           = "#a9dfbf"   # green  — resolved (defined in project)
INSTANCE_EXTERNAL_COLOR  = "#d5d8dc"   # grey   — unresolved (defined in another file not yet linked)
ASSIGN_COLOR             = "#f9cba5"
ALWAYS_COLOR             = "#d2b4de"
MODULE_COLOR             = "#aed6f1"


def add_edge(lines, src, dst, label=""):
    if label:
        lines.append(f'  {src} -> {dst} [label="{esc(label)}"];')
    else:
        lines.append(f'  {src} -> {dst};')


def expr_node(e, lines):
    """Recursively emit DOT nodes for an expression tree. Returns the root node id."""
    if e is None:
        nid = uid()
        lines.append(f'  {nid} [label="null" shape=point];')
        return nid

    kind = e.get("kind", "?")
    nid = uid()

    if kind == "IDENTIFIER":
        lines.append(f'  {nid} [label="{esc(e["name"])}" shape=ellipse style=filled fillcolor="{EXPR_COLOR}"];')

    elif kind == "LITERAL":
        lines.append(f'  {nid} [label="{esc(e["value"])}" shape=ellipse style=filled fillcolor="{EXPR_COLOR}"];')

    elif kind == "UNARY_OP":
        op = OP_LABELS.get(e.get("op", "?"), e.get("op", "?"))
        lines.append(f'  {nid} [label="{op}" shape=diamond style=filled fillcolor="{OP_COLOR}"];')
        child = expr_node(e.get("operand"), lines)
        add_edge(lines, nid, child)

    elif kind == "BINARY_OP":
        op = OP_LABELS.get(e.get("op", "?"), e.get("op", "?"))
        lines.append(f'  {nid} [label="{op}" shape=diamond style=filled fillcolor="{OP_COLOR}"];')
        lc = expr_node(e.get("lhs"), lines)
        rc = expr_node(e.get("rhs"), lines)
        add_edge(lines, nid, lc, "lhs")
        add_edge(lines, nid, rc, "rhs")

    elif kind == "CONDITIONAL":
        lines.append(f'  {nid} [label="MUX\\n?" shape=diamond style=filled fillcolor="{MUX_COLOR}"];')
        cc = expr_node(e.get("cond"), lines)
        tc = expr_node(e.get("then"), lines)
        ec = expr_node(e.get("else"), lines)
        add_edge(lines, nid, cc, "cond")
        add_edge(lines, nid, tc, "1")
        add_edge(lines, nid, ec, "0")

    elif kind == "CONCAT":
        lines.append(f'  {nid} [label="{{ }}" shape=record style=filled fillcolor="{EXPR_COLOR}"];')
        for i, p in enumerate(e.get("parts", [])):
            pc = expr_node(p, lines)
            add_edge(lines, nid, pc, str(i))

    elif kind == "REPLICATION":
        lines.append(f'  {nid} [label="{{n{{ }}}}" shape=record style=filled fillcolor="{EXPR_COLOR}"];')
        nc = expr_node(e.get("count"), lines)
        vc = expr_node(e.get("value"), lines)
        add_edge(lines, nid, nc, "n")
        add_edge(lines, nid, vc, "val")

    elif kind == "BIT_SELECT":
        lines.append(f'  {nid} [label="[ ]" shape=record style=filled fillcolor="{EXPR_COLOR}"];')
        bc = expr_node(e.get("base"), lines)
        ic = expr_node(e.get("index"), lines)
        add_edge(lines, nid, bc, "base")
        add_edge(lines, nid, ic, "idx")

    elif kind == "PART_SELECT":
        lines.append(f'  {nid} [label="[:]" shape=record style=filled fillcolor="{EXPR_COLOR}"];')
        bc = expr_node(e.get("base"), lines)
        mc = expr_node(e.get("msb"), lines)
        lc = expr_node(e.get("lsb"), lines)
        add_edge(lines, nid, bc, "base")
        add_edge(lines, nid, mc, "msb")
        add_edge(lines, nid, lc, "lsb")
    else:
        lines.append(f'  {nid} [label="{esc(kind)}" shape=ellipse];')

    return nid


def stmt_node(s, lines):
    """Recursively emit DOT nodes for a statement. Returns root node id."""
    if s is None:
        nid = uid()
        lines.append(f'  {nid} [label="null" shape=point];')
        return nid

    kind = s.get("kind", "?")
    nid  = uid()
    color = STMT_COLORS.get(kind, "#ffffff")
    lines.append(f'  {nid} [label="{kind}" style=filled fillcolor="{color}"];')

    if kind == "SEQ_BLOCK":
        for child in s.get("body", []):
            cc = stmt_node(child, lines)
            add_edge(lines, nid, cc)

    elif kind in ("BLOCKING_ASSIGN", "NONBLOCKING_ASSIGN"):
        lc = expr_node(s.get("lhs"), lines)
        rc = expr_node(s.get("rhs"), lines)
        add_edge(lines, nid, lc, "lhs")
        add_edge(lines, nid, rc, "rhs")

    elif kind == "IF_STATEMENT":
        cc = expr_node(s.get("cond"), lines)
        tc = stmt_node(s.get("then"), lines)
        add_edge(lines, nid, cc, "cond")
        add_edge(lines, nid, tc, "then")
        if s.get("else"):
            ec = stmt_node(s.get("else"), lines)
            add_edge(lines, nid, ec, "else")

    elif kind == "CASE_STATEMENT":
        ec = expr_node(s.get("expr"), lines)
        add_edge(lines, nid, ec, "expr")
        for item in s.get("items", []):
            iid = uid()
            pats = item.get("patterns", [])
            if pats:
                label = ", ".join(p.get("value", p.get("name", "?")) for p in pats)
            else:
                label = "default"
            lines.append(f'  {iid} [label="{esc(label)}" style=filled fillcolor="#d6eaf8"];')
            add_edge(lines, nid, iid)
            bc = stmt_node(item.get("body"), lines)
            add_edge(lines, iid, bc)

    return nid


def module_to_dot(mod, lines, idx, show_connections=True):
    mname = mod.get("name", f"module_{idx}")
    cname = f"cluster_{safe(mname)}_{idx}"

    lines.append(f'  subgraph {cname} {{')
    lines.append(f'    label=<<b>module {esc(mname)}</b>>;')
    lines.append(f'    style=filled; fillcolor="#eaf4fb";')

    mid = uid()
    lines.append(f'    {mid} [label="module\\n{esc(mname)}" shape=box3d style=filled fillcolor="{MODULE_COLOR}" fontcolor="#1a237e" fontsize=13];')

    # ports — expand individually for small modules, summarise for large ones
    ports = mod.get("ports", [])
    if len(ports) <= 12:
        for p in ports:
            pid = uid()
            width = "" if p.get("scalar") else f"\\n[{p['msb']}:{p['lsb']}]"
            lines.append(f'    {pid} [label="{esc(p["direction"])}\\n{esc(p["name"])}{width}" shape=parallelogram style=filled fillcolor="{PORT_COLOR}"];')
            add_edge(lines, mid, pid, "port")
    else:
        pid = uid()
        ins  = sum(1 for p in ports if p.get("direction") == "input")
        outs = sum(1 for p in ports if p.get("direction") == "output")
        inouts = sum(1 for p in ports if p.get("direction") == "inout")
        lines.append(f'    {pid} [label="Ports ({len(ports)})\\nin:{ins} out:{outs} inout:{inouts}" shape=parallelogram style=filled fillcolor="{PORT_COLOR}"];')
        add_edge(lines, mid, pid, "ports")

    # parameters
    for p in mod.get("parameters", []):
        pid = uid()
        label = f"param\\n{esc(p['name'])}"
        if p.get("default"):
            label += f"={esc(p['default'])}"
        lines.append(f'    {pid} [label="{label}" shape=note style=filled fillcolor="#fef9e7"];')
        add_edge(lines, mid, pid, "param")

    # net_decls — expand individually for small modules, summarise for large ones
    nets = mod.get("net_decls", [])
    if len(nets) <= 10:
        for n in nets:
            nid = uid()
            width = "" if n.get("scalar") else f"\\n[{n['msb']}:{n['lsb']}]"
            lines.append(f'    {nid} [label="{esc(n["type"])}\\n{esc(n["name"])}{width}" shape=diamond style=filled fillcolor="{NET_COLOR}"];')
            add_edge(lines, mid, nid, "net")
            if n.get("init"):
                ec = expr_node(n["init"], lines)
                add_edge(lines, nid, ec, "init")
    else:
        nid = uid()
        lines.append(f'    {nid} [label="Nets ({len(nets)})" shape=diamond style=filled fillcolor="{NET_COLOR}"];')
        add_edge(lines, mid, nid, "nets")

    # instances
    for inst in mod.get("instances", []):
        iid = uid()
        resolved = inst.get("resolved", True)
        if resolved:
            color     = INSTANCE_COLOR
            style     = "filled"
            fontcolor = "#1b5e20"
            suffix    = ""
        else:
            color     = INSTANCE_EXTERNAL_COLOR
            style     = "filled,dashed"
            fontcolor = "#5d6d7e"
            suffix    = "\\n[external]"
        lines.append(f'    {iid} [label="{esc(inst["module"])}{suffix}\\n({esc(inst["instance"])})" shape=box style="{style}" fillcolor="{color}" fontcolor="{fontcolor}"];')
        add_edge(lines, mid, iid, "instance")
        if show_connections:
            for conn in inst.get("connections", []):
                cid = uid()
                port   = conn.get("port", "")
                signal = conn.get("signal", "")
                lines.append(f'    {cid} [label=".{esc(port)}\\n({esc(signal)})" shape=plaintext fontsize=9];')
                add_edge(lines, iid, cid)

    # assigns
    for a in mod.get("assigns", []):
        aid = uid()
        lines.append(f'    {aid} [label="assign" style=filled fillcolor="{ASSIGN_COLOR}"];')
        add_edge(lines, mid, aid, "assign")
        lc = expr_node(a.get("lhs"), lines)
        rc = expr_node(a.get("rhs"), lines)
        add_edge(lines, aid, lc, "lhs")
        add_edge(lines, aid, rc, "rhs")

    # always blocks
    for ab in mod.get("always_blocks", []):
        abid = uid()
        sens = ab.get("sensitivity", "*")
        lines.append(f'    {abid} [label="always\\n@({esc(sens)})" style=filled fillcolor="{ALWAYS_COLOR}" fontcolor="#4a148c"];')
        add_edge(lines, mid, abid, "always")
        bc = stmt_node(ab.get("body"), lines)
        add_edge(lines, abid, bc)

    lines.append(f'  }}')


def ast_to_dot(ast):
    # count total nodes to decide layout hints
    total_ports = sum(len(m.get("ports", [])) for m in ast)
    large = total_ports > 20

    lines = [
        'digraph AST {',
        '  node [fontname="Helvetica" fontsize=11 margin="0.15,0.08"];',
        '  edge [fontsize=9 color="#555555"];',
        f'  rankdir={"LR" if large else "TB"};',
        '  compound=true;',
        '  concentrate=true;',
        '  nodesep=0.4;',
        '  ranksep=0.6;',
    ]
    for i, mod in enumerate(ast):
        module_to_dot(mod, lines, i, show_connections=not large)
    lines.append('}')
    return '\n'.join(lines)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    src = Path(sys.argv[1])
    if not src.exists():
        print(f"File not found: {src}")
        sys.exit(1)

    ast = json.loads(src.read_text())
    dot = ast_to_dot(ast)

    out = Path(sys.argv[2]) if len(sys.argv) > 2 else src.with_suffix(".dot")
    out.write_text(dot)
    print(f"Written: {out}")
    print(f"Render:  dot -Tpng {out} -o {out.with_suffix('.png')}")
