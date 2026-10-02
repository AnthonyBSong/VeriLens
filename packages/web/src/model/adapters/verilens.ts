// Adapter: VeriLens core AST JSON (gen_ast output) -> normalized Design.
// The core emits parsed expression trees; this file only walks them. No text parsing here.

import type { Cell, Design, ModuleDef, Net, PortDef, Segment } from '../design';
import { rootModules } from '../design';

// ---- AST JSON shape (subset used here) ----
export interface VlWidth { msb: number; lsb: number; scalar: boolean; unknown?: boolean; expr?: string }
export interface VlPort extends VlWidth { name: string; direction: 'input' | 'output' | 'inout'; type: string; line?: number }
export interface VlNet extends VlWidth { name: string; type: string; line?: number; init?: VlExpr }
export type VlExpr =
  | { kind: 'IDENTIFIER'; name: string }
  | { kind: 'LITERAL'; value: string }
  | { kind: 'UNARY_OP'; op: string; operand: VlExpr }
  | { kind: 'BINARY_OP'; op: string; lhs: VlExpr; rhs: VlExpr }
  | { kind: 'CONDITIONAL'; cond: VlExpr; then: VlExpr; else: VlExpr }
  | { kind: 'CONCAT'; parts: VlExpr[] }
  | { kind: 'REPLICATION'; count: VlExpr; value: VlExpr }
  | { kind: 'BIT_SELECT'; base: VlExpr; index: VlExpr }
  | { kind: 'PART_SELECT'; base: VlExpr; msb: VlExpr; lsb: VlExpr };
export type VlStmt =
  | { kind: 'SEQ_BLOCK'; body: (VlStmt | null)[] }
  | { kind: 'BLOCKING_ASSIGN' | 'NONBLOCKING_ASSIGN'; lhs: VlExpr; rhs: VlExpr }
  | { kind: 'IF_STATEMENT'; cond: VlExpr; then: VlStmt | null; else: VlStmt | null }
  | { kind: 'CASE_STATEMENT'; variant: string; expr: VlExpr; items: { patterns: VlExpr[]; body: VlStmt | null }[] };
export interface VlInstance {
  module: string; instance: string; line?: number; resolved: boolean; wildcard: boolean;
  parameters: { name: string; value: string }[];
  connections: { port: string; signal: string; expr: VlExpr | null }[];
}
export interface VlModule {
  name: string; source_file: string; line?: number; end_line?: number; pragmas?: string[];
  parameters: { name: string; default: string }[];
  ports: VlPort[];
  net_decls: VlNet[];
  instances: VlInstance[];
  gate_primitives: { type: string; instance: string; line?: number; ports: string[] }[];
  assigns: { line?: number; lhs: VlExpr; rhs: VlExpr }[];
  always_blocks: { line?: number; sensitivity: string; body: VlStmt | null }[];
}

export function isVerilensAst(x: unknown): x is VlModule[] {
  return Array.isArray(x) && (x.length === 0 || (typeof x[0] === 'object' && x[0] !== null && 'instances' in x[0] && 'net_decls' in x[0]));
}

const BINARY_TYPES: Record<string, string> = {
  '+': 'add', '-': 'sub', '*': 'mul', '/': 'div', '%': 'mod', '**': 'pow',
  '&': 'and', '|': 'or', '^': 'xor', '~^': 'xnor', '^~': 'xnor',
  '&&': 'logic_and', '||': 'logic_or',
  '==': 'eq', '!=': 'ne', '===': 'eq', '!==': 'ne',
  '<': 'lt', '<=': 'le', '>': 'gt', '>=': 'ge',
  '<<': 'shl', '>>': 'shr', '<<<': 'sshl', '>>>': 'sshr',
};
const UNARY_TYPES: Record<string, string> = {
  '~': 'not', '!': 'logic_not', '-': 'neg', '+': 'pos',
  '&': 'reduce_and', '|': 'reduce_or', '^': 'reduce_xor', '~&': 'reduce_nand', '~|': 'reduce_nor', '~^': 'reduce_xnor', '^~': 'reduce_xnor',
};

function widthOf(w: VlWidth): { width: number | null; widthExpr?: string } {
  if (w.unknown) return { width: null, widthExpr: w.expr };
  if (w.scalar) return { width: 1 };
  return { width: Math.abs(w.msb - w.lsb) + 1 };
}

