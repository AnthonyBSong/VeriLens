// Constraint stage: nudges the layered candidate so required leftOf/above/alignY
// rules hold, keeps preferred ones when that costs nothing required, and reports
// exactly what was violated and by how much. Runs before routing, so wires are
// always routed against final geometry.
import type { Constraint } from './dsl';
import type { GNode } from './layered';
import type { Diagnostic } from './types';

export interface SolveInput {
  nodes: Map<string, GNode>;
  layers: string[][];
  channels: { x0: number; x1: number }[];
  constraints: Constraint[];
  nodeGap: number;
  scope: string;
}

const EPS = 1e-6;

export function solveConstraints(input: SolveInput): { ok: boolean; diagnostics: Diagnostic[] } {
  const { nodes, layers, channels, constraints, nodeGap, scope } = input;
  const diagnostics: Diagnostic[] = [];
  const n = (id: string) => nodes.get(id)!;
  const right = (id: string) => n(id).x + n(id).w;
  const bottom = (id: string) => n(id).y + n(id).h;
  const centerY = (id: string) => n(id).y + n(id).h / 2;
  const inLayerIndex = (id: string) => layers[n(id).layer].indexOf(id);

  const violation = (c: Constraint): number => {
    if (c.kind !== 'alignY') return c.kind === 'leftOf' ? right(c.a) + c.gap - n(c.b).x : bottom(c.a) + c.gap - n(c.b).y;
    const cs = c.nodes.map((id) => centerY(id));
    const mean = cs.reduce((s, v) => s + v, 0) / cs.length;
    return Math.max(...cs.map((v) => Math.abs(v - mean)));
  };

  const shiftLayersFrom = (layer: number, dx: number) => {
    for (let l = layer; l < layers.length; l++) for (const id of layers[l]) n(id).x += dx;
    // channels[l+1] is after layer l; the channel just before `layer` grows, later ones move
    channels[layer].x1 += dx;
    for (let c = layer + 1; c < channels.length; c++) { channels[c].x0 += dx; channels[c].x1 += dx; }
  };
  // dummies (wire lanes) never block a nudge; they are re-settled between real nodes afterwards
  const real = (l: string[]) => l.filter((id) => !n(id).dummy);
  const moveDown = (id: string, y: number) => {
    const arr = real(layers[n(id).layer]);
    const i = arr.indexOf(id);
    n(id).y = y;
    for (let k = i + 1; k < arr.length; k++) {
      const prev = n(arr[k - 1]), cur = n(arr[k]);
      cur.y = Math.max(cur.y, prev.y + prev.h + nodeGap);
    }
  };
  const moveUp = (id: string, y: number) => {
    const arr = real(layers[n(id).layer]);
    const i = arr.indexOf(id);
    n(id).y = y;
    for (let k = i - 1; k >= 0; k--) {
      const next = n(arr[k + 1]), cur = n(arr[k]);
      cur.y = Math.min(cur.y, next.y - nodeGap - cur.h);
    }
  };
  const snapshot = () => ({ pos: new Map([...nodes].map(([id, g]) => [id, { x: g.x, y: g.y }])), ch: channels.map((c) => ({ ...c })) });
  const restore = (s: ReturnType<typeof snapshot>) => {
    for (const [id, p] of s.pos) { n(id).x = p.x; n(id).y = p.y; }
    s.ch.forEach((c, i) => { channels[i].x0 = c.x0; channels[i].x1 = c.x1; });
  };
  const required = constraints.filter((c) => c.strength === 'required');
  const requiredOk = () => required.every((c) => violation(c) <= EPS);
  const err = (message: string, ids: string[]) => diagnostics.push({ severity: 'error', scope, constraints: ids, message });

  // apply one positional rule; returns false if it is unsatisfiable by nudging
  const apply = (c: Constraint, ok: () => boolean): boolean => {
    const v = violation(c);
    if (v <= EPS) return true;
    if (c.kind === 'alignY') { alignGroup(c, ok); return violation(c) < v - EPS; }
    if (c.kind === 'leftOf') {
      if (n(c.a).layer >= n(c.b).layer) return false;
      shiftLayersFrom(n(c.b).layer, v);
      return true;
    }
    if (c.kind === 'above') {
      if (n(c.a).layer === n(c.b).layer && inLayerIndex(c.a) > inLayerIndex(c.b)) return false;
      // prefer pushing b down; if that breaks a required rule, pull a up instead
      const s = snapshot();
      moveDown(c.b, bottom(c.a) + c.gap);
      if (ok()) return true;
      restore(s);
      moveUp(c.a, n(c.b).y - c.gap - n(c.a).h);
      return true;
    }
    return false;
  };
  const alignGroup = (c: Extract<Constraint, { kind: 'alignY' }>, ok: () => boolean): void => {
    const mean = c.nodes.map((id) => centerY(id)).reduce((s, v) => s + v, 0) / c.nodes.length;
    for (const id of c.nodes) {
      const g = n(id);
      const want = mean - g.h / 2;
      const arr = real(layers[g.layer]);
      const i = arr.indexOf(id);
      const lo = i > 0 ? bottom(arr[i - 1]) + nodeGap : -Infinity;
      const hi = i < arr.length - 1 ? n(arr[i + 1]).y - nodeGap - g.h : Infinity;
      const before = g.y;
      g.y = Math.min(hi, Math.max(lo, want));
      if (!ok()) g.y = before;
    }
  };

  // 1. required: iterate to a fixed point
  let stable = false;
  for (let iter = 0; iter < 40 && !stable; iter++) {
    stable = true;
    for (const c of required) {
      if (violation(c) <= EPS) continue;
      stable = false;
      if (!apply(c, requiredOk)) {
        const why = c.kind === 'alignY' ? `nodes ${c.nodes.join(', ')} cannot share a vertical center without overlapping or breaking another required rule`
          : c.kind === 'leftOf' ? `'${c.a}' is not layered before '${c.b}' (feedback or conflicting rule)`
          : `'${c.a}' is ordered below '${c.b}' in the same layer`;
        err(`required ${c.kind} '${c.id}' cannot be satisfied: ${why}`, [c.id]);
        return { ok: false, diagnostics };
      }
    }
  }
  if (!stable) {
    const bad = required.filter((c) => violation(c) > EPS).map((c) => c.id);
    err(`required constraints did not converge (mutually pushing): ${bad.join(', ')}`, bad);
    return { ok: false, diagnostics };
  }

  // 2. preferred, heaviest first; each is kept only if all required rules still hold
  const preferred = constraints.filter((c) => c.strength === 'preferred').sort((a, b) => b.weight - a.weight);
  const kept: Constraint[] = []; // heavier preferred rules already satisfied must not be broken by lighter ones
  for (const c of preferred) {
    if (violation(c) > EPS) {
      const s = snapshot();
      const ok = () => requiredOk() && kept.every((k) => violation(k) <= EPS);
      const applied = apply(c, ok);
      if (!applied || !ok()) restore(s);
    }
    if (violation(c) <= EPS) kept.push(c);
  }
  for (const c of preferred) {
    const v = violation(c);
    if (v > EPS) diagnostics.push({ severity: 'warning', scope, constraints: [c.id], amount: Math.round(v * 10) / 10, message: `preferred ${c.kind} '${c.id}' relaxed by ${Math.round(v * 10) / 10} units` });
  }

  // 3. final verification of required rules (belt and braces) and in-layer overlap
  for (const c of required) if (violation(c) > EPS) err(`required ${c.kind} '${c.id}' violated after solving by ${violation(c)}`, [c.id]);
  for (const l of layers.map(real)) for (let k = 1; k < l.length; k++) {
    if (n(l[k]).y < bottom(l[k - 1]) - EPS) err(`nodes '${l[k - 1]}' and '${l[k]}' overlap after solving`, []);
  }
  settleDummies(layers, nodes, nodeGap);
  return { ok: !diagnostics.some((d) => d.severity === 'error'), diagnostics };
}

