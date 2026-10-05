// Deterministic layered (Sugiyama-style) placement. Heuristic, not optimal.
// Input: sized nodes + directed edges (+ layout-only constraint edges).
// Output: layer index, in-layer order, and x/y for every node including dummies.
//
// ponytail: single-pass barycenter ordering and priority placement. Upgrade path
// is network-simplex layering + Brandes-Köpf placement if drawings get too tall.

import type { NodeGeom, Point } from './types';

export interface GNode extends NodeGeom {
  /** Dummy nodes carry a long edge across a layer: zero size, one per (net, layer). */
  dummy?: boolean;
  net?: string;
}

export interface GEdge { from: string; to: string; net: string }

export interface LayerEdge { id: string; a: string; b: string; strength: 'required' | 'preferred' }
export interface OrderRule { id: string; a: string; b: string; strength: 'required' | 'preferred' }

export interface LayeredInput {
  nodes: GNode[];
  edges: GEdge[];
  /** leftOf constraints: a must be in an earlier layer than b. */
  layerRules: LayerEdge[];
  /** above constraints: within a layer, a before b. */
  orderRules: OrderRule[];
  nodeGap: number;
  layerGap: number;
  trackPitch: number;
  /** Channel usage per channel index (-1..L-1) is supplied after layering via callback. */
}

export interface LayeredResult {
  nodes: Map<string, GNode>;
  layers: string[][];
  /** channels[c+1] is the free vertical band between layer c and c+1; channels[0] is left of layer 0. */
  channels: { x0: number; x1: number }[];
  /** Circuit edges that could not be layered forward (feedback). */
  feedback: GEdge[];
  /** Preferred leftOf rules dropped because they would create a cycle. */
  droppedRules: string[];
  /** accepted forward edges, deduplicated by (from,to) */
  forward: GEdge[];
  dummies: Map<string, GNode>; // key `${net}:${layer}`
}

function reachable(adj: Map<string, Set<string>>, from: string, to: string): boolean {
  if (from === to) return true;
  const seen = new Set<string>([from]);
  const stack = [from];
  while (stack.length) {
    const n = stack.pop()!;
    for (const m of adj.get(n) ?? []) {
      if (m === to) return true;
      if (!seen.has(m)) { seen.add(m); stack.push(m); }
    }
  }
  return false;
}

export interface ChannelUsage { (result: { nodes: Map<string, GNode>; layers: string[][]; forward: GEdge[]; feedback: GEdge[] }): number[] }

