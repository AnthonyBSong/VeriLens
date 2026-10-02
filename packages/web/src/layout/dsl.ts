// Layout DSL (YAML, version 1). Parsing + validation against a design.
// Semantics are documented in docs/FORMATS.md. Layout rules never touch connectivity.
import YAML from 'yaml';
import type { Design } from '../model/design';
import { isExpandable, moduleAt, cellPorts } from '../model/design';
import type { Diagnostic, LayoutDefaults, Side } from './types';
import { DEFAULTS } from './types';

export type Strength = 'required' | 'preferred';

export type Constraint =
  | { id: string; kind: 'leftOf' | 'above'; a: string; b: string; gap: number; strength: Strength; weight: number }
  | { id: string; kind: 'alignY'; nodes: string[]; strength: Strength; weight: number };

export interface PortOrder { node: string; side: Side; order: string[] }
export interface Frame { node: string; width: number; height: number }

export interface ScopeRules { scope: string; constraints: Constraint[]; ports: PortOrder[]; frames: Frame[] }

export interface LayoutFile { version: 1; defaults: LayoutDefaults; scopes: ScopeRules[] }

export interface ParseResult { file: LayoutFile | null; diagnostics: Diagnostic[] }

const SUPPORTED_KINDS = new Set(['leftOf', 'above', 'alignY']);
const SIDES = new Set<string>(['west', 'east', 'north', 'south']);

const isRecord = (x: unknown): x is Record<string, unknown> => typeof x === 'object' && x !== null && !Array.isArray(x);
const isName = (x: unknown): x is string => typeof x === 'string' && x.length > 0;
const isPosNum = (x: unknown): x is number => typeof x === 'number' && Number.isFinite(x) && x > 0;
const isNonNegNum = (x: unknown): x is number => typeof x === 'number' && Number.isFinite(x) && x >= 0;

/** Detect a directed cycle; returns the ids of edges on one cycle, or null. */
function findCycle(edges: { id: string; a: string; b: string }[]): string[] | null {
  const adj = new Map<string, { id: string; b: string }[]>();
  for (const e of edges) { if (!adj.has(e.a)) adj.set(e.a, []); adj.get(e.a)!.push({ id: e.id, b: e.b }); }
  const state = new Map<string, 1 | 2>();
  const stack: { node: string; id: string }[] = [];
  const dfs = (n: string): string[] | null => {
    state.set(n, 1);
    for (const e of adj.get(n) ?? []) {
      const s = state.get(e.b);
      if (s === 1) {
        const i = stack.findIndex((f) => f.node === e.b);
        return [...stack.slice(i).map((f) => f.id), e.id];
      }
      if (!s) { stack.push({ node: e.b, id: e.id }); const r = dfs(e.b); stack.pop(); if (r) return r; }
    }
    state.set(n, 2);
    return null;
  };
  for (const n of adj.keys()) if (!state.has(n)) { stack.length = 0; stack.push({ node: n, id: '' }); const r = dfs(n); if (r) return r.filter(Boolean); }
  return null;
}

