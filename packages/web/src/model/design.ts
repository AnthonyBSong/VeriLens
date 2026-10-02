// Normalized hierarchical design model. This is *what exists*: modules, cells,
// ports, nets and connectivity. It never carries geometry or UI state.
// See docs/FORMATS.md for the JSON contract.

export type Direction = 'input' | 'output' | 'inout';

export interface SourceLoc { file?: string; line?: number }

export interface PortDef {
  name: string;
  direction: Direction;
  /** Bit width; null when unknown/parametric (see widthExpr). */
  width: number | null;
  widthExpr?: string;
  /** Module ports only: id of the internal net bound to this port (boundary mapping). Defaults to the port name. */
  net?: string;
}

/** One piece of a port connection, msb-first when several are listed (a concatenation). */
export type Segment = { net: string; msb?: number; lsb?: number } | { const: string };

export interface Cell {
  id: string;
  kind: 'instance' | 'primitive';
  /** kind=instance: module definition name. */
  module?: string;
  /** kind=primitive: cell type, e.g. and, or, mux, add, reg, process, const. Unknown types get a labeled box. */
  type?: string;
  label?: string;
  /** Primitive port list. Instances take their ports from the module definition; unresolved instances list what they connect. */
  ports?: PortDef[];
  connections: Record<string, Segment[]>;
  params?: Record<string, string>;
  source?: SourceLoc;
  /** false when the instantiated module is not in the design (black box). */
  resolved?: boolean;
  attrs?: Record<string, unknown>;
}

export interface Net {
  id: string;
  width: number | null;
  widthExpr?: string;
  /** Referenced but never declared (implicit net / hierarchical reference). */
  implicit?: boolean;
  source?: SourceLoc;
}

export interface ModuleDef {
  name: string;
  ports: PortDef[];
  cells: Cell[];
  nets: Net[];
  params?: Record<string, string>;
  source?: SourceLoc;
  /** Not expandable: internals unknown. */
  blackbox?: boolean;
  attrs?: Record<string, unknown>;
}

export interface Design {
  version: 1;
  top: string;
  modules: Record<string, ModuleDef>;
}

/** Instance paths: [top, child, grandchild]. Key form joins with '/'. */
export type InstancePath = string[];
export const pathKey = (p: InstancePath) => p.join('/');

export interface Endpoint {
  /** undefined = the module's own port (boundary terminal). */
  cell?: string;
  port: string;
  dir: Direction;
  segment: Segment;
}

export interface NetIndex { drivers: Endpoint[]; sinks: Endpoint[] }

export function isConst(s: Segment): s is { const: string } {
  return 'const' in s;
}

export function moduleAt(design: Design, path: InstancePath): ModuleDef | undefined {
  let mod: ModuleDef | undefined = design.modules[design.top];
  if (!mod || path[0] !== design.top) return undefined;
  for (let i = 1; i < path.length; i++) {
    const cell: Cell | undefined = mod.cells.find((c) => c.id === path[i]);
    if (!cell || cell.kind !== 'instance' || !cell.module) return undefined;
    mod = design.modules[cell.module];
    if (!mod) return undefined;
  }
  return mod;
}

/** Ports of a cell in declaration order. */
export function cellPorts(design: Design, cell: Cell): PortDef[] {
  if (cell.kind === 'instance' && cell.module && design.modules[cell.module]) {
    return design.modules[cell.module].ports;
  }
  if (cell.ports) return cell.ports;
  return Object.keys(cell.connections).map((name) => ({ name, direction: 'inout' as const, width: null }));
}

export function isExpandable(design: Design, cell: Cell): boolean {
  if (cell.kind !== 'instance' || !cell.module) return false;
  const m = design.modules[cell.module];
  return !!m && !m.blackbox;
}

/** Driver/sink endpoints per net for one module. inout ports count as both. */
export function buildNetIndex(design: Design, mod: ModuleDef): Map<string, NetIndex> {
  const idx = new Map<string, NetIndex>();
  const get = (net: string) => {
    let e = idx.get(net);
    if (!e) { e = { drivers: [], sinks: [] }; idx.set(net, e); }
    return e;
  };
  for (const n of mod.nets) get(n.id);
  for (const p of mod.ports) {
    const ep: Endpoint = { port: p.name, dir: p.direction, segment: { net: p.net ?? p.name } };
    const e = get(p.net ?? p.name);
    if (p.direction !== 'output') e.drivers.push(ep);
    if (p.direction !== 'input') e.sinks.push(ep);
  }
  for (const c of mod.cells) {
    const ports = new Map(cellPorts(design, c).map((p) => [p.name, p]));
    for (const [pname, segs] of Object.entries(c.connections)) {
      const dir = ports.get(pname)?.direction ?? 'inout';
      for (const seg of segs) {
        if (isConst(seg)) continue;
        const e = get(seg.net);
        const ep: Endpoint = { cell: c.id, port: pname, dir, segment: seg };
        if (dir !== 'input') e.drivers.push(ep);
        if (dir !== 'output') e.sinks.push(ep);
      }
    }
  }
  return idx;
}

export interface TracedNet { path: InstancePath; net: string }

