# VeriLens viewer (`packages/web`)

Interactive hierarchical RTL schematic viewer. Consumes the C++ core's AST JSON
(or a normalized design JSON) plus an optional YAML layout file, and renders a
layered schematic with expandable module instances. Runs as a dev server, a
static build, or a single self-contained HTML file.

## Run

The repo-root launcher is the one entry point for real projects:

```sh
./verilens rtl/ [more files...] [-l layout.yaml] [-t top_module] [-o out.html] [--dev] [--port N] [--no-open] [--rebuild]
```

It builds `gen_ast` and the viewer bundle when missing, parses the inputs, and
either writes a self-contained `<name>.verilens.html` (default, opened in the
browser) or, with `--dev`, starts the Vite dev server with the parsed design and
the layout file preloaded (served at `/__verilens/session.json`; **Reload files**
in the Layout tab re-reads both from disk).

Package scripts, for development:

```sh
cd packages/web
npm install
npm run dev            # http://localhost:5173, bundled demo only (?sample=layout|conflict, ?focus=top/compute)
npm run build          # dist/index.html, single file, self-contained
npm run preview        # serve dist on :4173
npm test               # vitest: layout invariants, DSL validation, adapter, connectivity, stale jobs
npm run test:e2e       # playwright (uses installed Google Chrome): drag/expand/highlight/focus/diagnostics
node scripts/build-html.mjs my.ast.json [layout.yaml] [--top NAME] -o my.html   # what the launcher calls
```

Or use **Open…** in the toolbar to load `*.ast.json` / `*.design.json` and
`*.yaml`, edit the rules in the **Layout** tab, **Apply**, and finish with
**Export HTML**.

## Workflow: plan, then finalize

1. **Plan.** Load the project. The *Modules* tab lists every module definition;
   click one to open it as the main schematic. Each module is laid out
   automatically. Write rules in the *Layout* tab (see `docs/FORMATS.md`) and
   *Apply*; required rules that cannot hold produce diagnostics and the last
   valid layout stays on screen.
2. **Finalize.** *Export HTML* writes one file with the design and rules
   embedded. Opening it gives the same interactive viewer: the top module with
   `[+]` on every instance, Focus module, search, net highlighting.

The module that opens first is the one marked with a `// verilens: top`
comment; otherwise the unique module that nothing instantiates.

## Interactions

- Drag the background to pan, wheel to zoom around the pointer, `f` / *Fit design*, *Fit selection*.
- Click a component: inspector shows path, source line, params, ports and nets;
  drivers turn green, sinks blue, both yellow (feedback).
- Click a wire or terminal: the net is highlighted through every expanded module
  boundary, and the inspector lists drivers, sinks and the same net in other scopes.
- `[+]` / `[−]` on an instance header expands or collapses its contents in
  place. Nothing else moves. Repeated instances (`pe0`, `pe1`) are independent.
- *Focus module* (inspector, tree `⤢`, breadcrumbs) opens a module as the main
  schematic with its own camera; *Back* restores the previous view and camera.
- Search matches hierarchical instance paths and net names.
- Components and ports cannot be dragged; there are no connection handles.

## Inputs

See `docs/FORMATS.md` for the exact contracts:

- **VeriLens AST JSON** – output of `gen_ast` (array of modules). Adapter: `src/model/adapters/verilens.ts`.
- **Normalized design JSON** – `{ version: 1, top, modules }`. Validated on load.
- **Layout YAML v1** – `defaults`, per-scope `constraints` (`leftOf`, `above`, `alignY`; required/preferred), `ports` order, `frames`.

Samples in `samples/`: `demo/demo.sv` (source), `demo.ast.json`,
`demo.design.json`, `demo.layout.yaml` (visibly changes placement: fixed 640×420
compute frame, explicit port order, datapath row, controller above),
`demo-conflict.layout.yaml` (every kind of validation error).

## Architecture

```
src/model/design.ts         normalized design model + validation + net index + cross-boundary trace
src/model/adapters/         VeriLens AST -> design
src/layout/symbols.ts       symbol catalogue: node sizes, port anchors
src/layout/layered.ts       deterministic layered placement (cycle breaking, longest-path layering,
                            barycenter ordering, priority placement, long-edge lanes)
src/layout/constraints.ts   required/preferred constraint solving on the candidate, verification
src/layout/router.ts        orthogonal channel router; junction dots only at real branches
src/layout/engine.ts        pipeline per scope -> ModuleLayout (pure, deterministic)
src/layout/dsl.ts           YAML layout file parsing + validation
src/layout/jobs.ts          versioned async jobs; stale results are dropped
src/state/store.ts          design | rules | geometry cache | UI state (kept separate)
src/ui/                     React + SVG renderer, panels
```

Layout pipeline per scope: validate rules → layered candidate → solve required
geometry + non-overlap close to the candidate → route wires on final geometry →
verify and report. Geometry only changes on load, on applying a layout file, or
on *Re-layout*. Selection, camera, and expansion never trigger layout.

Why not elkjs: ELK layered supports layer partitioning and semi-interactive
in-layer ordering, but not gap-based relative constraints, alignment, or fixed
frames, and its edge routes become invalid the moment a node is nudged. A small
in-house engine keeps constraints, routing and determinism in one place; the
price is a simpler heuristic (barycenter/priority) than ELK's.

## Module expansion invariant

Each expandable instance gets a fixed frame at layout time (default 260×170, or
the `frames` rule). Expansion renders the child's own layout inside a clipped
viewport under the header, scaled to fit. The frame, its port anchors, sibling
positions, external routes and the main camera are unchanged by expansion, and
collapsing then re-expanding shows the identical internal view (the layout is
cached per instance path). Trade-off: a big interior in a small frame is too
small to read inline; use *Focus module* for it. No nested pan/zoom in this
version.

## Validation results

- `npm test`: 29 unit tests (deterministic output, no node overlap, wires end on
  anchors, junctions only at branches, required constraints satisfied, preferred
  violations reported with amounts, conflicting rules rejected with ids,
  repeated instances independent, cross-boundary tracing, stale jobs ignored,
  rejected rules keep the last valid layout).
- `npm run test:e2e`: 9 browser tests (mouse and touch drag never move a
  component or pan; expansion preserves all other geometry, frame, ports, routes,
  camera; collapse/re-expand identical; independent repeated instances; highlight
  across the boundary; selection colors; tree/search locate; focus + back; conflict
  diagnostics with last layout kept).
- Core: 167 C++ tests (`ctest` in `build/`) after the AST additions.

## Limitations

- Layout quality is heuristic (barycenter ordering, priority placement, channel
  routing). Long feedback loops run below the drawing; crossings are not
  minimized globally.
- Only `direction: right` and orthogonal routing. `north`/`south` port sides are
  accepted for placement, but wires to them are routed as if on the west/east edge.
- Always blocks are one `process` cell (inputs = reads, outputs = writes); no
  register/mux inference inside them. No FSM view yet.
- Unresolved modules are black boxes with unknown port directions (drawn as
  `inout`). User-defined types and struct fields collapse to their base net.
- Yosys JSON is not supported. Large designs (thousands of cells per module)
  have not been profiled; layout runs on the main thread.
- Expanded interiors are read-only fits; no inline pan/zoom or lazy loading.
