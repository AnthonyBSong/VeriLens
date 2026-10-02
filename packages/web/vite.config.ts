import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { viteSingleFile } from 'vite-plugin-singlefile';

// Single-file build: dist/index.html is self-contained so "Export HTML" and
// scripts/build-html.mjs can embed a design into it.
export default defineConfig({
  plugins: [react(), viteSingleFile()],
  build: { target: 'es2022' },
});
