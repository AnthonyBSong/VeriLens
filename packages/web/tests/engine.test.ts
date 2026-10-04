import { readFileSync } from 'node:fs';
import { describe, expect, test } from 'vitest';
import { validateDesign, type Design } from '../src/model/design';
import { layoutModule } from '../src/layout/engine';
import { parseLayoutFile } from '../src/layout/dsl';
import type { ModuleLayout, Point } from '../src/layout/types';

const design = validateDesign(JSON.parse(readFileSync(new URL('../samples/demo.design.json', import.meta.url), 'utf8')));
const demoYaml = readFileSync(new URL('../samples/demo.layout.yaml', import.meta.url), 'utf8');
const conflictYaml = readFileSync(new URL('../samples/demo-conflict.layout.yaml', import.meta.url), 'utf8');

const lay = (path: string[], yaml: string | null): ModuleLayout => {
  const file = yaml ? parseLayoutFile(yaml, design).file : null;
  if (yaml && !file) throw new Error('layout file did not validate');
  const mod = design.modules[path.length === 1 ? design.top : path[path.length - 1] === 'pe0' || path[path.length - 1] === 'pe1' ? 'pe' : path[path.length - 1]];
  return layoutModule(design, path, mod, file);
};
const box = (l: ModuleLayout, id: string) => { const n = l.nodes[id]; return { left: n.x, right: n.x + n.w, top: n.y, bottom: n.y + n.h, cy: n.y + n.h / 2 }; };

describe('automatic layered layout', () => {
  test('is deterministic', () => {
    const a = lay(['top'], null), b = lay(['top'], null);
    expect(JSON.stringify(a)).toBe(JSON.stringify(b));
  });

  test('never changes connectivity', () => {
    const before = JSON.stringify(design);
    lay(['top'], demoYaml); lay(['top', 'compute'], demoYaml);
    expect(JSON.stringify(design)).toBe(before);
  });

  test.each(['top', 'input_fifo', 'controller', 'compute', 'output_fifo'])('%s: nodes do not overlap and wires end on port anchors', (name) => {
    const l = lay(name === 'top' ? ['top'] : ['top', name], null);
    expect(l.ok).toBe(true);
    const ns = Object.values(l.nodes);
    for (let i = 0; i < ns.length; i++) for (let j = i + 1; j < ns.length; j++) {
      const a = ns[i], b = ns[j];
      const overlap = a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
      expect(overlap, `${a.id} overlaps ${b.id}`).toBe(false);
    }
    const anchors = new Set<string>();
    for (const n of ns) for (const p of Object.values(n.ports)) anchors.add(`${Math.round(n.x + p.x)},${Math.round(n.y + p.y)}`);
    for (const w of l.wires) {
      const ends = w.polylines.flatMap((pl) => [pl[0], pl[pl.length - 1]]);
      const onAnchor = ends.filter((p) => anchors.has(`${Math.round(p.x)},${Math.round(p.y)}`));
      expect(onAnchor.length, `wire ${w.net} touches no port anchor`).toBeGreaterThan(0);
    }
  });

  test('junction dots appear only where a net branches, never at crossings', () => {
    const l = lay(['top', 'compute'], null);
    const onSegment = (p: Point, a: Point, b: Point) => {
      const eps = 1e-6;
      if (Math.abs(a.x - b.x) < eps) return Math.abs(p.x - a.x) < eps && p.y > Math.min(a.y, b.y) + eps && p.y < Math.max(a.y, b.y) - eps;
      if (Math.abs(a.y - b.y) < eps) return Math.abs(p.y - a.y) < eps && p.x > Math.min(a.x, b.x) + eps && p.x < Math.max(a.x, b.x) - eps;
      return false;
    };
    let total = 0;
    for (const w of l.wires) for (const j of w.junctions) {
      total++;
      let ends = 0;
      for (const pl of w.polylines) {
        for (const e of [pl[0], pl[pl.length - 1]]) if (Math.abs(e.x - j.x) < 1e-6 && Math.abs(e.y - j.y) < 1e-6) ends++;
        for (let i = 1; i < pl.length; i++) if (onSegment(j, pl[i - 1], pl[i])) ends += 2;
      }
      expect(ends, `junction of ${w.net} at ${j.x},${j.y}`).toBeGreaterThanOrEqual(3);
      // a different net passing through the same point must not exist as a junction owner
      for (const other of l.wires) if (other !== w) for (const oj of other.junctions) expect(oj.x === j.x && oj.y === j.y).toBe(false);
    }
    expect(total).toBeGreaterThan(0);
  });

  test('constants, unconnected ports and black boxes are explicit', () => {
    const l = lay(['top', 'compute'], null);
    expect(Object.values(l.nodes).some((n) => n.kind === 'const' && n.text === "12'd0")).toBe(true);
    const bb: Design = { version: 1, top: 't', modules: { t: { name: 't', ports: [{ name: 'a', direction: 'input', width: 1 }], nets: [{ id: 'a', width: 1 }, { id: 'q', width: 1 }],
      cells: [{ id: 'u', kind: 'instance', module: 'missing', resolved: false, ports: [{ name: 'x', direction: 'input', width: 1 }, { name: 'y', direction: 'output', width: 1 }], connections: { x: [{ net: 'a' }] } }] } } };
    const bl = layoutModule(bb, ['t'], bb.modules.t, null);
    expect(bl.nodes.u.ports.y.open).toBe(true);
    expect(bl.diagnostics.some((d) => d.message.includes('black box'))).toBe(true);
  });

  test('repeated instances of one module get identical, independent layouts', () => {
    const a = lay(['top', 'compute', 'pe0'], demoYaml), b = lay(['top', 'compute', 'pe1'], demoYaml);
    expect(a.scope).toBe('top/compute/pe0'); expect(b.scope).toBe('top/compute/pe1');
    expect(JSON.stringify({ ...a, scope: '' })).toBe(JSON.stringify({ ...b, scope: '' }));
  });
});