export function layered(input: LayeredInput, channelUsage: ChannelUsage): LayeredResult {
  const nodes = new Map<string, GNode>(input.nodes.map((n) => [n.id, { ...n, ports: { ...n.ports } }]));
  const adj = new Map<string, Set<string>>();
  const addEdge = (a: string, b: string) => { if (!adj.has(a)) adj.set(a, new Set()); adj.get(a)!.add(b); };

  // 1. required layer rules first (validated acyclic by the DSL layer), then circuit edges, then preferred rules
  for (const r of input.layerRules) if (r.strength === 'required') addEdge(r.a, r.b);
  const forward: GEdge[] = [];
  const feedback: GEdge[] = [];
  const seenPair = new Set<string>();
  // Heavier bundles are inserted first so that, when two nodes feed each other,
  // the thinner direction is the one that becomes feedback. Stable on ties.
  const pairWeight = new Map<string, number>();
  for (const e of input.edges) { const k = e.from + '\u0000' + e.to; pairWeight.set(k, (pairWeight.get(k) ?? 0) + 1); }
  const ordered = input.edges.map((e, i) => ({ e, i, w: pairWeight.get(e.from + '\u0000' + e.to)! }))
    .sort((a, b) => b.w - a.w || a.i - b.i).map((x) => x.e);
  for (const e of ordered) {
    const key = e.from + '\u0000' + e.to;
    if (e.from === e.to) { feedback.push(e); continue; }
    if (seenPair.has(key)) { (adj.get(e.from)?.has(e.to) ? forward : feedback).push(e); continue; }
    seenPair.add(key);
    if (reachable(adj, e.to, e.from)) feedback.push(e);
    else { addEdge(e.from, e.to); forward.push(e); }
  }
  const droppedRules: string[] = [];
  for (const r of input.layerRules) {
    if (r.strength !== 'preferred') continue;
    if (reachable(adj, r.b, r.a)) droppedRules.push(r.id); else addEdge(r.a, r.b);
  }

  // 2. longest-path layering (inputs left). Outputs and consts are re-placed afterwards.
  const layerOf = new Map<string, number>();
  const ids = [...nodes.keys()];
  const preds = new Map<string, string[]>();
  for (const [a, bs] of adj) for (const b of bs) { if (!preds.has(b)) preds.set(b, []); preds.get(b)!.push(a); }
  const visiting = new Set<string>();
  const layer = (id: string): number => {
    const cached = layerOf.get(id);
    if (cached !== undefined) return cached;
    if (visiting.has(id)) return 0; // cannot happen for a DAG; defensive
    visiting.add(id);
    let l = 0;
    for (const p of preds.get(id) ?? []) l = Math.max(l, layer(p) + 1);
    visiting.delete(id);
    layerOf.set(id, l);
    return l;
  };
  for (const id of ids) layer(id);
  let maxLayer = 0;
  for (const [id, l] of layerOf) if (nodes.get(id)!.kind !== 'out') maxLayer = Math.max(maxLayer, l);
  const hasCells = ids.some((id) => nodes.get(id)!.kind === 'cell' || nodes.get(id)!.kind === 'const');
  for (const id of ids) if (nodes.get(id)!.kind === 'out') layerOf.set(id, hasCells ? maxLayer + 1 : Math.max(1, maxLayer));
  // constants sit right before their sink (ALAP) so they do not stretch the first layer
  for (const id of ids) {
    const n = nodes.get(id)!;
    if (n.kind !== 'const') continue;
    const succ = [...(adj.get(id) ?? [])].map((s) => layerOf.get(s)!);
    if (succ.length) layerOf.set(id, Math.max(0, Math.min(...succ) - 1));
  }
  for (const id of ids) nodes.get(id)!.layer = layerOf.get(id)!;
  const L = Math.max(...ids.map((id) => layerOf.get(id)!), 0) + 1;

  // 3. dummies for long edges, one per (net, layer)
  const dummies = new Map<string, GNode>();
  const ordEdges: { from: string; to: string }[] = [];
  for (const e of forward) {
    const lf = layerOf.get(e.from)!, lt = layerOf.get(e.to)!;
    let prev = e.from;
    for (let l = lf + 1; l < lt; l++) {
      const key = `${e.net}:${l}`;
      let d = dummies.get(key);
      if (!d) {
        d = { id: `$d:${key}`, kind: 'cell', x: 0, y: 0, w: 0, h: 0, layer: l, ports: {}, dummy: true, net: e.net };
        dummies.set(key, d); nodes.set(d.id, d);
      }
      ordEdges.push({ from: prev, to: d.id });
      prev = d.id;
    }
    ordEdges.push({ from: prev, to: e.to });
  }

  // 4. ordering: barycenter sweeps with above-rules enforced after every sweep
  const layers: string[][] = Array.from({ length: L }, () => []);
  for (const [id, n] of nodes) layers[n.layer].push(id);
  const succs = new Map<string, string[]>(), predsO = new Map<string, string[]>();
  for (const e of ordEdges) {
    if (!succs.has(e.from)) succs.set(e.from, []); succs.get(e.from)!.push(e.to);
    if (!predsO.has(e.to)) predsO.set(e.to, []); predsO.get(e.to)!.push(e.from);
  }
  const pos = new Map<string, number>();
  const reindex = () => layers.forEach((l) => l.forEach((id, i) => pos.set(id, i)));
  const applyOrderRules = () => {
    for (const strength of ['preferred', 'required'] as const) {
      for (const r of input.orderRules) {
        if (r.strength !== strength) continue;
        const la = layerOf.get(r.a), lb = layerOf.get(r.b);
        if (la === undefined || la !== lb) continue;
        const arr = layers[la];
        const ia = arr.indexOf(r.a), ib = arr.indexOf(r.b);
        if (ia > ib) { arr.splice(ia, 1); arr.splice(ib, 0, r.a); }
      }
    }
    reindex();
  };
  reindex();
  const sweep = (down: boolean) => {
    const range = down ? layers.map((_, i) => i).slice(1) : layers.map((_, i) => i).slice(0, -1).reverse();
    for (const l of range) {
      const nb = down ? predsO : succs;
      const bary = new Map<string, number>();
      layers[l].forEach((id, i) => {
        const ns = nb.get(id) ?? [];
        bary.set(id, ns.length ? ns.reduce((s, m) => s + pos.get(m)!, 0) / ns.length : i);
      });
      layers[l].sort((a, b) => bary.get(a)! - bary.get(b)! || pos.get(a)! - pos.get(b)!);
      reindex();
    }
    applyOrderRules();
  };
  for (let i = 0; i < 4; i++) { sweep(true); sweep(false); }

  // 5. x placement: layer widths + channel widths from track usage
  const usage = channelUsage({ nodes, layers, forward, feedback });
  const layerW = layers.map((l) => Math.max(0, ...l.map((id) => nodes.get(id)!.w)));
  const channels: { x0: number; x1: number }[] = [];
  const chanW = (c: number) => Math.max(input.layerGap, ((usage[c + 1] ?? 0) + 1) * input.trackPitch);
  let x = 0;
  channels.push({ x0: 0, x1: chanW(-1) });
  x = chanW(-1);
  const layerX: number[] = [];
  for (let l = 0; l < L; l++) {
    layerX.push(x);
    x += layerW[l];
    channels.push({ x0: x, x1: x + chanW(l) });
    x += chanW(l);
  }
  for (let l = 0; l < L; l++) for (const id of layers[l]) {
    const n = nodes.get(id)!;
    n.x = n.kind === 'in' ? layerX[l] + layerW[l] - n.w : n.kind === 'out' ? layerX[l] : layerX[l] + (layerW[l] - n.w) / 2;
  }

  // 6. y placement: initial stacking, then priority alignment sweeps toward neighbour ports
  const gap = input.nodeGap;
  for (const l of layers) { let y = 0; for (const id of l) { const n = nodes.get(id)!; n.y = y; y += n.h + gap; } }
  const anchorY = (n: GNode, side: 'west' | 'east'): number => {
    const ps = Object.values(n.ports).filter((p) => p.side === side);
    return ps.length ? ps.reduce((s, p) => s + p.y, 0) / ps.length : n.h / 2;
  };
  const desiredY = (id: string, nb: string[], down: boolean): number | undefined => {
    if (!nb.length) return undefined;
    const n = nodes.get(id)!;
    const ys = nb.map((m) => { const mn = nodes.get(m)!; return mn.y + anchorY(mn, down ? 'east' : 'west'); });
    ys.sort((a, b) => a - b);
    const med = ys.length % 2 ? ys[(ys.length - 1) / 2] : (ys[ys.length / 2 - 1] + ys[ys.length / 2]) / 2;
    return med - anchorY(n, down ? 'west' : 'east');
  };
  const placeLayer = (l: string[], desired: Map<string, number>, nb: Map<string, string[]>) => {
    // priority: nodes with more connections are placed first at their desired y; others slot around them
    const prio = [...l].sort((a, b) => (nb.get(b)?.length ?? 0) - (nb.get(a)?.length ?? 0) || l.indexOf(a) - l.indexOf(b));
    const placed = new Set<string>();
    for (const id of prio) {
      const i = l.indexOf(id);
      const n = nodes.get(id)!;
      let lo = -Infinity, hi = Infinity;
      let need = 0;
      for (let k = i - 1; k >= 0; k--) { const m = nodes.get(l[k])!; if (placed.has(l[k])) { lo = m.y + m.h + gap + need; break; } need += m.h + gap; }
      need = 0;
      for (let k = i + 1; k < l.length; k++) { const m = nodes.get(l[k])!; if (placed.has(l[k])) { hi = m.y - gap - need - n.h; break; } need += m.h + gap; }
      const want = desired.get(id) ?? n.y;
      n.y = Math.min(hi, Math.max(lo, want));
      placed.add(id);
    }
  };
  for (let iter = 0; iter < 3; iter++) {
    for (let l = 1; l < L; l++) {
      const desired = new Map<string, number>();
      for (const id of layers[l]) { const d = desiredY(id, predsO.get(id) ?? [], true); if (d !== undefined) desired.set(id, d); }
      placeLayer(layers[l], desired, predsO);
    }
    for (let l = L - 2; l >= 0; l--) {
      const desired = new Map<string, number>();
      for (const id of layers[l]) { const d = desiredY(id, succs.get(id) ?? [], false); if (d !== undefined) desired.set(id, d); }
      placeLayer(layers[l], desired, succs);
    }
  }
  straightenDummies(layers, nodes, predsO, succs, gap);
  normalize(nodes);
  return { nodes, layers, channels, feedback, droppedRules, forward, dummies };
}