function litInt(e: VlExpr | null | undefined): number | undefined {
  if (!e || e.kind !== 'LITERAL') return undefined;
  const m = /^(\d+)$/.exec(e.value);
  return m ? Number(m[1]) : undefined;
}

interface Ctx {
  mod: VlModule;
  nets: Map<string, Net>;
  params: Set<string>;
  cells: Cell[];
  counter: number;
  file: string;
}

function baseIdent(name: string): string {
  // struct member access `bus.field` shares the wire bundle `bus`; package refs keep the full text
  if (name.includes('::')) return name;
  const dot = name.indexOf('.');
  return dot > 0 ? name.slice(0, dot) : name;
}

function netRef(ctx: Ctx, name: string): Segment {
  if (ctx.params.has(name)) return { const: name };
  const id = baseIdent(name);
  if (!ctx.nets.has(id)) ctx.nets.set(id, { id, width: null, implicit: true });
  return { net: id };
}

function tempNet(ctx: Ctx, hint: string): string {
  const id = `$${hint}_${++ctx.counter}`;
  ctx.nets.set(id, { id, width: null });
  return id;
}

function emitCell(ctx: Ctx, type: string, label: string, inputs: Record<string, Segment[]>, out: string, line?: number, attrs?: Record<string, unknown>): Segment[] {
  const y = tempNet(ctx, type);
  const id = `$${type}_${ctx.counter}`;
  const ports: PortDef[] = [...Object.keys(inputs).map((n) => ({ name: n, direction: 'input' as const, width: null })), { name: out, direction: 'output', width: null }];
  ctx.cells.push({ id, kind: 'primitive', type, label, ports, connections: { ...inputs, [out]: [{ net: y }] }, source: { file: ctx.file, line }, attrs });
  return [{ net: y }];
}

/** Lower an expression to segments, emitting primitive cells for operators. */
function lower(ctx: Ctx, e: VlExpr | null, line?: number): Segment[] {
  if (!e) return [];
  switch (e.kind) {
    case 'IDENTIFIER': return [netRef(ctx, e.name)];
    case 'LITERAL': return [{ const: e.value }];
    case 'CONCAT': return e.parts.flatMap((p) => lower(ctx, p, line));
    case 'BIT_SELECT':
    case 'PART_SELECT': {
      const msb = e.kind === 'BIT_SELECT' ? litInt(e.index) : litInt(e.msb);
      const lsb = e.kind === 'BIT_SELECT' ? msb : litInt(e.lsb);
      if (e.base.kind === 'IDENTIFIER' && msb !== undefined && lsb !== undefined) {
        const ref = netRef(ctx, e.base.name);
        return 'net' in ref ? [{ net: ref.net, msb, lsb }] : [ref];
      }
      // dynamic index: explicit select cell
      const inputs: Record<string, Segment[]> = { A: lower(ctx, e.base, line) };
      if (e.kind === 'BIT_SELECT') inputs.I = lower(ctx, e.index, line);
      else { inputs.M = lower(ctx, e.msb, line); inputs.L = lower(ctx, e.lsb, line); }
      return emitCell(ctx, e.kind === 'BIT_SELECT' ? 'bitsel' : 'partsel', e.kind === 'BIT_SELECT' ? '[i]' : '[m:l]', inputs, 'Y', line);
    }
    case 'REPLICATION': {
      const n = litInt(e.count);
      return emitCell(ctx, 'repl', `{${n ?? 'N'}{}}`, { A: lower(ctx, e.value, line) }, 'Y', line);
    }
    case 'UNARY_OP': {
      const type = UNARY_TYPES[e.op] ?? 'unary';
      return emitCell(ctx, type, e.op, { A: lower(ctx, e.operand, line) }, 'Y', line);
    }
    case 'BINARY_OP': {
      const type = BINARY_TYPES[e.op] ?? 'binary';
      return emitCell(ctx, type, e.op, { A: lower(ctx, e.lhs, line), B: lower(ctx, e.rhs, line) }, 'Y', line);
    }
    case 'CONDITIONAL':
      return emitCell(ctx, 'mux', 'mux', { S: lower(ctx, e.cond, line), '1': lower(ctx, e.then, line), '0': lower(ctx, e.else, line) }, 'Y', line);
  }
}

