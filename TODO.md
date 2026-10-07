# Verilog / SystemVerilog coverage TODO

What the `core/` parser cannot handle yet, found by feeding small snippets of
common RTL to `gen_ast` (2026-10-07). Work top to bottom: the first two groups
lose whole files or whole modules, the third group produces a schematic that
is quietly wrong, the fourth is fine to skip but should be reported.

How to close an item:

1. Add `core/tests/examples/<construct>.sv` with the smallest module that uses it
   (and a `// verilens: top` pragma).
2. Fix the parser. Keep the structural subset: we need names, widths, directions
   and connectivity, never evaluation.
3. `python3 core/tests/lexer/gen_golden.py && python3 core/tests/parser/gen_golden.py`,
   then review the new goldens by hand. Every example file needs a lexer golden,
   a parser golden and a clean validator run.
4. `touch core/tests/*/*_test.cpp && cmake --build build && ctest --test-dir build`.
   The GoogleTest case list is discovered at link time, so an untouched test
   binary will not see a new example file.
5. If the JSON shape changed, update `packages/web/src/model/adapters/verilens.ts`,
   `packages/web/docs/FORMATS.md` and the viewer tests.

Legend: **ERROR** = parse error, the whole file is dropped from the model.
**DROPPED** = no error, but the construct and its connectivity vanish.
**WRONG** = no error, but the model is incorrect.

## 0. Hangs and robustness

- [ ] **`unique` / `priority` qualifier makes the parser loop forever** when it is
      followed by `casez`, or by any `case` whose items fail to parse
      (`unique case (a) 2'b1?: ...`, `priority casez (a) 2'd1: ...`). `gen_ast`
      never returns. Cause: `unique` / `unique0` / `priority` are plain
      identifiers, so `parseStatement` treats them as an lvalue and falls into
      `skipToSemicolon`, which strands the token stream inside the case.
      Fix: consume the qualifier before `case` / `casez` / `casex` / `if`.
      Also make every skip loop in `Parser.cpp` consume at least one token per
      iteration, and add a regression that runs the parser under a timeout.
- [ ] **One parse error drops the entire file.** `gen_ast` catches the exception
      per file, so a single unsupported line removes every module in that file
      (exit code 3 since d9de07f). Add statement-level recovery in
      `parseModuleBodyItem`: on error, report it with line and column, skip to the
      next `;` / `end` / `endmodule`, and keep the module. This multiplies the
      value of every item below.

## 1. Parse errors in common RTL (ERROR)

Ordered by how often they appear in real designs.

- [ ] **Packed multi-dimensional arrays** `logic [3:0][7:0] a` in ports and
      declarations. `parseWidth` accepts one range only.
- [ ] **Multi-dimensional selects** `mem[1][2]`, `a[0][1]`, `a[i][7:0]`.
      `parseAtom` accepts one postfix index. Memories themselves
      (`reg [7:0] mem [0:255]`, `logic [3:0] arr [4]`) already parse.
- [ ] **Indexed part-selects** `a[3-:2]`, `a[0+:4]`.
- [ ] **Wildcard bits in literals** `2'b1?`, `4'b1??0` (lexer). Needed by every
      `casez`; today the `?` is read as the ternary operator.
- [ ] **Arrays of instances** `c u[3:0] (.i(a), .o(y));` and gate arrays
      `buf b[1:0] (y, a);`. The viewer needs a policy: one cell with a count
      attribute, or N expanded cells.
- [ ] **Implicit named port connections** `.clk, .rst_n` (dot-name without
      parentheses). `.*` already works.
- [ ] **User function calls in expressions** `assign y = f(a);`,
      `always_comb y = f(a);`. Function definitions are skipped correctly, but a
      call is a parse error. System functions (`$clog2`, `$signed`, `$bits`) work.
      Model a call as a labelled primitive cell with the arguments as inputs.
- [ ] **Casts** `8'(x)`, `int'(x)`, `signed'(x)`.
- [ ] **Operators** `inside`, `==?` / `!=?`, `**`.
- [ ] **Assignment patterns** `'{1'b1, 1'b0}`, `'{default: 0}`.
- [ ] **Header `import`** `module m import p::*; #(...) (...)`. Body-level
      `import p::*;` works.
- [ ] **Package-scoped names in expressions and widths** `p::N`,
      `logic [p::N-1:0] t`. Package-scoped port types (`input p::byte_t a`) work.
