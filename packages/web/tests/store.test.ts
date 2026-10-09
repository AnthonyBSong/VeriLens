import { readFileSync } from 'node:fs';
import { beforeEach, describe, expect, test } from 'vitest';
import { validateDesign } from '../src/model/design';
import { LayoutJobs } from '../src/layout/jobs';
import type { ModuleLayout } from '../src/layout/types';
import { applyLayoutText, ensureLayout, getState, loadDesign, resetStore, select, setTop, toggleExpand } from '../src/state/store';

const design = validateDesign(JSON.parse(readFileSync(new URL('../samples/demo.design.json', import.meta.url), 'utf8')));
const demoYaml = readFileSync(new URL('../samples/demo.layout.yaml', import.meta.url), 'utf8');
const conflictYaml = readFileSync(new URL('../samples/demo-conflict.layout.yaml', import.meta.url), 'utf8');
const tick = () => new Promise((r) => setTimeout(r, 0));
const fake = (tag: string): ModuleLayout => ({ module: tag, scope: 'k', ok: true, width: 1, height: 1, nodes: {}, wires: [], diagnostics: [], undriven: [] });

describe('versioned layout jobs', () => {
  test('a slow stale job cannot overwrite a newer result', async () => {
    const jobs = new LayoutJobs();
    const queued: (() => void)[] = [];
    const p1 = jobs.run('k', () => fake('old'), (fn) => queued.push(fn));
    const p2 = jobs.run('k', () => fake('new'), (fn) => queued.push(fn));
    queued[1](); queued[0](); // newer finishes first, stale one later
    expect((await p2)?.layout.module).toBe('new');
    expect(await p1).toBeNull();
  });
});

describe('store', () => {
  beforeEach(() => resetStore());

  test('setTop re-roots the design without mutating it; unknown or same top is a no-op', async () => {
    loadDesign(design, 'demo');
    const child = Object.keys(design.modules).find((n) => n !== design.top)!;
    setTop(child); await tick();
    expect(getState().design?.top).toBe(child);
    expect(getState().focus).toEqual([child]);
    expect(getState().layouts[child]).toBeDefined();
    expect(design.top).not.toBe(child);
    const d = getState().design;
    setTop('no-such-module'); setTop(child);
    expect(getState().design).toBe(d);
  });

  test('load computes the top layout once; stale duplicate requests are dropped', async () => {
    loadDesign(design, 'demo');
    void ensureLayout(['top'], true); void ensureLayout(['top'], true);
    await tick();
    const s = getState();
    expect(s.layouts.top).toBeDefined();
    expect(s.layoutGeneration).toBe(1);
  });

  test('expanding a module never recomputes the parent layout and is independent per instance', async () => {
    loadDesign(design, 'demo');
    await tick();
    const topBefore = getState().layouts.top;
    toggleExpand(['top', 'compute']);
    await tick();
    toggleExpand(['top', 'compute', 'pe0']);
    await tick();
    const s = getState();
    expect(s.layouts.top).toBe(topBefore);
    expect(s.expanded['top/compute/pe0']).toBe(true);
    expect(s.expanded['top/compute/pe1']).toBeUndefined();
    expect(s.layouts['top/compute/pe0']).toBeDefined();
    expect(s.layouts['top/compute/pe1']).toBeUndefined();
    // collapse + re-expand reuses the cached internal layout object
    const inner = s.layouts['top/compute/pe0'];
    toggleExpand(['top', 'compute', 'pe0']); toggleExpand(['top', 'compute', 'pe0']);
    await tick();
    expect(getState().layouts['top/compute/pe0']).toBe(inner);
  });

  test('selection and camera do not touch geometry', async () => {
    loadDesign(design, 'demo');
    await tick();
    const before = getState().layouts.top;
    select({ kind: 'cell', path: ['top', 'compute'] });
    select({ kind: 'net', path: ['top'], net: 'clk' });
    expect(getState().layouts.top).toBe(before);
    expect(getState().layoutGeneration).toBe(1);
  });

  test('a rejected layout file keeps the previous valid layout and reports errors', async () => {
    loadDesign(design, 'demo');
    applyLayoutText(demoYaml);
    await tick();
    const withRules = getState().layouts.top;
    expect(withRules.nodes.compute.w).toBe(640);
    const ok = applyLayoutText(conflictYaml);
    await tick();
    expect(ok).toBe(false);
    expect(getState().layouts.top).toBe(withRules);
    expect(getState().layoutDiagnostics.filter((d) => d.severity === 'error').length).toBeGreaterThan(5);
  });
});