function identsOf(e: VlExpr | null | undefined, out: string[]) {
  if (!e) return;
  switch (e.kind) {
    case 'IDENTIFIER': out.push(baseIdent(e.name)); break;
    case 'LITERAL': break;
    case 'UNARY_OP': identsOf(e.operand, out); break;
    case 'BINARY_OP': identsOf(e.lhs, out); identsOf(e.rhs, out); break;
    case 'CONDITIONAL': identsOf(e.cond, out); identsOf(e.then, out); identsOf(e.else, out); break;
    case 'CONCAT': e.parts.forEach((p) => identsOf(p, out)); break;
    case 'REPLICATION': identsOf(e.count, out); identsOf(e.value, out); break;
    case 'BIT_SELECT': identsOf(e.base, out); identsOf(e.index, out); break;
    case 'PART_SELECT': identsOf(e.base, out); identsOf(e.msb, out); identsOf(e.lsb, out); break;
  }
}

function lhsBase(e: VlExpr): string | undefined {
  if (e.kind === 'IDENTIFIER') return baseIdent(e.name);
  if (e.kind === 'BIT_SELECT' || e.kind === 'PART_SELECT') return lhsBase(e.base);
  return undefined;
}

function walkStmt(s: VlStmt | null, reads: string[], writes: string[]) {
  if (!s) return;
  switch (s.kind) {
    case 'SEQ_BLOCK': s.body.forEach((b) => walkStmt(b, reads, writes)); break;
    case 'BLOCKING_ASSIGN':
    case 'NONBLOCKING_ASSIGN': {
      identsOf(s.rhs, reads);
      if (s.lhs.kind === 'CONCAT') s.lhs.parts.forEach((p) => { const b = lhsBase(p); if (b) writes.push(b); });
      else { const b = lhsBase(s.lhs); if (b) writes.push(b); }
      if (s.lhs.kind === 'BIT_SELECT') identsOf(s.lhs.index, reads);
      break;
    }
    case 'IF_STATEMENT': identsOf(s.cond, reads); walkStmt(s.then, reads, writes); walkStmt(s.else, reads, writes); break;
    case 'CASE_STATEMENT':
      identsOf(s.expr, reads);
      s.items.forEach((it) => { it.patterns.forEach((p) => identsOf(p, reads)); walkStmt(it.body, reads, writes); });
      break;
  }
}

const uniq = (xs: string[]) => [...new Set(xs)];