describe('layout DSL', () => {
  test('demo rules validate and required constraints are satisfied', () => {
    const l = lay(['top'], demoYaml);
    expect(l.ok).toBe(true);
    expect(l.diagnostics.filter((d) => d.severity === 'error')).toEqual([]);
    expect(box(l, 'input_fifo').right + 80).toBeLessThanOrEqual(box(l, 'compute').left + 1e-6);
    expect(box(l, 'compute').right + 80).toBeLessThanOrEqual(box(l, 'output_fifo').left + 1e-6);
    // preferred rules held here too
    expect(Math.abs(box(l, 'input_fifo').cy - box(l, 'compute').cy)).toBeLessThan(1e-6);
    expect(Math.abs(box(l, 'output_fifo').cy - box(l, 'compute').cy)).toBeLessThan(1e-6);
    expect(box(l, 'controller').bottom + 40).toBeLessThanOrEqual(box(l, 'compute').top + 1e-6);
    const c = lay(['top', 'compute'], demoYaml);
    expect(box(c, 'pe0').right + 60).toBeLessThanOrEqual(box(c, 'pe1').left + 1e-6);
  });

  test('frames and port order are applied', () => {
    const l = lay(['top'], demoYaml);
    expect(l.nodes.compute.w).toBe(640); expect(l.nodes.compute.h).toBe(420);
    const west = ['data_in', 'valid_in', 'start', 'mode', 'coeff_sel', 'ready_in', 'clk', 'rst_n'].map((p) => l.nodes.compute.ports[p].y);
    for (let i = 1; i < west.length; i++) expect(west[i]).toBeGreaterThan(west[i - 1]);
    const east = ['data_out', 'valid_out', 'ready_out', 'done'].map((p) => l.nodes.compute.ports[p].y);
    for (let i = 1; i < east.length; i++) expect(east[i]).toBeGreaterThan(east[i - 1]);
  });

  test('required rule against signal flow still wins (circuit edge becomes feedback)', () => {
    const yaml = `version: 1\nscopes:\n  - scope: top\n    constraints:\n      - { id: rev, kind: leftOf, a: output_fifo, b: input_fifo, gap: 50, strength: required }\n`;
    const l = lay(['top'], yaml);
    expect(l.ok).toBe(true);
    expect(box(l, 'output_fifo').right + 50).toBeLessThanOrEqual(box(l, 'input_fifo').left + 1e-6);
  });

  test('preferred rules that cannot hold are reported with the amount', () => {
    const yaml = `version: 1\nscopes:\n  - scope: top\n    constraints:\n      - { id: ctl-above, kind: above, a: controller, b: compute, gap: 40, strength: required }\n      - { id: same-row, kind: alignY, nodes: [controller, compute], strength: preferred, weight: 2 }\n`;
    const l = lay(['top'], yaml);
    expect(l.ok).toBe(true);
    const w = l.diagnostics.find((d) => d.constraints?.includes('same-row'));
    expect(w?.severity).toBe('warning');
    expect(w?.amount).toBeGreaterThan(0);
  });

  test('conflicting file is rejected with scope/constraint ids in every message', () => {
    const r = parseLayoutFile(conflictYaml, design);
    expect(r.file).toBeNull();
    const msgs = r.diagnostics.map((d) => d.message);
    expect(msgs.some((m) => m.includes('duplicate constraint id'))).toBe(true);
    expect(msgs.some((m) => m.includes("unknown node 'nonexistent_block'"))).toBe(true);
    expect(msgs.some((m) => m.includes("unsupported kind 'sameRow'"))).toBe(true);
    expect(msgs.some((m) => m.includes('gap must be a non-negative number'))).toBe(true);
    expect(msgs.some((m) => m.includes('form a cycle: a-left-of-b -> b-left-of-a'))).toBe(true);
    expect(msgs.some((m) => m.includes("unknown port 'no_such_port'"))).toBe(true);
    expect(msgs.some((m) => m.includes('top/does_not_exist'))).toBe(true);
    const cyc = r.diagnostics.find((d) => d.message.includes('cycle'));
    expect(cyc?.scope).toBe('top');
    expect(cyc?.constraints).toEqual(['a-left-of-b', 'b-left-of-a']);
  });

  test('contradictory required alignY + above is an error', () => {
    const yaml = `version: 1\nscopes:\n  - scope: top\n    constraints:\n      - { id: x, kind: above, a: controller, b: compute, gap: 10, strength: required }\n      - { id: y, kind: alignY, nodes: [controller, compute], strength: required }\n`;
    const r = parseLayoutFile(yaml, design);
    expect(r.file).toBeNull();
    expect(r.diagnostics[0].constraints).toEqual(['y', 'x']);
  });

  test('unsupported defaults are rejected', () => {
    const r = parseLayoutFile('version: 1\ndefaults: { direction: down }\n', design);
    expect(r.file).toBeNull();
    expect(r.diagnostics[0].message).toContain('direction');
  });
});