export function parseLayoutFile(text: string, design: Design): ParseResult {
  const diagnostics: Diagnostic[] = [];
  const err = (message: string, scope?: string, constraints?: string[]) => diagnostics.push({ severity: 'error', message, scope, constraints });
  let raw: unknown;
  try { raw = YAML.parse(text); } catch (e) { err(`layout: YAML parse error: ${(e as Error).message}`); return { file: null, diagnostics }; }
  if (raw == null) raw = {};
  if (!isRecord(raw)) { err('layout: document must be a mapping'); return { file: null, diagnostics }; }
  if (raw.version !== 1) err(`layout: unsupported version ${JSON.stringify(raw.version)} (expected 1)`);

  const defaults: LayoutDefaults = { ...DEFAULTS };
  if (raw.defaults !== undefined) {
    if (!isRecord(raw.defaults)) err('layout: defaults must be a mapping');
    else {
      const d = raw.defaults;
      if (d.direction !== undefined && d.direction !== 'right') err(`layout: defaults.direction '${String(d.direction)}' unsupported (only 'right')`);
      if (d.routing !== undefined && d.routing !== 'orthogonal') err(`layout: defaults.routing '${String(d.routing)}' unsupported (only 'orthogonal')`);
      if (d.nodeGap !== undefined) { if (isPosNum(d.nodeGap)) defaults.nodeGap = d.nodeGap; else err('layout: defaults.nodeGap must be a positive number'); }
      if (d.layerGap !== undefined) { if (isPosNum(d.layerGap)) defaults.layerGap = d.layerGap; else err('layout: defaults.layerGap must be a positive number'); }
      for (const k of Object.keys(d)) if (!['direction', 'routing', 'nodeGap', 'layerGap'].includes(k)) err(`layout: unknown defaults key '${k}'`);
    }
  }

  const scopes: ScopeRules[] = [];
  const seenScopes = new Set<string>();
  const rawScopes = raw.scopes ?? [];
  if (!Array.isArray(rawScopes)) err('layout: scopes must be a list');
  else rawScopes.forEach((rs, si) => {
    if (!isRecord(rs)) { err(`layout: scopes[${si}] must be a mapping`); return; }
    const scopeName = rs.scope ?? rs.module; // `module:` accepted as alias of `scope:`
    if (!isName(scopeName)) { err(`layout: scopes[${si}] needs a 'scope' (hierarchical instance path)`); return; }
    const scope = scopeName;
    if (seenScopes.has(scope)) err(`scope '${scope}': listed more than once`, scope);
    seenScopes.add(scope);
    const path = scope.split('/');
    const mod = moduleAt(design, path);
    if (!mod) { err(`scope '${scope}': no instance at this path (paths start with the top module '${design.top}')`, scope); return; }
    const cellById = new Map(mod.cells.map((c) => [c.id, c]));
    const knownNode = (n: unknown, ctx: string, ids?: string[]) => {
      if (!isName(n)) { err(`scope '${scope}': ${ctx}: node must be a non-empty string`, scope, ids); return false; }
      if (!cellById.has(n)) { err(`scope '${scope}': ${ctx}: unknown node '${n}' (must be an immediate child instance or cell)`, scope, ids); return false; }
      return true;
    };

    const constraints: Constraint[] = [];
    const ids = new Set<string>();
    const rawCons = rs.constraints ?? [];
    if (!Array.isArray(rawCons)) err(`scope '${scope}': constraints must be a list`, scope);
    else rawCons.forEach((rc, ci) => {
      if (!isRecord(rc)) { err(`scope '${scope}': constraints[${ci}] must be a mapping`, scope); return; }
      const id = isName(rc.id) ? rc.id : `#${ci}`;
      if (!isName(rc.id)) err(`scope '${scope}': constraints[${ci}] needs an 'id'`, scope);
      if (ids.has(id)) err(`scope '${scope}': duplicate constraint id '${id}'`, scope, [id]);
      ids.add(id);
      const kind = rc.kind as Constraint['kind'];
      if (!isName(kind) || !SUPPORTED_KINDS.has(kind)) { err(`scope '${scope}': constraint '${id}': unsupported kind '${String(kind)}' (supported: leftOf, above, alignY)`, scope, [id]); return; }
      const strength = rc.strength ?? 'required';
      if (strength !== 'required' && strength !== 'preferred') { err(`scope '${scope}': constraint '${id}': strength must be 'required' or 'preferred'`, scope, [id]); return; }
      const weight = rc.weight ?? 1;
      if (!isPosNum(weight)) { err(`scope '${scope}': constraint '${id}': weight must be a positive number`, scope, [id]); return; }
      if (kind === 'alignY') {
        if (!Array.isArray(rc.nodes) || rc.nodes.length < 2) { err(`scope '${scope}': constraint '${id}': alignY needs 'nodes' with at least two entries`, scope, [id]); return; }
        if (!rc.nodes.every((n) => knownNode(n, `constraint '${id}'`, [id]))) return;
        if (new Set(rc.nodes).size !== rc.nodes.length) { err(`scope '${scope}': constraint '${id}': alignY nodes repeat`, scope, [id]); return; }
        constraints.push({ id, kind, nodes: rc.nodes as string[], strength, weight });
      } else {
        if (!knownNode(rc.a, `constraint '${id}' a`, [id]) || !knownNode(rc.b, `constraint '${id}' b`, [id])) return;
        if (rc.a === rc.b) { err(`scope '${scope}': constraint '${id}': a and b are the same node`, scope, [id]); return; }
        const gap = rc.gap ?? 0;
        if (!isNonNegNum(gap)) { err(`scope '${scope}': constraint '${id}': gap must be a non-negative number`, scope, [id]); return; }
        constraints.push({ id, kind, a: rc.a as string, b: rc.b as string, gap, strength, weight });
      }
    });

    // contradictions among required constraints
    for (const kind of ['leftOf', 'above'] as const) {
      const edges = constraints.filter((c) => c.kind === kind && c.strength === 'required').map((c) => ({ id: c.id, a: (c as { a: string }).a, b: (c as { b: string }).b }));
      const cyc = findCycle(edges);
      if (cyc) err(`scope '${scope}': required ${kind} constraints form a cycle: ${cyc.join(' -> ')}`, scope, cyc);
    }
    for (const al of constraints) {
      if (al.kind !== 'alignY' || al.strength !== 'required') continue;
      for (const ab of constraints) {
        if (ab.kind === 'above' && ab.strength === 'required' && al.nodes.includes(ab.a) && al.nodes.includes(ab.b))
          err(`scope '${scope}': required alignY '${al.id}' contradicts required above '${ab.id}' (same nodes)`, scope, [al.id, ab.id]);
      }
    }

    const ports: PortOrder[] = [];
    const rawPorts = rs.ports ?? [];
    if (!Array.isArray(rawPorts)) err(`scope '${scope}': ports must be a list`, scope);
    else rawPorts.forEach((rp, pi) => {
      if (!isRecord(rp)) { err(`scope '${scope}': ports[${pi}] must be a mapping`, scope); return; }
      if (!knownNode(rp.node, `ports[${pi}]`)) return;
      const node = rp.node as string;
      if (!isName(rp.side) || !SIDES.has(rp.side)) { err(`scope '${scope}': ports[${pi}] (${node}): side must be west|east|north|south`, scope); return; }
      if (!Array.isArray(rp.order) || !rp.order.every(isName)) { err(`scope '${scope}': ports[${pi}] (${node}): order must be a list of port names`, scope); return; }
      const valid = new Set(cellPorts(design, cellById.get(node)!).map((p) => p.name));
      const order = rp.order as string[];
      for (const p of order) if (!valid.has(p)) err(`scope '${scope}': ports (${node}): unknown port '${p}'`, scope);
      if (ports.some((q) => q.node === node && q.side === rp.side)) err(`scope '${scope}': ports (${node}): side '${rp.side}' listed twice`, scope);
      ports.push({ node, side: rp.side as Side, order });
    });

    const frames: Frame[] = [];
    const rawFrames = rs.frames ?? [];
    if (!Array.isArray(rawFrames)) err(`scope '${scope}': frames must be a list`, scope);
    else rawFrames.forEach((rf, fi) => {
      if (!isRecord(rf)) { err(`scope '${scope}': frames[${fi}] must be a mapping`, scope); return; }
      if (!knownNode(rf.node, `frames[${fi}]`)) return;
      const node = rf.node as string;
      if (!isExpandable(design, cellById.get(node)!)) err(`scope '${scope}': frames (${node}): frames apply only to expandable module instances`, scope);
      if (!isPosNum(rf.width) || !isPosNum(rf.height)) { err(`scope '${scope}': frames (${node}): width and height must be positive numbers`, scope); return; }
      if (frames.some((f) => f.node === node)) err(`scope '${scope}': frames (${node}): listed twice`, scope);
      frames.push({ node, width: rf.width, height: rf.height });
    });

    for (const k of Object.keys(rs)) if (!['scope', 'module', 'constraints', 'ports', 'frames'].includes(k)) err(`scope '${scope}': unknown key '${k}'`, scope);
    scopes.push({ scope, constraints, ports, frames });
  });
  for (const k of Object.keys(raw)) if (!['version', 'defaults', 'scopes'].includes(k)) err(`layout: unknown top-level key '${k}'`);

  const hasErrors = diagnostics.some((d) => d.severity === 'error');
  return { file: hasErrors ? null : { version: 1, defaults, scopes }, diagnostics };
}