/** Re-settle dummy lanes so they never sit inside a real node; lanes that already fit keep their position. */
export function settleDummies(layers: string[][], nodes: Map<string, GNode>, gap: number) {
  for (const l of layers) {
    let i = 0;
    while (i < l.length) {
      if (!nodes.get(l[i])!.dummy) { i++; continue; }
      let j = i;
      while (j < l.length && nodes.get(l[j])!.dummy) j++;
      const prev = i > 0 ? nodes.get(l[i - 1])! : undefined;
      const next = j < l.length ? nodes.get(l[j])! : undefined;
      const lo = prev ? prev.y + prev.h : -Infinity;
      const hi = next ? next.y : Infinity;
      const run = l.slice(i, j).map((id) => nodes.get(id)!);
      const half = gap / 2;
      const fits = run.every((d, k) => d.y >= lo + half && d.y <= hi - half && (k === 0 || d.y >= run[k - 1].y));
      if (!fits) {
        if (prev && next) { const step = (hi - lo) / (run.length + 1); run.forEach((d, k) => { d.y = lo + step * (k + 1); }); }
        else if (prev) run.forEach((d, k) => { d.y = lo + half + gap * k; });
        else if (next) run.forEach((d, k) => { d.y = hi - half - gap * (run.length - 1 - k); });
      }
      i = j;
    }
  }
}
