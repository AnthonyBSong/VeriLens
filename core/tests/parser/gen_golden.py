#!/usr/bin/env python3
"""Generate .ast.json golden files for parser tests.

All output goes into core/tests/parser/ regardless of where the .v file lives.
Processes .v/.sv files from:
  - core/tests/parser/
  - core/tests/lexer/
"""

import subprocess, json, sys
from pathlib import Path

root     = Path(__file__).resolve().parent.parent.parent.parent
gen_ast  = root / "build" / "core" / "tools" / "gen_ast"
out_dir  = Path(__file__).resolve().parent   # always write here

if not gen_ast.exists():
    print(f"gen_ast not found at {gen_ast}")
    print("Build first: cmake --build build")
    sys.exit(1)

source_dirs = [
    out_dir,
    out_dir.parent / "lexer",
]

for d in source_dirs:
    files = sorted(d.rglob("*.v")) + sorted(d.rglob("*.sv"))
    if not files:
        continue
    print(f"\n{d.name}/")
    for vfile in files:
        result = subprocess.run([str(gen_ast), str(vfile)], capture_output=True, text=True)
        if result.returncode != 0:
            print(f"  ERROR {vfile.name}: {result.stderr.strip()}")
            continue
        try:
            ast = json.loads(result.stdout)
        except json.JSONDecodeError as e:
            print(f"  ERROR {vfile.name}: invalid JSON — {e}")
            continue
        out = out_dir / (vfile.name + ".ast.json")
        out.write_text(json.dumps(ast, indent=2))
        print(f"  {vfile.name} -> {out.name}")
