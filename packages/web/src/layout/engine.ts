// Layout pipeline for one module instance (scope):
//   1. validate/normalize rules for this scope   2. layered candidate
//   3. constraint solve + non-overlap             4. route wires on final geometry
//   5. verify + diagnostics
// Pure and deterministic: same design + rules -> identical ModuleLayout.
import type { Design, InstancePath, ModuleDef } from '../model/design';
import { buildNetIndex, cellPorts, isConst, isExpandable, pathKey } from '../model/design';
import { solveConstraints } from './constraints';
import type { LayoutFile, ScopeRules } from './dsl';
import type { GEdge, GNode } from './layered';
import { layered } from './layered';
import { channelUsage, route, type RouteNet } from './router';
import { sizeCell, sizeConst, sizeTerminal } from './symbols';
import type { Diagnostic, LayoutDefaults, ModuleLayout, NodeGeom, Side } from './types';
import { DEFAULTS } from './types';

const TRACK_PITCH = 10;
const PADDING = 24;

export function scopeRulesFor(file: LayoutFile | null, scope: string): ScopeRules | undefined {
  return file?.scopes.find((s) => s.scope === scope);
}

export function layoutModule(design: Design, path: InstancePath, mod: ModuleDef, file: LayoutFile | null): ModuleLayout {
  const scope = pathKey(path);
  const defaults: LayoutDefaults = file?.defaults ?? DEFAULTS;
  const rules = scopeRulesFor(file, scope);
  const diagnostics: Diagnostic[] = [];
  const netIndex = buildNetIndex(design, mod);
  const netWidth = new Map(mod.nets.map((n) => [n.id, n.width]));

  // --- nodes
  const gnodes: GNode[] = [];
  const push = (g: Omit<NodeGeom, 'x' | 'y' | 'layer'>) => gnodes.push({ ...g, x: 0, y: 0, layer: 0 });
  for (const p of mod.ports) {
    if (p.direction !== 'output') push(sizeTerminal(p.name, 'in', p.width));
    if (p.direction !== 'input') push(sizeTerminal(p.name, 'out', p.width));
  }
  for (const c of mod.cells) {
    const portOrder: Partial<Record<Side, string[]>> = {};
    for (const po of rules?.ports ?? []) if (po.node === c.id) portOrder[po.side] = po.order;
    const frame = rules?.frames.find((f) => f.node === c.id);
    push(sizeCell(design, c, { frame: frame ? { width: frame.width, height: frame.height } : undefined, portOrder }));
  }
  const inTerm = (name: string) => `$in:${name}`;
  const outTerm = (name: string) => `$out:${name}`;

  // --- nets -> route nets + edges
  const edges: GEdge[] = [];
  const routeNets: RouteNet[] = [];
  const undriven: string[] = [];
  const connected = new Set<string>(); // `${node}\u0000${port}`
  const sliceOf = (s: { msb?: number; lsb?: number }) => s.msb === undefined ? undefined : s.msb === s.lsb ? `[${s.msb}]` : `[${s.msb}:${s.lsb}]`;
  const epNode = (cell: string | undefined, port: string, asDriver: boolean) => cell ?? (asDriver ? inTerm(port) : outTerm(port));
  for (const [netId, idx] of netIndex) {
    // primary driver: a true output first, then anything else (inout / black-box)
    const drivers = [...idx.drivers].sort((a, b) => (a.dir === 'output' || (!a.cell && a.dir === 'input') ? 0 : 1) - (b.dir === 'output' || (!b.cell && b.dir === 'input') ? 0 : 1));
    const sinks = idx.sinks.filter((s) => !(drivers[0] && s.cell === drivers[0].cell && s.port === drivers[0].port));
    if (!drivers.length) { if (sinks.length) undriven.push(netId); continue; }
    if (drivers.filter((d) => d.dir === 'output' || (!d.cell && d.dir === 'input')).length > 1)
      diagnostics.push({ severity: 'warning', scope, message: `net '${netId}' has multiple drivers` });
    const d = drivers[0];
    const dnode = epNode(d.cell, d.port, true);
    const extra = drivers.slice(1).map((x) => ({ cell: x.cell, port: x.port, segment: x.segment, dir: x.dir }));
    const allSinks = [...sinks, ...extra.filter((x) => !sinks.some((s) => s.cell === x.cell && s.port === x.port))];
    connected.add(`${dnode}\u0000${d.port}`);
    const rsinks = allSinks.map((s) => {
      const node = epNode(s.cell, s.port, false);
      connected.add(`${node}\u0000${s.port}`);
      edges.push({ from: dnode, to: node, net: netId });
      return { node, port: s.port, slice: isConst(s.segment) ? undefined : sliceOf(s.segment) };
    });
    const w = netWidth.get(netId) ?? null;
    routeNets.push({
      net: netId, width: w, driver: { node: dnode, port: d.port, slice: isConst(d.segment) ? undefined : sliceOf(d.segment) }, sinks: rsinks,
      label: netId.startsWith('$') ? undefined : w && w > 1 ? `${netId} [${w - 1}:0]` : netId,
    });
  }
  // constants: one const node per (cell, port, segment)
  for (const c of mod.cells) {
    const dirs = new Map(cellPorts(design, c).map((p) => [p.name, p.direction]));
    for (const [pname, segs] of Object.entries(c.connections)) {
      segs.forEach((seg, i) => {
        if (!isConst(seg) || dirs.get(pname) === 'output') return;
        const id = `$const:${c.id}:${pname}:${i}`;
        push(sizeConst(id, seg.const));
        edges.push({ from: id, to: c.id, net: id });
        connected.add(`${c.id}\u0000${pname}`);
        routeNets.push({ net: id, width: null, driver: { node: id, port: 'Y' }, sinks: [{ node: c.id, port: pname }] });
      });
    }
  }

  // --- layered candidate
  const cons = rules?.constraints ?? [];
  const layerRules = cons.filter((c) => c.kind === 'leftOf').map((c) => ({ id: c.id, a: (c as { a: string }).a, b: (c as { b: string }).b, strength: c.strength }));
  const orderRules = cons.filter((c) => c.kind === 'above').map((c) => ({ id: c.id, a: (c as { a: string }).a, b: (c as { b: string }).b, strength: c.strength }));
  const res = layered(
    { nodes: gnodes, edges, layerRules, orderRules, nodeGap: defaults.nodeGap, layerGap: defaults.layerGap, trackPitch: TRACK_PITCH },
    ({ nodes, layers }) => channelUsage(routeNets, nodes, layers.length + 1),
  );
  for (const id of res.droppedRules) diagnostics.push({ severity: 'warning', scope, constraints: [id], message: `preferred leftOf '${id}' conflicts with signal flow; dropped from layering` });

  // --- constraints
  const solved = solveConstraints({ nodes: res.nodes, layers: res.layers, channels: res.channels, constraints: cons, nodeGap: defaults.nodeGap, scope });
  diagnostics.push(...solved.diagnostics);
  const nodes: Record<string, NodeGeom> = {};
  for (const [id, g] of res.nodes) {
    if (g.dummy) continue;
    nodes[id] = { id, kind: g.kind, x: g.x + PADDING, y: g.y + PADDING, w: g.w, h: g.h, layer: g.layer, ports: g.ports, text: g.text };
    for (const p of Object.values(nodes[id].ports)) if (!connected.has(`${id}\u0000${p.name}`)) p.open = true;
  }
  for (const d of res.dummies.values()) { d.y += PADDING; d.x += PADDING; }
  for (const g of res.nodes.values()) if (!g.dummy) { g.x += PADDING; g.y += PADDING; }
  const channels = res.channels.map((c) => ({ x0: c.x0 + PADDING, x1: c.x1 + PADDING }));
  if (!solved.ok) {
    return { module: mod.name, scope, ok: false, width: 0, height: 0, nodes, wires: [], diagnostics, undriven };
  }

  // --- routing on final geometry
  let bottom = 0;
  for (const g of Object.values(nodes)) bottom = Math.max(bottom, g.y + g.h);
  const wires = route({ nodes: res.nodes, channels, dummies: res.dummies, nets: routeNets, trackPitch: TRACK_PITCH, bottomY: bottom });
  let width = 0, height = 0;
  for (const g of Object.values(nodes)) { width = Math.max(width, g.x + g.w); height = Math.max(height, g.y + g.h); }
  for (const w of wires) for (const pl of w.polylines) for (const p of pl) { width = Math.max(width, p.x); height = Math.max(height, p.y); }
  if (mod.cells.length === 0 && mod.ports.length === 0) diagnostics.push({ severity: 'info', scope, message: 'empty module' });
  for (const c of mod.cells) if (c.kind === 'instance' && c.resolved === false) diagnostics.push({ severity: 'info', scope, message: `'${c.id}' is a black box (module '${c.module}' not in design)` });
  return { module: mod.name, scope, ok: true, width: width + PADDING, height: height + PADDING, nodes, wires, diagnostics, undriven };
}

/** Frame size (world units) a parent layout allocated to an expandable child. */
export function frameOf(layout: ModuleLayout, cellId: string) {
  const n = layout.nodes[cellId];
  return n ? { w: n.w, h: n.h } : undefined;
}

export { isExpandable };
