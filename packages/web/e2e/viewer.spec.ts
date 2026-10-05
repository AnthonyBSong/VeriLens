import { expect, test, type Page } from '@playwright/test';

// Geometry snapshot of everything drawn in a scope: cell transforms, port pins, wire paths.
async function snapshot(page: Page, scope: string, except?: string) {
  return page.evaluate(([scope, except]) => {
    const view = document.querySelector(`.module-view[data-scope="${scope}"]`)!;
    const cells: Record<string, string> = {};
    for (const c of view.querySelectorAll<SVGGElement>(':scope > .nodes > .cell')) {
      const p = c.dataset.path!;
      if (p === except) continue;
      cells[p] = c.getAttribute('transform') + '|' + [...c.querySelectorAll(':scope > .ports .port rect')].map((r) => `${r.getAttribute('x')},${r.getAttribute('y')}`).join(';');
    }
    const wires = [...view.querySelectorAll<SVGPathElement>(':scope > .wires .wire .line')].map((w) => w.getAttribute('d')).sort();
    const terminals = [...view.querySelectorAll<SVGGElement>(':scope > .nodes > .terminal')].map((t) => t.getAttribute('transform')).sort();
    return { cells, wires, terminals };
  }, [scope, except ?? ''] as const);
}
const camera = (page: Page) => page.locator('#camera').getAttribute('transform');
const frameOf = (page: Page, path: string) => page.evaluate((p) => {
  const c = document.querySelector<SVGGElement>(`.cell[data-path="${p}"]`)!;
  const r = c.querySelector(':scope > .box > rect.body')!;
  return { transform: c.getAttribute('transform'), w: r.getAttribute('width'), h: r.getAttribute('height'), ports: [...c.querySelectorAll(':scope > .ports .port rect')].map((q) => `${q.getAttribute('x')},${q.getAttribute('y')}`).join(';') };
}, path);

test.beforeEach(async ({ page }) => {
  await page.goto('/?sample=layout');
  await page.waitForSelector('.module-view[data-scope="top"] .cell[data-path="top/compute"]');
});

test('dragging a component with the mouse or touch never moves it (and does not pan)', async ({ page }) => {
  const cell = page.locator('.cell[data-path="top/compute"]');
  const before = await frameOf(page, 'top/compute');
  const cam = await camera(page);
  const b = (await cell.boundingBox())!;
  await page.mouse.move(b.x + b.width / 2, b.y + b.height / 2);
  await page.mouse.down();
  await page.mouse.move(b.x + b.width / 2 + 150, b.y + b.height / 2 + 90, { steps: 8 });
  await page.mouse.up();
  expect(await frameOf(page, 'top/compute')).toEqual(before);
  expect(await camera(page)).toBe(cam);
  // touch drag via CDP
  const cdp = await page.context().newCDPSession(page);
  const x = b.x + b.width / 2, y = b.y + b.height / 2;
  await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [{ x, y }] });
  await cdp.send('Input.dispatchTouchEvent', { type: 'touchMove', touchPoints: [{ x: x + 120, y: y + 60 }] });
  await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
  expect(await frameOf(page, 'top/compute')).toEqual(before);
  expect(await camera(page)).toBe(cam);
  // there are no drag handles or editing affordances
  expect(await page.locator('.cell [draggable], .cell .handle, .cell .resize').count()).toBe(0);
});

test('background drag pans and wheel zooms around the pointer', async ({ page }) => {
  const cam = await camera(page);
  await page.mouse.move(400, 700); await page.mouse.down(); await page.mouse.move(500, 750, { steps: 5 }); await page.mouse.up();
  expect(await camera(page)).not.toBe(cam);
  const c2 = await camera(page);
  await page.mouse.move(600, 500); await page.mouse.wheel(0, -300);
  expect(await camera(page)).not.toBe(c2);
});

test('expanding a module keeps every other coordinate, its frame, its ports, external routes and the camera', async ({ page }) => {
  const before = await snapshot(page, 'top');
  const frame = await frameOf(page, 'top/compute');
  const cam = await camera(page);
  const toggle = page.locator('[data-toggle="top/compute"]');
  const tb = (await toggle.boundingBox())!;
  // click with a little jitter: must not start a pan
  const cx = tb.x + tb.width / 2, cy = tb.y + tb.height / 2;
  await page.mouse.move(cx, cy); await page.mouse.down(); await page.mouse.move(cx + 1, cy + 1); await page.mouse.up();
  await page.waitForSelector('svg[data-nested="top/compute"] .module-view');
  expect(await snapshot(page, 'top')).toEqual(before);
  expect(await frameOf(page, 'top/compute')).toEqual(frame);
  expect(await camera(page)).toBe(cam);
  await expect(toggle.locator('text')).toHaveText('−');
  // internal boundary terminals exist for the module's ports
  await expect(page.locator('svg[data-nested="top/compute"] .terminal')).toHaveCount(12);
});

