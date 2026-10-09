#!/usr/bin/env node
// Compiles the C++ parser to src/wasm/verilens.js (gitignored, wasm embedded as base64 so the
// single-file bundle and Export HTML stay self-contained). Skipped with a notice when emcc is
// missing; the viewer then reports that Verilog cannot be opened in the browser.
import { spawnSync } from 'node:child_process';
import { existsSync, mkdirSync, statSync } from 'node:fs';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('../../../', import.meta.url));
const core = `${root}core/`;
const out = fileURLToPath(new URL('../src/wasm/verilens.js', import.meta.url));
const sources = ['tools/wasm.cpp', 'Lexer.cpp', 'Preprocessor.cpp', 'Parser.cpp', 'Linker.cpp', 'Validator.cpp'].map((f) => core + f);

if (spawnSync('emcc', ['--version'], { stdio: 'ignore' }).error) {
  console.log('build-wasm: emcc not found, skipping (install Emscripten to open Verilog in the browser)');
  process.exit(0);
}
const newest = Math.max(statSync(fileURLToPath(import.meta.url)).mtimeMs, ...sources.map((f) => statSync(f).mtimeMs), ...['Lexer.h', 'Parser.h', 'Linker.h', 'Validator.h', 'Preprocessor.h'].map((f) => statSync(core + f).mtimeMs));
if (existsSync(out) && statSync(out).mtimeMs > newest) process.exit(0);

const includes = ['-I', core];
const brew = spawnSync('brew', ['--prefix', 'nlohmann-json'], { encoding: 'utf8' });
if (brew.status === 0) includes.push('-I', `${brew.stdout.trim()}/include`);
mkdirSync(new URL('../src/wasm/', import.meta.url), { recursive: true });
const args = ['-O2', '-std=c++17', '-fwasm-exceptions', '--bind', ...includes, ...sources,
  '-sMODULARIZE', '-sEXPORT_ES6', '-sSINGLE_FILE', '-sALLOW_MEMORY_GROWTH', '-sENVIRONMENT=web', '-o', out];
console.log('build-wasm: em++ -> src/wasm/verilens.js');
const r = spawnSync('em++', args, { stdio: 'inherit' });
process.exit(r.status ?? 1);
