![VeriLens](sources/VeriLens.png)

# VeriLens

**VeriLens** is an open-source hardware visualization tool that turns Verilog and SystemVerilog projects into interactive, explorable hardware diagrams.

Unlike simple static schematic generators, VeriLens is designed as a **project-aware hardware comprehension tool**. It analyzes an entire Verilog/SystemVerilog codebase, extracts structural hierarchy and connectivity, validates the design model, and renders zoomable diagrams that help users understand how modules, signals, and subsystems fit together.

The goal is not to replace a simulator, synthesizer, or full compiler. Instead, VeriLens focuses on making hardware projects easier to explore, debug, and explain through visualizations.

## Architecture

VeriLens contains a lexer, parser, structural AST, linker, validator, and interactive diagram frontend. It is a **structural extractor**, not a full compiler. The goal is to extract enough information from Verilog/SystemVerilog to produce accurate, meaningful hardware diagrams.

```
Lexer → Parser → AST → Linker → Validator → Diagram Frontend
```

### What each stage does

**Lexer / Parser / AST**
The parser extracts the structural information needed for visualization and intentionally ignores language features that do not contribute to diagram generation. The AST acts as a structural model of the hardware project.

The extracted information includes:

- Module declarations, including names, ports, directions, and widths
- Module instantiations, including module names, instance names, and port connections
- Wire/reg and other net declarations for tracing connections between instances
- Continuous assign statements with their full expression trees (binary/unary operators, muxes, concatenation, replication, bit/part selects)
- Always blocks with their sensitivity lists and full statement trees (sequential blocks, if/case statements, blocking and non-blocking assignments)

The AST is not intended to represent every semantic detail of Verilog/SystemVerilog. Instead, it captures the subset of the design that is useful for understanding hierarchy, connectivity, and structural organization.

The parser uses a two-pass approach: a lightweight first pass collects all module names defined in the current file, and the full parse uses this set to disambiguate module instantiations from net declarations when the same `IDENTIFIER IDENTIFIER` pattern appears in a module body. Cross-file instantiations are handled by look-ahead disambiguation — if the token after an identifier is `#(` or the following two tokens are `IDENTIFIER (`, it is always an instantiation regardless of whether the module is locally defined.

**Linker**
The linker operates over the ASTs produced from all source files in a project. It builds a project-level symbol table mapping module names to their definitions, then resolves all module instantiation references against it.

Each instance in the AST carries a `resolved` flag. After the link pass:

- `resolved: true` — the referenced module is defined somewhere in the project and the instance is fully connected
- `resolved: false` — the referenced module is external (defined in a file not yet provided, or a library cell)

The `gen_ast` tool accepts a single file, a list of files, or a directory. In directory mode it scans recursively for `.v` and `.sv` files, parses all of them, runs the linker, and emits a single combined JSON containing all modules with their resolution state. Unresolved instances are preserved in the output rather than silently dropped — this lets the diagram frontend render them as external placeholders that can be expanded later when the missing files are provided.

Each module in the output also carries a `source_file` field so the diagram frontend can label modules by the file they came from and support cross-file navigation.

**Validator**
The validator performs lightweight semantic analysis over the linked structural AST. Its purpose is to ensure that the generated diagram is coherent and not misleading.

The validator checks:

- Port connection counts match the target module definition
- Connected signals have compatible widths
- Required ports do not have dangling connections

Because the linker has already built the symbol table and resolved all cross-file references before the validator runs, the validator can focus purely on connectivity correctness. This is connectivity validation, not full language-level semantic analysis.

**Diagram Frontend**
The diagram frontend consumes the validated structural model and renders an interactive hardware diagram. The structural model maps naturally to a graph: modules become nodes, and port/signal connections become edges.

VeriLens is designed around interactive exploration rather than static diagram output. Users should be able to zoom in and out of the design, expand modules using controls such as a `+` button, collapse submodules, and navigate the hierarchy at different levels of detail.

The frontend should make large hardware designs easier to understand by supporting:

- Hierarchical zoom in / zoom out
- Expandable and collapsible modules
- Cross-file module navigation
- Signal tracing between instances
- Clean hardware-oriented layout, not just raw graph output
- Multiple views of the same design at different abstraction levels

## Diagram Views

VeriLens produces two diagram types per module. They are complementary — one shows what the hardware is made of, the other shows what the control logic is doing.

