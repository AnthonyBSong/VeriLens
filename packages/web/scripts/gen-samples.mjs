// Regenerates samples/demo.design.json from samples/demo.ast.json through the adapter.
import { createServer } from 'vite';
import { readFileSync, writeFileSync } from 'node:fs';
const server = await createServer({ server: { middlewareMode: true }, logLevel: 'error' });
const { fromVerilensAst } = await server.ssrLoadModule('/src/model/adapters/verilens.ts');
const { validateDesign } = await server.ssrLoadModule('/src/model/design.ts');
const ast = JSON.parse(readFileSync(new URL('../samples/demo.ast.json', import.meta.url), 'utf8'));
const design = validateDesign(fromVerilensAst(ast));
writeFileSync(new URL('../samples/demo.design.json', import.meta.url), JSON.stringify(design, null, 2) + '\n');
console.log(`wrote samples/demo.design.json (top=${design.top}, ${Object.keys(design.modules).length} modules)`);
await server.close();