/**
 * Follow a net across hierarchy boundaries in both directions: into resolved
 * child instances (through the child's port->net mapping) and up into the
 * parent (through the parent's connection to this instance's port).
 */
export function traceNet(design: Design, path: InstancePath, net: string): TracedNet[] {
  const seen = new Set<string>();
  const out: TracedNet[] = [];
  const queue: TracedNet[] = [{ path, net }];
  while (queue.length) {
    const cur = queue.shift()!;
    const key = pathKey(cur.path) + ':' + cur.net;
    if (seen.has(key)) continue;
    seen.add(key);
    out.push(cur);
    const mod = moduleAt(design, cur.path);
    if (!mod) continue;
    // up: this net is bound to a module port and we have a parent
    if (cur.path.length > 1) {
      const parentPath = cur.path.slice(0, -1);
      const parent = moduleAt(design, parentPath);
      const instId = cur.path[cur.path.length - 1];
      const inst = parent?.cells.find((c) => c.id === instId);
      for (const p of mod.ports) {
        if ((p.net ?? p.name) !== cur.net) continue;
        for (const seg of inst?.connections[p.name] ?? []) {
          if (!isConst(seg)) queue.push({ path: parentPath, net: seg.net });
        }
      }
    }
    // down: cell ports connected to this net whose cell is a resolved instance
    for (const c of mod.cells) {
      if (c.kind !== 'instance' || !c.module) continue;
      const child = design.modules[c.module];
      if (!child) continue;
      for (const [pname, segs] of Object.entries(c.connections)) {
        if (!segs.some((s) => !isConst(s) && s.net === cur.net)) continue;
        const cp = child.ports.find((p) => p.name === pname);
        if (cp) queue.push({ path: [...cur.path, c.id], net: cp.net ?? cp.name });
      }
    }
  }
  return out;
}

export interface DesignError { message: string }

/** Structural validation. Throws with all problems joined when the design is unusable. */
export function validateDesign(input: unknown): Design {
  const errors: string[] = [];
  const d = input as Design;
  if (!d || typeof d !== 'object') throw new Error('design: not an object');
  if (d.version !== 1) errors.push(`design: unsupported version ${String(d.version)} (expected 1)`);
  if (!d.modules || typeof d.modules !== 'object') throw new Error('design: missing modules');
  if (!d.top || !d.modules[d.top]) errors.push(`design: top module '${d.top}' not found`);
  for (const [name, mod] of Object.entries(d.modules)) {
    if (mod.name !== name) errors.push(`module '${name}': name field '${mod.name}' does not match key`);
    const nets = new Set((mod.nets ?? []).map((n) => n.id));
    const cellIds = new Set<string>();
    for (const p of mod.ports ?? []) {
      if (!nets.has(p.net ?? p.name)) errors.push(`module '${name}': port '${p.name}' bound to unknown net '${p.net ?? p.name}'`);
    }
    for (const c of mod.cells ?? []) {
      if (cellIds.has(c.id)) errors.push(`module '${name}': duplicate cell id '${c.id}'`);
      cellIds.add(c.id);
      if (c.kind === 'instance') {
        if (!c.module) errors.push(`module '${name}': instance '${c.id}' has no module`);
        else if (!d.modules[c.module] && c.resolved !== false)
          errors.push(`module '${name}': instance '${c.id}' references unknown module '${c.module}' (mark resolved:false for black boxes)`);
      } else if (c.kind !== 'primitive') {
        errors.push(`module '${name}': cell '${c.id}' has invalid kind '${String(c.kind)}'`);
      }
      const portNames = new Set(cellPorts(d, c).map((p) => p.name));
      for (const [pname, segs] of Object.entries(c.connections ?? {})) {
        if (c.kind === 'instance' && c.module && d.modules[c.module] && !portNames.has(pname))
          errors.push(`module '${name}': cell '${c.id}' connects unknown port '${pname}' of '${c.module}'`);
        for (const s of segs) {
          if (!isConst(s) && !nets.has(s.net)) errors.push(`module '${name}': cell '${c.id}' port '${pname}' references unknown net '${s.net}'`);
        }
      }
    }
  }
  if (errors.length) throw new Error(errors.join('\n'));
  return d;
}

/** Instances never instantiated by another module. */
export function rootModules(modules: Record<string, ModuleDef>): string[] {
  const used = new Set<string>();
  for (const m of Object.values(modules)) for (const c of m.cells) if (c.kind === 'instance' && c.module) used.add(c.module);
  return Object.keys(modules).filter((n) => !used.has(n));
}

/** All instance paths, depth-first in declaration order. */
export function allInstancePaths(design: Design): InstancePath[] {
  const out: InstancePath[] = [];
  const walk = (path: InstancePath, mod: ModuleDef, depth: number) => {
    out.push(path);
    if (depth > 64) return;
    for (const c of mod.cells) {
      if (c.kind !== 'instance' || !c.module) continue;
      const child = design.modules[c.module];
      if (child) walk([...path, c.id], child, depth + 1);
    }
  };
  const top = design.modules[design.top];
  if (top) walk([design.top], top, 0);
  return out;
}
