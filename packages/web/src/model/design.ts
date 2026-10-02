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