test('collapse then re-expand restores the identical internal view', async ({ page }) => {
  await page.click('[data-toggle="top/compute"]');
  await page.waitForSelector('svg[data-nested="top/compute"] .module-view');
  const inner = await snapshot(page, 'top/compute');
  const viewBox = await page.locator('svg[data-nested="top/compute"]').getAttribute('viewBox');
  await page.click('[data-toggle="top/compute"]');
  await expect(page.locator('svg[data-nested="top/compute"]')).toHaveCount(0);
  await page.click('[data-toggle="top/compute"]');
  await page.waitForSelector('svg[data-nested="top/compute"] .module-view');
  expect(await snapshot(page, 'top/compute')).toEqual(inner);
  expect(await page.locator('svg[data-nested="top/compute"]').getAttribute('viewBox')).toBe(viewBox);
});

test('repeated instances expand and select independently', async ({ page }) => {
  await page.click('[data-toggle="top/compute"]');
  await page.waitForSelector('svg[data-nested="top/compute"] .module-view');
  await page.click('[data-toggle="top/compute/pe0"]');
  await page.waitForSelector('svg[data-nested="top/compute/pe0"] .module-view');
  await expect(page.locator('svg[data-nested="top/compute/pe1"]')).toHaveCount(0);
  await page.locator('svg[data-nested="top/compute/pe0"] .cell').first().click({ force: true });
  await expect(page.getByTestId('status-selection')).toContainText('top/compute/pe0/');
  const selected = await page.locator('.cell.selected').count();
  expect(selected).toBe(1);
});

test('net highlighting follows real connectivity across the hierarchy boundary', async ({ page }) => {
  await page.click('[data-toggle="top/compute"]');
  await page.waitForSelector('svg[data-nested="top/compute"] .module-view');
  await page.locator('.module-view[data-scope="top"] .wire[data-net="fifo_data"] .hit').dispatchEvent('click');
  await expect(page.locator('.module-view[data-scope="top"] .wire[data-net="fifo_data"]')).toHaveClass(/highlight/);
  await expect(page.locator('.module-view[data-scope="top/compute"] .wire[data-net="data_in"]')).toHaveClass(/highlight/);
  await expect(page.locator('.module-view[data-scope="top/compute"] .wire[data-net="pe0_out"]')).not.toHaveClass(/highlight/);
  await expect(page.getByTestId('inspector')).toContainText('top/input_fifo');
});

test('selecting a cell colors drivers and sinks; hierarchy tree and search locate instances', async ({ page }) => {
  await page.click('.cell[data-path="top/compute"]');
  await expect(page.locator('.cell[data-path="top/compute"]')).toHaveClass(/selected/);
  await expect(page.locator('.cell[data-path="top/input_fifo"]')).toHaveClass(/driving|feedback/);
  await expect(page.locator('.cell[data-path="top/output_fifo"]')).toHaveClass(/driven|feedback/);
  await page.click('[data-tree="top/compute/pe1"] .label');
  await expect(page.getByTestId('status-selection')).toContainText('cell top/compute/pe1');
  await expect(page.locator('.breadcrumbs')).toContainText('compute');
  await page.getByTestId('search').fill('output_fifo');
  await page.getByTestId('search').press('Enter');
  await expect(page.getByTestId('status-selection')).toContainText('cell top/output_fifo');
});

test('focus module opens the module as the main schematic and Back restores the view', async ({ page }) => {
  const cam = await camera(page);
  await page.click('.cell[data-path="top/compute"]');
  await page.getByRole('button', { name: 'Focus module' }).click();
  await page.waitForSelector('.module-view[data-scope="top/compute"] .cell[data-path="top/compute/pe0"]');
  await expect(page.locator('.breadcrumbs')).toContainText('top/compute');
  await page.getByRole('button', { name: '◀ Back' }).click();
  await page.waitForSelector('.module-view[data-scope="top"]');
  expect(await camera(page)).toBe(cam);
});

test('conflicting layout rules are rejected with diagnostics while the last valid layout stays', async ({ page }) => {
  const before = await snapshot(page, 'top');
  await page.locator('.tabs button', { hasText: /^layout$/ }).click();
  await page.getByRole('button', { name: 'Conflicting' }).click();
  await expect(page.getByTestId('layout-errors')).toContainText('a-left-of-b -> b-left-of-a');
  await expect(page.getByTestId('diagnostics')).toContainText("unknown node 'nonexistent_block'");
  expect(await snapshot(page, 'top')).toEqual(before);
});
