import { chromium } from '@playwright/test';
const browser = await chromium.launch({ channel: 'chrome' });
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
await page.goto('file:///Users/song/Projects/VeriLens/out/ProcBase.verilens.html');
await page.waitForSelector('.module-view .cell');
await page.click('[data-toggle="lab2_proc_ProcBase/dpath"]'); await page.waitForSelector('svg[data-nested="lab2_proc_ProcBase/dpath"] .module-view');
await page.click('.diag-btn'); await page.waitForTimeout(200);
console.log(await page.locator('[data-testid=diagnostics]').innerText());
await browser.close();
