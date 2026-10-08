#!/usr/bin/env node
// Generates the viewer's bundled sample from samples/demo/demo.sv:
//   samples/demo.ast.json     gen_ast output (the core binary is built first if missing)
//   samples/demo.design.json  the same design through the adapter
// Both are build artifacts (gitignored). npm run dev | build | typecheck | test run this first.
import { spawnSync } from 'node:child_process';
import { existsSync, readFileSync, writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { createServer } from 'vite';

const root = fileURLToPath(new URL('../../../', import.meta.url));
const genAst = `${root}build/core/tools/gen_ast`;
const source = fileURLToPath(new URL('../samples/demo/demo.sv', import.meta.url));
const astOut = fileURLToPath(new URL('../samples/demo.ast.json', import.meta.url));
const designOut = fileURLToPath(new URL('../samples/demo.design.json', import.meta.url));

const run = (cmd, args) => {
  const r = spawnSync(cmd, args, { stdio: 'inherit' });
  if (r.status !== 0) { console.error(`error: ${cmd} ${args.join(' ')} failed`); process.exit(1); }
};
if (!existsSync(genAst)) {
  console.log('gen_ast not built yet: configuring and building the core');
  run('cmake', ['-S', root, '-B', `${root}build`]);
  run('cmake', ['--build', `${root}build`, '--target', 'gen_ast']);
}
const r = spawnSync(genAst, [source], { encoding: 'utf8' });
if (r.stderr) process.stderr.write(r.stderr);
if (![0, 2, 3].includes(r.status)) { console.error(`error: gen_ast exited with ${r.status}`); process.exit(1); }
writeFileSync(astOut, r.stdout);

const server = await createServer({ server: { middlewareMode: true }, logLevel: 'error' });
const { fromVerilensAst } = await server.ssrLoadModule('/src/model/adapters/verilens.ts');
const { validateDesign } = await server.ssrLoadModule('/src/model/design.ts');
const design = validateDesign(fromVerilensAst(JSON.parse(readFileSync(astOut, 'utf8'))));
writeFileSync(designOut, JSON.stringify(design, null, 2) + '\n');
console.log(`wrote samples/demo.ast.json and samples/demo.design.json (top=${design.top}, ${Object.keys(design.modules).length} modules)`);
await server.close();
