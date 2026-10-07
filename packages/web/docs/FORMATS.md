# Input formats

The viewer consumes two independent inputs: a **design** (what exists) and an
optional **layout file** (how to arrange it). Layout rules never change
connectivity; they only influence geometry.

## 1. Normalized design JSON (`*.design.json`)

Produced by `scripts/gen-samples.mjs` from the core's AST output, or written by
hand / by another adapter. Validated by `validateDesign()` on load.

```jsonc
{
  "version": 1,
  "top": "top",                       // module definition opened first
  "modules": {
    "top": {
      "name": "top",
      "source": { "file": "demo.sv", "line": 5 },      // optional
      "params": { "WIDTH": "32" },                      // optional, raw text
      "blackbox": false,                                // optional: not expandable
      "ports": [
        { "name": "clk", "direction": "input", "width": 1 },
        // width: number of bits, or null when parametric (then widthExpr holds the text)
        // net: id of the internal net bound to the port (boundary mapping). Defaults to the port name.
        { "name": "data", "direction": "output", "width": null, "widthExpr": "W-1:0", "net": "data" }
      ],
      "nets": [
        { "id": "clk", "width": 1 },
        { "id": "$add_5", "width": null },              // ids starting with $ are generated (no label)
        { "id": "x", "width": null, "implicit": true }  // referenced but never declared
      ],
      "cells": [
        {
          "id": "compute", "kind": "instance", "module": "compute",
          "resolved": true,                             // false = black box (module not in design)
          "params": { "DEPTH": "4" },
          "connections": {
            // port -> ordered list of segments, msb-first (several = concatenation)
            "clk":  [ { "net": "clk" } ],
            "b":    [ { "net": "acc", "msb": 15, "lsb": 0 } ],
            "coeff":[ { "const": "12'd0" }, { "net": "coeff_sel" } ],
            "dbg":  []                                  // explicitly unconnected
          },
          "source": { "file": "demo.sv", "line": 40 }
        },
        {
          "id": "$mux_6", "kind": "primitive", "type": "mux", "label": "mux",
          "ports": [ { "name": "S", "direction": "input", "width": null },
                     { "name": "1", "direction": "input", "width": null },
                     { "name": "0", "direction": "input", "width": null },
                     { "name": "Y", "direction": "output", "width": null } ],
          "connections": { "S": [{ "net": "mode" }], "1": [{ "net": "sum" }], "0": [{ "net": "pe1_out" }], "Y": [{ "net": "mux_out" }] }
        }
      ]
    }
  }
}
```

Rules:

- Cell ids are unique within a module. Instance paths are `top/child/grandchild`
  and identify *instances*; two instances of one module have independent
  selection and expansion state.
- Instances take their port list from the module definition. Unresolved
  instances (`resolved: false`) list the ports they connect in `ports`.
- Primitives must list `ports`. Known `type`s get a schematic symbol:
  `and nand or nor xor xnor not buf mux add sub mul div mod pow neg eq ne lt le gt ge shl shr sshl sshr`
  plus `process` (an always block; `attrs.seq` selects the register symbol).
  Any other type is drawn as a labelled box.
- Drivers and sinks are derived from port directions: outputs and module input
  ports drive; inputs and module output ports sink; `inout` counts as both.
  Nets with no driver or several drivers are reported, never repaired.
- Constants are `{ "const": "..." }` segments and render as literal boxes.
- Nothing is inferred from names. Connectivity across a boundary is only what
  `ports[].net` inside the child and `connections` in the parent say.

## 2. VeriLens core AST JSON (`gen_ast` output)

An array of modules as emitted by `build/core/tools/gen_ast`. The adapter in
`src/model/adapters/verilens.ts` converts it:

| AST construct | Design model |
| --- | --- |
| module ports / net declarations | ports, nets (struct member access `bus.field` maps to net `bus`) |
| instance with named/positional/`.*` connections | instance cell; positional ports resolved through the target definition |
| connection expression (`{a, 2'b0}`, `x[3:0]`) | segment list, using the parsed `expr` tree from the core |
| `assign y = a + b` | one primitive cell per operator; nested operators use generated `$…` nets |
| `assign y = a` / `assign y = 8'd0` | `buf` cell |
| `wire b = a;` | net `b` plus a `buf` cell (the implied continuous assignment) |
| `f(a, b)`, `$clog2(N)` | `call` cell labelled `f()` with inputs `A1, A2…` |
| `a[i]` with a non-constant index | `bitsel` / `partsel` cell |
| `a[b+:w]` with constant `b`, `w` | plain part-select; otherwise a `partsel` cell whose `M` input is the `+:` operator cell and `L` is absent |
| `c u[3:0] (...)`, `buf b[1:0] (...)` | one cell with `attrs.array = "3:0"` (shown in the inspector) |
| `int i;`, `bit [3:0] c;`, typedef'd / enum / struct variables | nets; SV integer types get their real width, typedef'd types an unknown width named after the type |
| interface port `bus_if.mst m` | `inout` port of unknown width named after the interface type; the interface instance is a black box |
| `{N{a}}` | `repl` cell |
| gate primitive `and g(y, a, b)` | primitive cell with ports `Y, A1, A2…` |
| `always` block | `process` cell: inputs = signals read (plus clock/reset), outputs = signals written; a signal both read and written is state, not an input |
| parameters used as identifiers | constants |
| `// verilens: top` pragma | `design.top` (otherwise the unique root module, otherwise the first root) |
| `notes` (parser diagnostics) | `attrs.notes` on the module; shown in the viewer's diagnostics drawer. `error` = a construct failed to parse and was dropped, `warning` = skipped on purpose (initial/final blocks, function and task bodies, specify, covergroup, event, real/time, variable initializers) |

The core produces this from Verilog/SystemVerilog with its own lexer, token
preprocessor (`define/`ifdef/macros with arguments) and parser (no
regular-expression parsing). A statement that fails to parse is dropped and
reported in `notes`; the module survives. Not supported: `generate` semantics
beyond flattening (every branch's contents are kept), interface contents and
modports (interface ports and instances are honest black boxes), package
contents (package-scoped names are kept as written), and `include` files.

Preparation workflow:

```sh
cmake -S . -B build && cmake --build build
./build/core/tools/gen_ast path/to/project > project.ast.json   # a file, several files or a directory
# open project.ast.json in the viewer, or:
node packages/web/scripts/build-html.mjs project.ast.json [layout.yaml] -o project.html
```

Yosys JSON is not supported yet; the core parser is the ingestion path.

## 3. Layout DSL (YAML, version 1)

```yaml
version: 1

defaults:
  direction: right      # only 'right' is supported
  nodeGap: 32           # vertical gap between nodes in a layer (world units)
  layerGap: 80          # minimum horizontal gap between layers
  routing: orthogonal   # only 'orthogonal' is supported

scopes:
  - scope: top/compute      # hierarchical *instance* path (also accepted: `module:`)
    constraints:
      - id: pe-chain        # ids are unique within a scope
        kind: leftOf        # right(a) + gap <= left(b)
        a: pe0
        b: pe1
        gap: 60
        strength: required  # required | preferred (default required)
      - id: pe-row
        kind: alignY        # equal vertical centers
        nodes: [pe0, pe1]
        strength: preferred
        weight: 3           # preferred rules are applied heaviest first
      - id: ctl-above
        kind: above         # bottom(a) + gap <= top(b)
        a: controller
        b: compute
        gap: 40
    ports:
      - node: compute
        side: west          # west | east | north | south
        order: [data_in, valid_in]   # top-to-bottom on west/east, left-to-right on north/south;
                                     # unlisted ports follow in declaration order
    frames:
      - node: compute       # expandable instances only
        width: 640          # includes the header and the internal viewport
        height: 420
```

Semantics:

- Scope paths identify instances (`top`, `top/compute`, `top/compute/pe0`);
  node names inside a scope reference the immediate child cells of that
  instance's module (instances, generated operator cells such as `$add_5`).
- All dimensions and gaps are world units (SVG user units).
- `leftOf`/`above` are enforced by nudging the automatic candidate: layers are
  shifted right for `leftOf` gaps, nodes are pushed down (or the upper node
  pulled up) for `above`. `leftOf` also feeds layering, so a required `leftOf`
  against the signal flow turns the conflicting circuit edge into a feedback
  route rather than failing.
- Required rules must hold or the scope's layout fails with a diagnostic naming
  the scope and constraint ids; the previously valid layout is retained.
- Preferred rules are kept only if they do not break a required rule or a
  heavier preferred rule already satisfied; violations are reported with the
  remaining amount in world units.
- Frames fix the node size. A large interior is fitted (scaled down) into the
  frame's viewport when expanded; use Focus module to read it at full size.

Validation (at import, all reported at once): unknown scope paths, unknown
nodes, unknown ports, duplicate constraint ids, unsupported kinds
(`sameRow`, …), invalid values (negative gaps, non-positive sizes/weights),
unknown keys, cycles among required `leftOf`/`above` rules, and required
`alignY` contradicting required `above` on the same nodes. Any error rejects the
whole file; nothing is applied partially.
