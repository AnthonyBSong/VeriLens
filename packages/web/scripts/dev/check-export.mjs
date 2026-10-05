// Opens the exported single-file HTML from disk and checks it boots from embedded data.
import { chromium } from '@playwright/test';
const browser = await chromium.launch({ channel: 'chrome' });
const page = await browser.newPage({ viewport: { width: 1200, height: 800 } });
const errors = [];
page.on('pageerror', (e) => errors.push(e.message));
const url = process.argv[2].startsWith('http') ? process.argv[2] : 'file://' + process.argv[2];
await page.goto(url);
await page.waitForSelector('.module-view .cell', { timeout: 15000 });
console.log('page booted:', await page.locator('.design-name').innerText(), '| focus:', await page.locator('.breadcrumbs').innerText(), '| cells:', await page.locator('.module-view .cell').count(), errors.length ? '| errors: ' + errors.join('; ') : '');
await browser.close();
