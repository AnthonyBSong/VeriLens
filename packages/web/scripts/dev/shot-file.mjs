// Screenshots of an exported HTML: top view, expanded instance, focused instance.
//   node scripts/dev/shot-file.mjs <file.html> <instance-path-to-expand>
import { chromium } from '@playwright/test';
import { mkdirSync } from 'node:fs';
const out = new URL('../../test-results/shots/', import.meta.url).pathname; mkdirSync(out, { recursive: true });
const [file, path] = process.argv.slice(2);
const browser = await chromium.launch({ channel: 'chrome' });
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
const errors = []; page.on('pageerror', (e) => errors.push(e.message));
await page.goto('file://' + file);
await page.waitForSelector('.module-view .cell');
await page.waitForTimeout(300);
await page.screenshot({ path: out + 'file-1-top.png' });
if (path) {
  await page.click(`[data-toggle="${path}"]`);
  await page.waitForSelector(`svg[data-nested="${path}"] .module-view`);
  await page.waitForTimeout(300);
  await page.screenshot({ path: out + 'file-2-expanded.png' });
  await page.click(`.cell[data-path="${path}"]`);
  await page.getByRole('button', { name: 'Focus module' }).click();
  await page.waitForSelector(`.module-view[data-scope="${path}"]`);
  await page.waitForTimeout(400);
  await page.screenshot({ path: out + 'file-3-focused.png' });
  const stats = await page.evaluate(() => ({ cells: document.querySelectorAll('.module-view .cell').length, wires: document.querySelectorAll('.module-view .wire').length, diag: document.querySelector('.diag-btn')?.textContent }));
  console.log('focused stats:', JSON.stringify(stats));
}
console.log(errors.length ? 'errors: ' + errors.join('; ') : 'no page errors');
await browser.close();
