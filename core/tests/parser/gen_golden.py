#!/usr/bin/env python3
"""Generate .ast.json golden files for parser tests.

Scans core/tests/examples/ for .v and .sv files, runs gen_ast on each,
and writes <filename>.ast.json golden files into this directory (core/tests/parser/).
"""

import subprocess, json, sys
from pathlib import Path

root        = Path(__file__).resolve().parent.parent.parent.parent
gen_ast     = root / "build" / "core" / "tools" / "gen_ast"
parser_dir  = Path(__file__).resolve().parent
examples_dir = parser_dir.parent / "examples"

if not gen_ast.exists():
    print(f"gen_ast not found at {gen_ast}")
    print("Build first: cmake --build build")
    sys.exit(1)

files = sorted(examples_dir.rglob("*.v")) + sorted(examples_dir.rglob("*.sv"))
if not files:
    print(f"No .v/.sv files found in {examples_dir}")
    sys.exit(1)

ok = err = 0
for vfile in files:
    result = subprocess.run([str(gen_ast), str(vfile)], capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  ERROR {vfile.name}: {result.stderr.strip()}")
        err += 1
        continue
    try:
        ast = json.loads(result.stdout)
    except json.JSONDecodeError as e:
        print(f"  ERROR {vfile.name}: invalid JSON — {e}")
        err += 1
        continue
    out = parser_dir / (vfile.name + ".ast.json")
    out.write_text(json.dumps(ast, indent=2))
    print(f"  {vfile.name} → {out.name}")
    ok += 1

print(f"\n{ok} generated, {err} errors")