function convertModule(vm: VlModule, all: Map<string, VlModule>): ModuleDef {
  const ctx: Ctx = { mod: vm, nets: new Map(), params: new Set(vm.parameters.map((p) => p.name)), cells: [], counter: 0, file: vm.source_file };
  const ports: PortDef[] = vm.ports.map((p) => ({ name: p.name, direction: p.direction, ...widthOf(p) }));
  for (const p of vm.ports) ctx.nets.set(p.name, { id: p.name, ...widthOf(p), source: { file: vm.source_file, line: p.line } });
  for (const n of vm.net_decls) if (!ctx.nets.has(n.name)) ctx.nets.set(n.name, { id: n.name, ...widthOf(n), source: { file: vm.source_file, line: n.line } });

  const seenIds = new Set<string>();
  const uniqueId = (base: string) => { let id = base, k = 1; while (seenIds.has(id)) id = `${base}_${k++}`; seenIds.add(id); return id; };

  for (const inst of vm.instances) {
    const target = all.get(inst.module);
    const connections: Record<string, Segment[]> = {};
    const unresolvedPorts: PortDef[] = [];
    inst.connections.forEach((c, i) => {
      const pname = c.port || target?.ports[i]?.name || `p${i}`;
      connections[pname] = c.expr ? lower(ctx, c.expr, inst.line) : (c.signal ? [{ const: c.signal }] : []);
      if (!target) unresolvedPorts.push({ name: pname, direction: 'inout', width: null });
    });
    if (inst.wildcard && target) {
      for (const tp of target.ports) if (!(tp.name in connections) && ctx.nets.has(tp.name)) connections[tp.name] = [{ net: tp.name }];
    }
    const params: Record<string, string> = {};
    inst.parameters.forEach((p, i) => { params[p.name || target?.parameters[i]?.name || `#${i}`] = p.value; });
    ctx.cells.push({
      id: uniqueId(inst.instance), kind: 'instance', module: inst.module, resolved: !!target,
      ports: target ? undefined : unresolvedPorts, connections,
      params: Object.keys(params).length ? params : undefined,
      source: { file: vm.source_file, line: inst.line },
    });
  }

  vm.gate_primitives.forEach((g, gi) => {
    const id = uniqueId(g.instance || `$${g.type}_g${gi}`);
    const gports: PortDef[] = g.ports.map((_, i) => ({ name: i === 0 ? 'Y' : `A${i}`, direction: i === 0 ? 'output' : 'input', width: null }));
    const connections: Record<string, Segment[]> = {};
    // gate ports are raw text in the core output; identifiers map to nets, anything else is kept as an opaque const
    g.ports.forEach((sig, i) => { connections[gports[i].name] = /^[A-Za-z_][\w$.]*$/.test(sig) ? [netRef(ctx, sig)] : [{ const: sig }]; });
    ctx.cells.push({ id, kind: 'primitive', type: g.type, label: g.type, ports: gports, connections, source: { file: vm.source_file, line: g.line } });
  });

  for (const a of vm.assigns) {
    const lhs = lower(ctx, a.lhs, a.line);
    const rhs = lower(ctx, a.rhs, a.line);
    const last = ctx.cells[ctx.cells.length - 1];
    const r0 = rhs[0];
    const ly = last?.connections.Y?.[0];
    const producedByCell = rhs.length === 1 && r0 && 'net' in r0 && r0.net.startsWith('$') && last?.kind === 'primitive' && ly && 'net' in ly && ly.net === r0.net;
    if (producedByCell) {
      // retarget the operator's output straight onto the assigned nets
      ctx.nets.delete(r0.net);
      last.connections.Y = lhs;
    } else {
      const y = tempNet(ctx, 'buf');
      ctx.nets.delete(y);
      const id = `$buf_${ctx.counter}`;
      ctx.cells.push({ id, kind: 'primitive', type: 'buf', label: '=', ports: [{ name: 'A', direction: 'input', width: null }, { name: 'Y', direction: 'output', width: null }], connections: { A: rhs, Y: lhs }, source: { file: vm.source_file, line: a.line } });
    }
  }

  vm.always_blocks.forEach((ab, i) => {
    const reads: string[] = []; const writes: string[] = [];
    const seq = /\b(posedge|negedge)\b/.test(ab.sensitivity);
    const sens = uniq(ab.sensitivity.replace(/\b(posedge|negedge|or)\b/g, ' ').split(/[\s,]+/).filter((t) => /^[A-Za-z_]/.test(t)));
    walkStmt(ab.body, reads, writes);
    const w = uniq(writes);
    // a signal the block both reads and writes is its own state (hold/feedback), not an input
    const r = uniq([...(seq ? sens : []), ...reads]).filter((n) => !w.includes(n));
    const inputs = r.filter((n) => !ctx.params.has(n));
    const ports: PortDef[] = [...inputs.map((n) => ({ name: n, direction: 'input' as const, width: null })), ...w.map((n) => ({ name: n, direction: 'output' as const, width: null }))];
    const connections: Record<string, Segment[]> = {};
    for (const n of [...inputs, ...w]) connections[n] = [netRef(ctx, n)];
    ctx.cells.push({
      id: `$always_${i + 1}`, kind: 'primitive', type: 'process', label: seq ? 'always_ff' : (ab.sensitivity === '*' ? 'always_comb' : `always @(${ab.sensitivity})`),
      ports, connections, source: { file: vm.source_file, line: ab.line }, attrs: { seq, sensitivity: ab.sensitivity },
    });
  });

  const params: Record<string, string> = {};
  for (const p of vm.parameters) params[p.name] = p.default;
  return {
    name: vm.name, ports, cells: ctx.cells, nets: [...ctx.nets.values()],
    params: Object.keys(params).length ? params : undefined,
    source: { file: vm.source_file, line: vm.line },
    attrs: vm.pragmas?.length ? { pragmas: vm.pragmas } : undefined,
  };
}

export interface AdapterOptions { top?: string }

export function fromVerilensAst(ast: VlModule[], opts: AdapterOptions = {}): Design {
  const all = new Map(ast.map((m) => [m.name, m]));
  const modules: Record<string, ModuleDef> = {};
  for (const vm of ast) if (!modules[vm.name]) modules[vm.name] = convertModule(vm, all);
  const pragmaTop = ast.find((m) => m.pragmas?.includes('top'))?.name;
  const roots = rootModules(modules);
  const top = opts.top ?? pragmaTop ?? roots[0] ?? ast[0]?.name ?? '';
  return { version: 1, top, modules };
}
