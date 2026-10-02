// Symbol catalogue: sizes and port placement for cells. Pure geometry, no rendering.
import type { Cell, Design, PortDef } from '../model/design';
import { cellPorts, isExpandable } from '../model/design';
import type { NodeGeom, PortAnchor, Side } from './types';

export const FONT = 11;
export const CHAR_W = 6.3; // approx advance for 11px sans
export const textWidth = (s: string, size = FONT) => s.length * CHAR_W * (size / FONT);

export const PORT_PITCH = 16;
export const HEADER_H = 20;
/** Default frame for expandable module instances (header + viewport). */
export const DEFAULT_FRAME = { w: 260, h: 170 };
export const MIN_BOX = { w: 96, h: 44 };

export type SymbolShape = 'and' | 'nand' | 'or' | 'nor' | 'xor' | 'xnor' | 'not' | 'buf' | 'mux' | 'circle' | 'reg' | 'box' | 'process';

const SHAPES: Record<string, SymbolShape> = {
  and: 'and', reduce_and: 'and', logic_and: 'and', nand: 'nand', reduce_nand: 'nand',
  or: 'or', reduce_or: 'or', logic_or: 'or', nor: 'nor', reduce_nor: 'nor',
  xor: 'xor', reduce_xor: 'xor', xnor: 'xnor', reduce_xnor: 'xnor',
  not: 'not', logic_not: 'not', buf: 'buf', mux: 'mux',
  add: 'circle', sub: 'circle', mul: 'circle', div: 'circle', mod: 'circle', pow: 'circle', neg: 'circle', pos: 'circle',
  eq: 'circle', ne: 'circle', lt: 'circle', le: 'circle', gt: 'circle', ge: 'circle',
  shl: 'circle', shr: 'circle', sshl: 'circle', sshr: 'circle',
  reg: 'reg', dff: 'reg', process: 'process',
};

export const CIRCLE_TEXT: Record<string, string> = {
  add: '+', sub: '−', mul: '×', div: '÷', mod: '%', pow: '**', neg: '−', pos: '+',
  eq: '=', ne: '≠', lt: '<', le: '≤', gt: '>', ge: '≥', shl: '<<', shr: '>>', sshl: '<<<', sshr: '>>>',
};

export function shapeOf(cell: Cell): SymbolShape {
  if (cell.kind === 'instance') return 'box';
  if (cell.type === 'process') return (cell.attrs?.seq ? 'reg' : 'process');
  return SHAPES[cell.type ?? ''] ?? 'box';
}

/** Whether port labels are drawn inside the symbol (boxes) or omitted (gates). */
export function labelsInside(shape: SymbolShape): boolean {
  return shape === 'box' || shape === 'reg' || shape === 'process';
}

export interface SizeOpts { frame?: { width: number; height: number }; portOrder?: Partial<Record<Side, string[]>> }

function sideOf(p: PortDef): Side {
  return p.direction === 'output' ? 'east' : 'west';
}

/** Assign ports to sides honoring an explicit order (unlisted ports follow in declaration order). */
export function portSides(ports: PortDef[], order?: Partial<Record<Side, string[]>>): Record<Side, string[]> {
  const sides: Record<Side, string[]> = { west: [], east: [], north: [], south: [] };
  const placed = new Set<string>();
  for (const side of ['west', 'east', 'north', 'south'] as Side[]) {
    for (const name of order?.[side] ?? []) { if (!placed.has(name)) { sides[side].push(name); placed.add(name); } }
  }
  for (const p of ports) if (!placed.has(p.name)) { sides[sideOf(p)].push(p.name); placed.add(p.name); }
  return sides;
}

export function spread(n: number, from: number, to: number): number[] {
  if (n === 0) return [];
  const step = (to - from) / (n + 1);
  return Array.from({ length: n }, (_, i) => from + step * (i + 1));
}