- [ ] **Conditional compilation** `` `ifdef `` / `` `else `` / `` `endif `` at file
      level: directives are ignored, so both branches are parsed and a module is
      defined twice. Inside a module body it happens to work. Needs a real
      preprocessor pass (defines table, `ifdef` evaluation).
- [ ] **Macros with arguments** `` `ADD(a, 1) ``. Object-like macros (`` `W ``)
      work because the lexer substitutes them.
- [ ] **Continuous assign with delay or strength** `assign #1 y = a;`,
      `assign (strong1, weak0) y = a;`, and net delays `wire #2 w = a;`.
- [ ] **Concurrent assertions** `assert property (@(posedge clk) a |-> b);`,
      `property` / `sequence` blocks. Labeled (`ap: assert property ...`) and
      immediate assertions already skip cleanly; the unlabeled form errors on
      the sequence operators. Skip to `;` or `endproperty`.
- [ ] **Nested module declarations** (rare).

## 2. Silently wrong model (WRONG / DROPPED)

These violate the honest-rendering invariant: the schematic looks plausible
but connectivity or widths are lost.

- [ ] **Net declaration with initializer** `wire b = a;` drops the implied
      continuous assignment, so `b` renders as undriven. Emit an `assign` from
      `parseNetDeclaration`. (`logic c = 1'b0;` is a variable initializer; keep
      skipping it, but report it.)
- [ ] **SV variable types** `bit`, `int`, `byte`, `shortint`, `longint`,
      `integer` (the last is skipped on purpose) and **typedef'd types**
      (`st_t st;`, `pkt_t pk;`, enums, structs) are dropped. References become
      implicit nets of unknown width. Treat `IDENTIFIER IDENTIFIER ;` as a net
      declaration when the first identifier is not a module name; add the SV
      integer types as net keywords.
- [ ] **`trireg`** nets are dropped.
- [ ] **`var` in ANSI ports** `input var logic a` creates bogus ports named `var`
      and a duplicate-port error.
- [ ] **Attributes** `(* keep *)` in front of a declaration or an `assign`
      swallow that item. Skip `(* ... *)` before dispatching the item.
- [ ] **Generate blocks without the `generate` keyword**
      `for (genvar k = 0; ...) begin : g ... end` and `if (P) begin : g ... end`
      at module level: contents are dropped. The keyworded form works.
- [ ] **`generate case`** and **nested generate loops** drop their contents.
- [ ] **`default clocking ... endclocking`** and **`covergroup ... endgroup`**
      swallow the items that follow them.
- [ ] **`case (x) inside`** parses with `inside` as an identifier (a warning
      about an undeclared net).
- [ ] **`defparam u.W = 4;`** is skipped, so the override is lost.
- [ ] **Interfaces** (`interface`, `modport`, `bus_if.master m` ports): the
      definition is dropped, interface ports become three bogus input ports,
      and the instance is an unresolved black box. See the phased plan in the
      2026-10-07 discussion: keywords and clean skipping first, then a real
      `Interface` node, then bundle ports in the viewer.

## 3. Skipped on purpose, should be reported

Fine for a structural extractor, but the viewer should show a note instead of
silence so nobody wonders where the logic went.

- [ ] `initial` / `final` blocks (including `$readmemh`).
- [ ] `function` / `task` bodies, `typedef`, `let`, `class`, `program`, `bind`,
      `primitive` / UDP (instances become black boxes), `specify`, `event`,
      `timeunit` / `timeprecision`, `` `timescale ``, `` `default_nettype ``.
- [ ] `` `include `` is not followed: macros and typedefs from included files
      are unknown to the current file.

## Verified working (reference, 2026-10-07)

Non-ANSI and ANSI ports, Verilog-2001 untyped ANSI ports, unpacked port arrays,
port default values, `signed` / `unsigned`, `parameter type`, typed and string
parameters, positional and named parameters, positional and named connections,
`.*`, unconnected `.z()`, expression connections, multiple assigns per
statement, LHS part-selects and concatenations, streaming `{<<{a}}`, `'0` /
`'1`, `$clog2` / `$signed` / `$bits`, reduction operators, `<<<`, `!==`, nested
ternaries, `always @*`, `always_ff` with async reset, `always_latch`,
`if` / `else if`, `unique` / `priority case` (when nothing after them fails),
`casex`, multi-label case items, `for` / `foreach` / `while` / `repeat` /
`forever`, labeled blocks, `++` / `+=`, `generate for` / `generate if` with the
keyword, object-like `` `define `` (also multi-line), `` `ifdef `` inside a module
body, body-level `import`, package-typed ports, labeled and immediate
assertions, gate primitives with strength and delay, memories, hierarchical
references, typed `localparam`, `inout` with `'bz`, struct member references
(as dotted names), modules in any order within a file.
