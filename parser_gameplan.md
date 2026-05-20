# Parser Gameplan

## Goal

The parser produces an AST that supports two diagram outputs:

**Combined structural + dataflow view** — the primary diagram for any module.
Submodule instantiations (`Instance` nodes) and internal dataflow logic (`Assign`
nodes with full expression trees) are rendered together. The expression tree is what
makes operator nodes possible — `BinaryOpExpr{"+"}` becomes an adder box,
`ConditionalExpr` becomes a mux box, `BinaryOpExpr{"|"}` becomes an OR gate. Without
the full expression tree, we can't render the dataflow side of the combined view.

**FSM view** — a per-module lens for any module containing a detectable state machine.
Detected from `always` blocks that contain a `CaseStatement` with `NonBlockingAssign`
transitions. States become nodes, transitions become labelled edges.

Both views are served by the same AST — no separate parse pass needed.

---

## Phase 1 — Extend the AST for richer analysis ✅

**New/updated nodes:**

- [x] `PortWidth` struct — `{ int msb, lsb, bool scalar }` with `width()` helper (see `ASTNodes/PortWidth.h`)
- [x] `Parameter` struct — `{ string name, string default_value }` (see `ASTNodes/Parameter.h`)
- [x] `Expression` hierarchy — `IdentifierExpr`, `LiteralExpr`, `UnaryOpExpr`, `BinaryOpExpr`, `ConditionalExpr`, `ConcatExpr`, `ReplicationExpr`, `BitSelectExpr`, `PartSelectExpr` (see `ASTNodes/Expression.h`)
- [x] Update `Assign` — `lhs` and `rhs` are now `ExprPtr` instead of raw strings
- [x] `Statement` base class + `SeqBlock`, `BlockingAssign`, `NonBlockingAssign`, `IfStatement`, `CaseStatement`, `CaseItem` (see `ASTNodes/Statement.h`)
- [x] Update `AlwaysBlock` — `body` is now `StmtPtr` instead of opaque strings
- [x] Update `Module` — `parameters` is now `vector<Parameter>` instead of `vector<string>`
- [x] Fix `NetDecl` — renamed `kind` → `net_type` to remove shadowing; `width` is now `PortWidth`
- [x] Update `Port` — `width` is now `PortWidth` instead of raw string

---

## Phase 2 — Parser class skeleton ✅

See `Parser.h` and `Parser.cpp` — all function stubs are in place.

---

## Phase 3 — Implementation order

**Token helpers first** — everything depends on these:
- [ ] `current()`, `peek()`, `consume()`, `expect()`, `check()`, `match()`
- [ ] `skipToSemicolon()`, `skipBlock()`, `skipToEndmodule()`

**Pass 1:**
- [ ] `collectModuleNames()` — scan for `MODULE IDENTIFIER` pairs, ~10 lines

**Pass 2 top-down:**
- [ ] `parse()` — loop calling `parseModule()` until EOF
- [ ] `parseModule()` — calls header then dispatches body items by keyword
- [ ] `parseModuleHeader()` + `parseParameterList()` + `parsePortList()`
- [ ] `parsePortDeclaration()`, `parseNetDeclaration()`, `parseParameterDeclaration()`
- [ ] `parseContinuousAssign()` — must call `parseExpression()` for full rhs tree (needed for combined view)
- [ ] `parseInstance()` — uses `module_names_` to disambiguate from net declarations
- [ ] `parseAlwaysBlock()` + `parseSensitivityList()`
- [ ] `parseStatement()` dispatcher
- [ ] `parseSeqBlock()`, `parseIfStatement()`
- [ ] `parseCaseStatement()` + `parseCaseItem()` — FSM-critical path
- [ ] `parseBlockingAssign()`, `parseNonBlockingAssign()`
- [ ] `parseExpression()` — full recursive descent with precedence (see expression precedence table below)
- [ ] `parseLValue()` — identifier with optional `[i]` or `[a:b]`
- [ ] `parseWidth()` — `[msb:lsb]`

---

## Phase 4 — JSON netlist serialization

The C++ core outputs a JSON structural netlist consumed by the diagram renderer.
The JSON must carry enough information for both diagram views.

**For the combined view**, each module's JSON needs:
- Its ports (name, direction, width)
- Its submodule instances (module name, instance name, port connections)
- Its net declarations (for tracing signal edges)
- Its assign statements serialized as expression trees — so the renderer knows
  to draw an adder box, mux box, gate, etc. and which signals connect to each port

**For the FSM view**, each module's JSON needs:
- Its always blocks serialized with full statement trees
- The renderer (or a separate analysis pass) walks the statement tree to detect:
  - The state register (reg assigned in a clocked always block via case)
  - States (case item patterns)
  - Transitions (non-blocking assigns to the state register inside case items)
  - Output assertions (other non-blocking/blocking assigns inside case items)

**Serialization checklist:**
- [ ] `ExprToJson` — walk `Expression` tree and emit operator/operand JSON
- [ ] `StmtToJson` — walk `Statement` tree recursively
- [ ] `ModuleToJson` — serialize ports, parameters, nets, instances, assigns, always blocks
- [ ] Schema definition in `schemas/` — agreed contract between core and renderer

---

## Key design notes

### Expression tree → diagram nodes (combined view)

The renderer maps expression tree nodes to diagram boxes:

| Expression node | Diagram node |
|---|---|
| `BinaryOpExpr{ "+" }` | Adder |
| `BinaryOpExpr{ "-" }` | Subtractor |
| `BinaryOpExpr{ "&" }` | AND gate |
| `BinaryOpExpr{ "\|" }` | OR gate |
| `BinaryOpExpr{ "^" }` | XOR gate |
| `BinaryOpExpr{ "<<" / ">>" }` | Shifter |
| `ConditionalExpr` | Mux |
| `ConcatExpr` | Concatenation node |
| `IdentifierExpr` | Wire (connects to a named signal) |
| `LiteralExpr` | Constant input |

### FSM detection heuristic (FSM view)

A module is considered to have an FSM if it contains an `AlwaysBlock` where:
1. The sensitivity list contains `posedge` or `negedge` (clocked block)
2. The body contains a `CaseStatement`
3. At least one `CaseItem` body contains a `NonBlockingAssign` whose `lhs` matches
   a reg declared in the module (the state register)

### `parseStatement()` dispatcher

| Current token | Calls |
|---|---|
| `BEGIN` | `parseSeqBlock()` |
| `IF` | `parseIfStatement()` |
| `CASE` / `CASEX` / `CASEZ` | `parseCaseStatement()` |
| `IDENTIFIER` | `parseLValue()` then check `=` vs `<=` |

### `parseInstance()` vs net declaration

When you see `IDENTIFIER IDENTIFIER`, check if the first name is in `module_names_`.
If yes → instance. If no → net with implicit type (common in older Verilog).

### `parseExpression()` — precedence ladder (lowest to highest)

```
ternary         ? :
logical or      ||
logical and     &&
bitwise or      |
bitwise xor     ^
bitwise and     &
equality        == != === !==
relational      < <= > >=
shift           << >> <<< >>>
additive        + -
multiplicative  * / %
unary           ~ ! - & | ^ (reduction)
atom            literal, identifier, (expr), {concat}, bit/part select
```

Each level is a separate helper function calling the next level down — the standard
recursive descent approach for operator precedence.