/** Compute node size and port anchor positions (relative to node origin). */
export function sizeCell(design: Design, cell: Cell, opts: SizeOpts = {}): Omit<NodeGeom, 'x' | 'y' | 'layer'> {
  const ports = cellPorts(design, cell);
  const shape = shapeOf(cell);
  const sides = portSides(ports, opts.portOrder);
  let w: number, h: number;
  const anchors: Record<string, PortAnchor> = {};
  const place = (side: Side, names: string[], x: number, y0: number, y1: number) => {
    spread(names.length, y0, y1).forEach((y, i) => { anchors[names[i]] = { x, y, side, name: names[i] }; });
  };
  switch (shape) {
    case 'and': case 'nand': case 'or': case 'nor': case 'xor': case 'xnor': {
      w = 34; h = Math.max(26, sides.west.length * 10 + 6);
      place('west', sides.west, 0, 0, h); place('east', sides.east, w, 0, h);
      return { id: cell.id, kind: 'cell', w, h, ports: anchors };
    }
    case 'not': case 'buf': {
      w = 30; h = 20;
      place('west', sides.west, 0, 0, h); place('east', sides.east, w, 0, h);
      return { id: cell.id, kind: 'cell', w, h, ports: anchors };
    }
    case 'circle': {
      w = 30; h = 30;
      place('west', sides.west, 0, 0, h); place('east', sides.east, w, 0, h);
      return { id: cell.id, kind: 'cell', w, h, ports: anchors };
    }
    case 'mux': {
      w = 22; h = 40;
      // data inputs on west (1 above 0 like netlistsvg), select on south
      const data = sides.west.filter((n) => n !== 'S');
      place('west', data, 0, 0, h);
      if (sides.west.includes('S')) anchors.S = { x: w / 2, y: h - 5, side: 'south', name: 'S' };
      place('east', sides.east, w, 0, h);
      return { id: cell.id, kind: 'cell', w, h, ports: anchors };
    }
    default: {
      // box / reg / process: labelled ports inside, name label above
      const westW = Math.max(0, ...sides.west.map((n) => textWidth(n)));
      const eastW = Math.max(0, ...sides.east.map((n) => textWidth(n)));
      const label = cell.label ?? cell.module ?? cell.type ?? cell.id;
      const expandable = isExpandable(design, cell);
      w = Math.max(MIN_BOX.w, westW + eastW + 28, textWidth(cell.id) + 30, textWidth(label) + 16);
      h = Math.max(MIN_BOX.h, Math.max(sides.west.length, sides.east.length) * PORT_PITCH + 12 + (expandable ? HEADER_H : 0));
      if (expandable) { w = Math.max(w, DEFAULT_FRAME.w); h = Math.max(h, DEFAULT_FRAME.h); }
      if (opts.frame) { w = opts.frame.width; h = opts.frame.height; }
      const top = expandable ? HEADER_H : 0;
      place('west', sides.west, 0, top, h); place('east', sides.east, w, top, h);
      spread(sides.north.length, 0, w).forEach((x, i) => { anchors[sides.north[i]] = { x, y: 0, side: 'north', name: sides.north[i] }; });
      spread(sides.south.length, 0, w).forEach((x, i) => { anchors[sides.south[i]] = { x, y: h, side: 'south', name: sides.south[i] }; });
      return { id: cell.id, kind: 'cell', w, h, ports: anchors };
    }
  }
}

export function sizeTerminal(name: string, dir: 'in' | 'out', width: number | null): Omit<NodeGeom, 'x' | 'y' | 'layer'> {
  const text = width && width > 1 ? `${name} [${width - 1}:0]` : name;
  const w = textWidth(text) + 18, h = 18;
  const ports: Record<string, PortAnchor> = dir === 'in'
    ? { [name]: { x: w, y: h / 2, side: 'east', name } }
    : { [name]: { x: 0, y: h / 2, side: 'west', name } };
  return { id: (dir === 'in' ? '$in:' : '$out:') + name, kind: dir, w, h, ports, text };
}

export function sizeConst(id: string, text: string): Omit<NodeGeom, 'x' | 'y' | 'layer'> {
  const w = textWidth(text) + 10, h = 16;
  return { id, kind: 'const', w, h, ports: { Y: { x: w, y: h / 2, side: 'east', name: 'Y' } }, text };
}
