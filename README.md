![VeriLens](sources/VeriLens.png)

# VeriLens
An open source visualizer that creates hardware diagrams for Verilog and SystemVerilog.

## Architecture

VeriLens contains a lexer, parser, some semantic analysis, and a frontend that draws the hardware diagram. It is a structural extractor — not a full compiler. The goal is to extract just enough information from Verilog to produce accurate, meaningful hardware diagrams.

```
Lexer → Parser → AST → Validator → Diagram Frontend
```

### What each stage does

**Lexer / Parser / AST**
Parse Verilog and build an AST that captures structural information only. Anything that doesn't contribute to the diagram is discarded at parse time. The AST is effectively a structural netlist model:
- Module declarations (name, ports with direction and width)
- Instantiations (module name, instance name, port connections)
- Wire/reg declarations (for tracing connections between instances)
- Always/assign blocks (treated as opaque logic nodes — internals are not analyzed)

`generate` blocks are unrolled into parallel instantiation nodes. Parameters must be resolved before elaboration, which is a reasonable constraint for a visualization tool.

**Validator**
A lightweight pass, some semantic analysis, over the AST that checks connectivity before diagram generation — enough to ensure the diagram is coherent and not misleading:
- All instantiated modules are defined in the input
- Port connection counts match the module definition
- Connected signals have compatible widths
- No dangling connections on non-optional ports

The validator builds a symbol table (module name → port list) as a first pass, then checks all instantiations against it. This is connectivity validation, not full semantic analysis.

**Diagram Frontend**
Consumes the validated AST and renders the hardware diagram. The structural netlist model maps naturally to a graph — modules are nodes, port connections are edges.

## Repository Structure

VeriLens is a monorepo. The C++ core and all JS/TS frontends live together with clear package boundaries.

```
verilens/
├── core/          # C++ — lexer, parser, AST, validator, JSON netlist output
├── packages/
│   ├── renderer/  # Shared diagram rendering — consumes JSON, used by all frontends
│   ├── extension/ # VSCode extension — calls core binary, renders in a Webview panel
│   ├── cli/       # Terminal tool — calls core binary, opens diagram in browser
│   └── web/       # Website — drag-and-drop Verilog upload, fully browser-based
└── schemas/       # JSON schema defining the netlist contract between core and renderer
```

The C++ core outputs a JSON netlist. That JSON is the interface between the C++ and JS sides — both can be developed independently as long as they agree on that schema.

`packages/renderer` is the shared package that all three frontends depend on. Diagram rendering logic is written once and reused across the VSCode extension, CLI, and website.

The VSCode extension and CLI invoke the compiled `core` binary via `child_process`. For the website, `core` can either be compiled to WebAssembly via Emscripten (fully client-side) or called from a backend server.

## What VeriLens does not do

VeriLens is intentionally scoped to visualization. For other use cases we recommend:

- **Full linting and semantic analysis** — [Verible](https://github.com/chipsalliance/verible)
- **Simulation** — [Verilator](https://www.veripool.org/verilator/) or [Icarus Verilog](https://github.com/steveicarus/iverilog)
- **Synthesis** — [Yosys](https://github.com/YosysHQ/yosys)
