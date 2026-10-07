#!/usr/bin/env node
// Build a self-contained interactive HTML for a design.
//   node scripts/build-html.mjs design.ast.json [layout.yaml] [--top NAME] [-o out.html]
// Requires `npm run build` first (dist/index.html is the single-file viewer).
import { existsSync, readFileSync, writeFileSync } from 'node:fs';
import { createServer } from 'vite';

const args = process.argv.slice(2);
const take = (flag) => { const i = args.indexOf(flag); return i >= 0 ? args.splice(i, 2)[1] : undefined; };
const out = take('-o') ?? 'design.verilens.html';
const top = take('--top');
const [designPath, layoutPath] = args;
if (!designPath) { console.error('usage: build-html.mjs design.json [layout.yaml] [-o out.html]'); process.exit(1); }
const template = new URL('../dist/index.html', import.meta.url);
if (!existsSync(template)) { console.error('dist/index.html not found: run `npm run build` first'); process.exit(1); }

const server = await createServer({ server: { middlewareMode: true }, logLevel: 'error' });
const { fromVerilensAst, isVerilensAst } = await server.ssrLoadModule('/src/model/adapters/verilens.ts');
const { validateDesign } = await server.ssrLoadModule('/src/model/design.ts');
const { parseLayoutFile } = await server.ssrLoadModule('/src/layout/dsl.ts');
await server.close();

const raw = JSON.parse(readFileSync(designPath, 'utf8'));
let design;
try {
  design = validateDesign(isVerilensAst(raw) ? fromVerilensAst(raw, { top }) : top ? { ...raw, top } : raw);
} catch (e) {
  console.error(`error: ${e.message}`);
  if (isVerilensAst(raw) && raw.length === 0) console.error('the AST has no modules: check the gen_ast errors above');
  process.exit(1);
}
const layoutText = layoutPath ? readFileSync(layoutPath, 'utf8') : '';
if (layoutText) {
  const r = parseLayoutFile(layoutText, design);
  for (const d of r.diagnostics) console.error(`[${d.severity}] ${d.message}`);
  if (!r.file) { console.error('layout file rejected'); process.exit(2); }
}
const data = JSON.stringify({ design, layoutText, name: designPath }).replace(/<\/script/gi, '<\\/script');
const html = readFileSync(template, 'utf8').replace('</head>', `<script type="application/json" id="verilens-data">${data}</script></head>`);
writeFileSync(out, html);
console.log(`wrote ${out} (top=${design.top}, ${Object.keys(design.modules).length} modules)`);
