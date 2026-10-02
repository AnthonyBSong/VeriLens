import { defineConfig } from '@playwright/test';

export default defineConfig({
  testDir: 'e2e',
  timeout: 30_000,
  use: { channel: 'chrome', viewport: { width: 1400, height: 900 }, baseURL: 'http://localhost:4173' },
  webServer: { command: 'npm run build && npm run preview', url: 'http://localhost:4173', reuseExistingServer: true, timeout: 120_000 },
});
