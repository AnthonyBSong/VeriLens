// Verilog sources -> gen_ast JSON, in the browser, via the Emscripten build of the core
// (scripts/build-wasm.mjs). The glob resolves to nothing when the build was skipped.
const wasm = import.meta.glob<{ default: () => Promise<{ parse: (files: string) => string }> }>('../wasm/verilens.js');

export interface ParseResult { ast: unknown; diagnostics: string }

export async function parseVerilog(files: { name: string; text: string }[]): Promise<ParseResult> {
  const load = Object.values(wasm)[0];
  if (!load) throw new Error('the browser parser is not built (install Emscripten, then rebuild)');
  const mod = await (await load()).default();
  return JSON.parse(mod.parse(JSON.stringify(files))) as ParseResult;
}