**Combined structural + dataflow view**
The primary diagram for any module. Shows submodule instantiations and internal dataflow logic together — operator nodes (muxes, adders, gates) from `assign` statements alongside submodule boxes, all connected by signal edges. Real modules contain both, so separating them into distinct views would split apart tightly coupled logic and produce less useful diagrams. Users can zoom in to a single module, zoom out to the project hierarchy, expand or collapse submodules, and trace signals across module boundaries.

**FSM view**
A per-module lens available on any module that contains a detectable state machine. VeriLens detects the state register and state transitions from `always` block `case` statement structure and renders a dedicated FSM diagram with states as nodes and transitions as labelled edges. Modules without a detectable FSM — pure datapath logic, memories, glue logic — simply do not show this view.

Consider a `datapath_controlunit` module: the combined view shows the hardware — an adder, a mux, registers, submodules, and the signal wires connecting them. The FSM view on the same module shows the control logic driving all of it — what states exist, what conditions cause transitions, what outputs are asserted in each state. A user understanding an unfamiliar design would naturally use both: the combined view to understand the structure, the FSM view to understand the behavior.

## Repository Structure

VeriLens is a monorepo. The C++ core and all JS/TS frontends live together with clear package boundaries.

```
verilens/
├── core/          # C++ — lexer, parser, AST, linker, validator, JSON netlist output
├── packages/
│   ├── renderer/  # Shared diagram rendering — consumes JSON, used by all frontends
│   ├── extension/ # VSCode extension — calls core binary, renders in a Webview panel
│   ├── cli/       # Terminal tool — calls core binary, opens diagram in browser
│   └── web/       # Website — drag-and-drop Verilog upload, browser/server frontend
└── schemas/       # JSON schema defining the contract between core and renderer
```

The C++ core outputs a JSON structural netlist. This JSON format is the interface between the C++ analysis side and the JS/TS visualization side. Both sides can be developed independently as long as they agree on the schema.

`packages/renderer` is the shared rendering package used by all frontends. Diagram rendering logic is written once and reused across the VSCode extension, CLI, and website.

The VSCode extension and CLI invoke the compiled `core` binary through `child_process`. For the website, `core` can either be compiled to WebAssembly using Emscripten for a fully client-side experience, or called from a backend server for larger projects and heavier analysis.

## Installation

### macOS

Install dependencies via Homebrew, then build:

```bash
brew bundle
cmake -S . -B build
cmake --build build
```

### Linux

The recommended approach is **Nix**, which works on any distro and uses a declarative `shell.nix`:

```bash
nix-shell        # drops you into a shell with all dependencies available
cmake -S . -B build
cmake --build build
```

If you don't have Nix, install it from [nixos.org/download](https://nixos.org/download) first.

Alternatively, install manually for your distro:

```bash
# Debian / Ubuntu
sudo apt install cmake libgtest-dev nlohmann-json3-dev

# Arch
sudo pacman -S cmake gtest nlohmann-json
```

### Windows

Install CMake via winget, then install C++ libraries via vcpkg:

```powershell
# Install tools
winget import -i winget-packages.json

# Install C++ libraries
vcpkg install

# Build
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build
```

> **Note:** `VCPKG_ROOT` should point to your local vcpkg installation. If you do not have vcpkg, clone it from [github.com/microsoft/vcpkg](https://github.com/microsoft/vcpkg) and run `bootstrap-vcpkg.bat`.

---

## What makes VeriLens different

VeriLens is not trying to be the first tool that draws a diagram from Verilog. Instead, its goal is to provide a better developer experience for understanding real hardware projects.

Many existing approaches generate static schematics, raw Graphviz diagrams, or simple hierarchy views. VeriLens focuses on:

- Whole-project Verilog/SystemVerilog analysis
- Interactive diagram exploration
- Zoomable and expandable module hierarchy
- Multiple abstraction levels
- Cross-file structural understanding
- Connectivity-aware validation
- Shared rendering across VSCode, CLI, and web frontends

The main contribution is the combination of structural extraction, project-scale understanding, and an interactive frontend designed specifically for hardware comprehension.

## What VeriLens does not do

VeriLens is intentionally scoped to visualization and structural understanding. For other use cases we recommend:

- **Full linting and semantic analysis** — [Verible](https://github.com/chipsalliance/verible)
- **Simulation** — [Verilator](https://www.veripool.org/verilator/) or [Icarus Verilog](https://github.com/steveicarus/iverilog)
- **Synthesis** — [Yosys](https://github.com/YosysHQ/yosys)

VeriLens does not aim to replace these tools. Instead, it complements them by helping users understand the structure of their hardware projects visually.
