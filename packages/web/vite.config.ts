import { readFileSync } from 'node:fs';
import { defineConfig, type Plugin } from 'vite';
import react from '@vitejs/plugin-react';
import { viteSingleFile } from 'vite-plugin-singlefile';

// `./verilens --dev` exports VERILENS_SESSION_* so the dev server can hand the
// generated AST and layout file to the app at /__verilens/session.json.
// Files are re-read on every request, so editing the YAML + "Reload" picks it up.
function launcherSession(): Plugin {
  return {
    name: 'verilens-session',
    configureServer(server) {
      server.middlewares.use('/__verilens/session.json', (_req, res) => {
        const ast = process.env.VERILENS_SESSION_AST;
        if (!ast) { res.statusCode = 404; res.end(); return; }
        const layoutPath = process.env.VERILENS_SESSION_LAYOUT;
        const body = {
          ast: JSON.parse(readFileSync(ast, 'utf8')),
          layoutText: layoutPath ? readFileSync(layoutPath, 'utf8') : '',
          top: process.env.VERILENS_SESSION_TOP || undefined,
          name: process.env.VERILENS_SESSION_NAME || ast,
          layoutPath: layoutPath || undefined,
        };
        res.setHeader('content-type', 'application/json');
        res.setHeader('cache-control', 'no-store');
        res.end(JSON.stringify(body));
      });
    },
  };
}

// Single-file build: dist/index.html is self-contained so "Export HTML" and
// scripts/build-html.mjs can embed a design into it.
export default defineConfig({
  plugins: [react(), viteSingleFile(), launcherSession()],
  build: { target: 'es2022' },
});
