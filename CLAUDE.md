# VeriLens

Verilog/SystemVerilog structural extractor (C++ `core/`) + interactive schematic viewer (`packages/web/`, TypeScript/React/Vite/SVG).

## Build / test

- Launcher: `./verilens <inputs> [-l layout.yaml] [-t top] [--dev]` at the repo root is the single entry point (core build → `gen_ast` → `packages/web/scripts/build-html.mjs`, or the dev server with `VERILENS_SESSION_*` env vars served by the `verilens-session` Vite plugin at `/__verilens/session.json`).

- Core: `cmake -S . -B build && cmake --build build`, tests `cd build && ctest`. JSON goldens in `core/tests/parser/*.ast.json` are regenerated with `python3 core/tests/parser/gen_golden.py` whenever the JSON shape changes.
- Viewer: `cd packages/web && npm install && npm run dev | build | test | test:e2e`. Samples: `samples/demo.ast.json` and `samples/demo.design.json` are generated (gitignored) from `samples/demo/demo.sv` by `npm run gen:samples`, which `dev`, `build`, `typecheck` and `test` run first; it builds `gen_ast` if needed, then `scripts/build-wasm.mjs` compiles the core to `src/wasm/verilens.js` (gitignored, embedded wasm) with `em++` when Emscripten is installed, so Open/drop parses `.v`/`.sv` in the browser via `src/app/parser.ts`; without Emscripten the build skips it and Open reports that the browser parser is missing.

## Contract between core and viewer

`gen_ast` emits an array of modules (see `core/Parser.cpp` `moduleToJson`). The viewer's adapter is `packages/web/src/model/adapters/verilens.ts`. Instance connections carry a parsed `expr` tree; never re-parse Verilog text in the viewer (no regex parsing anywhere). `// verilens: top` marks the top module.

## Architectural invariants (viewer)

1. **Data, rules, geometry, UI state are separate.** `Design` (model/design.ts) is what exists. `LayoutFile` (layout/dsl.ts) is how to arrange it. `ModuleLayout` (layout/types.ts) is computed geometry. UI state lives in state/store.ts. Layout rules never change connectivity; layout code never mutates the design; never add fake circuit edges to the design to express layout wishes (constraint edges live only inside layered.ts).
2. **Geometry changes only on** design load, applying a layout file, or explicit Re-layout. Selection, highlighting, camera and module expansion never trigger layout.
3. **Expansion moves nothing.** Every expandable instance owns a fixed frame from the parent's layout. Expanded contents render in a clipped nested `<svg>` under the header, in the child's local coordinates, scaled to fit. Sibling coordinates, the frame, its port anchors, external routes and the main camera stay identical. Layouts are cached per instance path so collapse/re-expand is identical. Large interiors are read via Focus module, not by growing frames.
4. **Nothing is draggable.** Components, ports and wires have no drag/edit handlers. Panning starts only on the background rect.
5. **Wires are routed after all node movement.** Pipeline in layout/engine.ts: validate → layered candidate → constraint solve (constraints.ts) → route (router.ts) → verify. Never move a node after routing. Junction dots mean a branch; crossings get no dot.
6. **Layout jobs are versioned** (layout/jobs.ts). A result is applied only if it is still the newest for its scope key.
7. **Deterministic**: same design + rules → identical layout. No randomness, stable iteration order (arrays, insertion-ordered maps).
8. **Honest rendering**: undriven / multiply-driven nets, unconnected ports, black boxes, constants and implicit nets are shown or reported, never repaired or hidden.

## Tests that guard the invariants

`packages/web/tests/*.test.ts` (vitest) and `packages/web/e2e/viewer.spec.ts` (Playwright, Chrome channel). Keep them green; add a test when touching layered/constraints/router/store.
