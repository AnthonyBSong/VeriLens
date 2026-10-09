# Verilog / SystemVerilog coverage TODO

What the `core/` parser cannot handle yet, found by feeding small snippets of
common RTL to `gen_ast` (survey 2026-10-07, first pass of fixes 2026-10-07).
Work top to bottom.

How to close an item:

1. Add the smallest module that uses the construct to one of the topical
   example files in `core/tests/examples/` (`sv_types.sv`, `sv_expressions.sv`,
   `sv_generate.sv`, `preprocessor.sv`, `sv_skipped.sv`, `recovery.sv`), or a
   new file with a `// verilens: top` pragma.
2. Fix the parser. Keep the structural subset: we need names, widths, directions
   and connectivity, never evaluation. Anything we skip on purpose gets a
   WARNING note (`Parser::note`), anything we fail on gets an ERROR note and
   statement-level recovery, never a dropped file.
3. `python3 core/tests/lexer/gen_golden.py && python3 core/tests/parser/gen_golden.py`,
   then review the new goldens by hand (diff them: every change must be an
   improvement you can explain). Every example file needs a lexer golden, a
   parser golden and a clean validator run (no `[error]` from the validator;
   parser notes are fine).
4. `touch core/tests/*/*_test.cpp && cmake --build build && ctest --test-dir build`.
   The GoogleTest case list is discovered at link time, so an untouched test
   binary will not see a new example file. `gen_ast_examples` runs every
   example through the CLI under a 60 s limit, so a parser that stops making
   progress fails instead of hanging.
5. If the JSON shape changed, update `packages/web/src/model/adapters/verilens.ts`,
   `packages/web/docs/FORMATS.md` and `packages/web/tests/model.test.ts`.

Legend: **ERROR** = parse error, the construct is dropped and reported.
**DROPPED** = no error, but the construct and its connectivity vanish.
**WRONG** = no error, but the model is incorrect.

## Open

- [ ] **Interfaces** (`interface`, `modport`, `bus_if.master m` ports). Phase 0
      is done: an interface-typed port is one `inout` port of unknown width named
      after the type, the interface instance is an unresolved black box, nothing
      is misparsed. Still to do, in order: (1) parse `interface ... endinterface`
      into an `Interface` node with its signals and modports; (2) adapter: bundle
      the interface's signals as one wide port on both sides; (3) rendering of
      bundles; (4) modport direction validation; (5) virtual interfaces, generic
      `interface` ports, parameterised interfaces. Deferred on purpose
      (2026-10-07): the user asked not to implement interfaces yet.
- [ ] **`` `include ``** is not followed: macros and typedefs from included files
      are unknown to the current file. An undefined macro at module level is
      reported (`undefined macro ... ignored`); one inside an expression stays as
      a literal with the macro's name. Needs an include path option on `gen_ast`
      and the launcher.
- [ ] **Loop and genvar variables** used as indices (`y[i] = a[i]` inside
      `for (int i ...)`, `assign d = a[i]` in a generate loop) become implicit
      nets named `i` in the viewer. Register loop variables and genvars in the
      parser and skip them in the adapter.
- [ ] **Hierarchical `defparam`** (`defparam u.v.W = 4`) is reported, not applied.
      Only `inst.PARAM` in the same module is applied.
- [ ] **Procedural code at module level left by unexpanded macros** (trace
      macros from a header that is not on the include path, see
      `external_macros.v`) is skipped as one block with a warning. Fine, but `-D`/`` `define `` on the command line would
      let users expand such macros properly. Same mechanism as the include item.
- [ ] **Generate elaboration**: `if`/`case` generate branches are all flattened
      into the module and instance arrays are one cell, so a `generate if`
      with two alternatives shows both. Needs parameter evaluation; the
      viewer policy for arrays (one cell with `attrs.array` today, or N cells)
      can be revisited then.
- [ ] **`fork ... join`** bodies: the statements after the first `;` are parsed
      as separate statements and `join` is reported. Rare in RTL.
- [ ] **`wire (strong1, weak0) w = a;`** net strength before the declarator and
      `wire [3:0] #(1,2) v` delays are skipped, but charge strength on `trireg`
      (`trireg (small) t;`) is not.

## Done (2026-10-07)

Kept here so the next person knows what is covered without re-probing.

### 0. Hangs and robustness

- [x] `unique` / `unique0` / `priority` before `case` / `casez` / `casex` / `if`.
- [x] Every parse loop guarantees progress (`if (pos_ == before) consume()`), and
      `gen_ast_examples` is a timed CTest case.
- [x] Statement-level recovery: a parse error drops one module item, one
      statement inside `begin ... end`, or one case item, records an ERROR note
      with line and column, and resynchronises past the next `;`. A module
      header error keeps the module with the ports parsed so far. `gen_ast`
      exit code 3 now means "something was dropped" (file or construct); the
      launcher continues.
- [x] Parser notes travel in the module JSON (`notes`) and show up in the
      viewer's diagnostics drawer.

### 1. Parse errors (all now parse)

Packed multi-dimensional arrays (total width is computed, `[3:0][7:0]` = 32
bits), multi-dimensional selects, indexed part-selects `+:` / `-:` (constant
form folds to a plain part-select), `?` wildcard digits in based literals,
arrays of instances and gate arrays (one cell with `attrs.array`), implicit
named connections `.clk`, user function calls (`call` cell), casts `8'(x)` /
`int'(x)` / `signed'(x)` (transparent), `inside`, `==?`, `!=?`, `**`,
assignment patterns `'{...}`, header `import`, package-scoped names in
expressions and widths, a real token preprocessor (`` `define `` with
arguments and `\` continuations, `` `undef ``, `` `ifdef `` / `` `ifndef `` /
`` `elsif `` / `` `else `` / `` `endif `` evaluated at file and module level,
`` `include `` / `` `timescale `` / `` `default_nettype `` dropped), constant
folding of literal arithmetic in ranges (`[`W-1:0]`), assign delays and
strengths, net delays, unlabeled concurrent assertions, `property` /
`sequence` / `checker` blocks, nested module declarations, labelled module
items, statement labels, `begin : name` / `end : name` /
`endfunction : name`, `foreach`, `do ... while`, compound assignments
(`+=`, `<<=`, ...) and `++` / `--`, selects on literals, `module automatic`,
`#(parameter A = 1, B = 2, int C = 3)` headers, `parameter type`.

### 2. Silently wrong (all fixed)

`wire b = a;` emits the assign; variable initializers (`logic c = 0;`) are
kept and reported; `bit` / `byte` / `shortint` / `int` / `longint` /
`integer` / `trireg` / `uwire` and typedef'd, enum, struct and
package-scoped types declare nets (SV integer types with their real width,
user types with an unknown width named after the type); `var` ports;
`(* attributes *)` before items, statements and ports; generate `for` / `if`
/ `case` without the keyword, `generate case`, nested generate loops;
`default clocking` / `clocking` / `covergroup` skipped cleanly; `case (x)
inside` with ranges; `defparam u.P = v` applied to the instance; interface
ports (phase 0, see above).

### 3. Skipped on purpose, now reported as WARNING notes

`initial` / `final` blocks, `function` / `task` bodies (calls are shown as a
`call` cell), `specify`, `covergroup`, `event`, `real` / `time` declarations,
system task calls at module level, undefined macros, variable initializers,
bare procedural `begin ... end` at module level. Silent by design (no
behaviour hidden): `typedef`, `let`, `import`, `export`, `bind`, `class`,
`program`, `primitive` (instances become black boxes), `timeunit`,
`timeprecision`, `genvar`, `property`, `sequence`, `clocking`.