/**
 * Long-edge lanes (dummies), boundary terminals and constants follow their
 * neighbours' y when a free band exists there, even if barycenter ordering put
 * them elsewhere. Cells never move; a lane landing inside a cell snaps to just
 * outside it. Layer order is then re-derived from y so routing stays consistent.
 */
function straightenDummies(layers: string[][], nodes: Map<string, GNode>, preds: Map<string, string[]>, succs: Map<string, string[]>, gap: number) {
  const movable = (m: GNode) => !!m.dummy || m.kind !== 'cell';
  const centerOf = (id: string) => { const m = nodes.get(id)!; return m.y + m.h / 2; };
  // pass 0 sweeps left->right following predecessors (lanes run straight from their source);
  // pass 1 sweeps right->left for source-side movables (input terminals, constants) following successors
  for (let pass = 0; pass < 2; pass++) {
    const order = pass === 0 ? layers : [...layers].reverse();
    for (const l of order) {
      const cells = l.map((id) => nodes.get(id)!).filter((m) => !movable(m));
      for (const id of l) {
        const d = nodes.get(id)!;
        if (!movable(d)) continue;
        const ps = preds.get(id) ?? [];
        const nb = pass === 0 ? ps : (ps.length ? [] : succs.get(id) ?? []);
        if (!nb.length) continue;
        let y = nb.reduce((s, m) => s + centerOf(m), 0) / nb.length - d.h / 2;
        for (const r of cells) {
          if (y + d.h > r.y - gap / 2 && y < r.y + r.h + gap / 2) { y = Math.abs(y - r.y) < Math.abs(y - (r.y + r.h)) ? r.y - gap / 2 - d.h : r.y + r.h + gap / 2; }
        }
        d.y = y;
      }
      l.sort((a, b) => nodes.get(a)!.y - nodes.get(b)!.y || l.indexOf(a) - l.indexOf(b));
      // stack movable nodes so they do not overlap each other (cells stay put)
      let cursor = -Infinity;
      for (const id of l) {
        const m = nodes.get(id)!;
        if (!movable(m)) { cursor = m.y + m.h + gap / 2; continue; }
        m.y = Math.max(m.y, cursor);
        cursor = m.y + m.h + (m.dummy ? gap / 2 : gap / 2);
      }
    }
  }
}

/** Shift all nodes so the minimum y is 0. x is left untouched. */
export function normalize(nodes: Map<string, GNode>) {
  let minY = Infinity;
  for (const n of nodes.values()) minY = Math.min(minY, n.y);
  if (!Number.isFinite(minY)) return;
  for (const n of nodes.values()) n.y -= minY;
}

export const absAnchor = (n: NodeGeom, port: string): Point | undefined => {
  const a = n.ports[port];
  return a ? { x: n.x + a.x, y: n.y + a.y } : undefined;
};
