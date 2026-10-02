import { chromium } from '@playwright/test';
const out = new URL('../../test-results/shots/', import.meta.url).pathname; await import('node:fs').then((fs) => fs.mkdirSync(out, { recursive: true }));
const browser = await chromium.launch({ channel: 'chrome' });
const page = await browser.newPage({ viewport: { width: 1400, height: 900 } });
const errors = [];
page.on('pageerror', (e) => errors.push('pageerror: ' + e.message));
page.on('console', (m) => { if (m.type() === 'error' || m.type() === 'warning') errors.push(m.type() + ': ' + m.text()); });
const sample = process.argv[2] ?? 'layout';
const focus = process.argv[4] ? '&focus=' + process.argv[4] : '';
await page.goto('http://localhost:4173/?sample=' + sample + focus);
try { await page.waitForSelector('.module-view', { timeout: 8000 }); } catch { await page.screenshot({ path: out + 'fail.png' }); console.log(await page.locator('body').innerText()); console.log(errors.join('\n')); process.exit(1); }
await page.waitForTimeout(400);
await page.screenshot({ path: out + sample + (process.argv[4] ? '-' + process.argv[4].replace(/\//g, '_') : '') + '-1.png' });
if (process.argv[3] === 'expand') {
  await page.click('[data-toggle="top/compute"]');
  await page.waitForTimeout(400);
  await page.screenshot({ path: out + sample + '-2-expanded.png' });
  await page.click('[data-nested="top/compute"] [data-toggle="top/compute/pe0"]');
  await page.waitForTimeout(400);
  await page.screenshot({ path: out + sample + '-3-nested.png' });
}
console.log(errors.join('\n') || 'no console errors');
await browser.close();
