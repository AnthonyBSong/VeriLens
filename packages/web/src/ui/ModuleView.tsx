// Renders one module's computed layout in its local coordinate system.
// Expanded child instances render their own layout inside a clipped nested
// <svg> viewport under the header; the outer frame, its port anchors and every
// sibling keep their coordinates regardless of expansion state.
import { memo, useMemo } from 'react';
import type { Cell, Design, InstancePath, ModuleDef } from '../model/design';
import { buildNetIndex, cellPorts, isConst, isExpandable, moduleAt, pathKey } from '../model/design';
import { CIRCLE_TEXT, HEADER_H, labelsInside, shapeOf } from '../layout/symbols';
import type { ModuleLayout, NodeGeom, WireRoute } from '../layout/types';
import { select, toggleExpand, useStore, type Selection } from '../state/store';
import { SymbolBody } from './Symbols';

export interface ViewProps {
  design: Design;
  path: InstancePath;
  layout: ModuleLayout;
  /** effective world->screen scale, for level of detail */
  scale: number;
  highlight: Set<string>;
}

const LOD_LABELS = 0.45;

export const ModuleView = memo(function ModuleView({ design, path, layout, scale, highlight }: ViewProps) {
  const key = pathKey(path);
  const mod = moduleAt(design, path)!;
  const selection = useStore((s) => s.selection);
  const expanded = useStore((s) => s.expanded);
  const layouts = useStore((s) => s.layouts);
  const failed = useStore((s) => s.failed);
  const showLabels = scale >= LOD_LABELS;
  const cells = useMemo(() => new Map(mod.cells.map((c) => [c.id, c])), [mod]);
  const netIndex = useMemo(() => buildNetIndex(design, mod), [design, mod]);
  const rel = useMemo(() => relations(selection, key, netIndex), [selection, key, netIndex]);
  const selectedCell = selection?.kind === 'cell' && pathKey(selection.path.slice(0, -1)) === key ? selection.path[selection.path.length - 1] : null;
  const selectedNet = selection?.kind === 'net' && pathKey(selection.path) === key ? selection.net : null;

  return (
    <g className="module-view" data-scope={key}>
      <g className="wires">
        {layout.wires.map((w) => (
          <Wire key={w.net} wire={w} scope={key} path={path} showLabels={showLabels}
            highlighted={highlight.has(`${key}:${w.net}`) || selectedNet === w.net} />
        ))}
      </g>
      <g className="nodes">
        {Object.values(layout.nodes).map((n) => {
          if (n.kind === 'in' || n.kind === 'out') {
            const pname = Object.keys(n.ports)[0];
            const net = mod.ports.find((p) => p.name === pname)?.net ?? pname;
            return <Terminal key={n.id} node={n} path={path} mod={mod} showLabels={showLabels} highlighted={highlight.has(`${key}:${net}`) || selectedNet === net} />;
          }
          if (n.kind === 'const') return <ConstNode key={n.id} node={n} showLabels={showLabels} />;
          const cell = cells.get(n.id);
          if (!cell) return null;
          const cpath = [...path, cell.id];
          const ckey = pathKey(cpath);
          const state = selectedCell === cell.id ? 'selected' : rel.both.has(cell.id) ? 'feedback' : rel.drivers.has(cell.id) ? 'driving' : rel.driven.has(cell.id) ? 'driven' : '';
          const expandable = isExpandable(design, cell);
          const isOpen = expandable && !!expanded[ckey];
          return (
            <g key={n.id} className={`cell ${state}`} data-path={ckey} data-kind={cell.kind} transform={`translate(${n.x},${n.y})`}
              onClick={(e) => { e.stopPropagation(); select({ kind: 'cell', path: cpath }); }}>
              <title>{ckey}{cell.module ? ` : ${cell.module}` : cell.type ? ` : ${cell.type}` : ''}</title>
              <CellBody cell={cell} node={n} showLabels={showLabels} expandable={expandable} open={isOpen} cpath={cpath} />
              {isOpen && (
                <ExpandedContent design={design} cpath={cpath} node={n} layout={layouts[ckey]} failed={failed[ckey]} scale={scale} highlight={highlight} />
              )}
              <Ports design={design} cell={cell} node={n} showLabels={showLabels} inside={labelsInside(shapeOf(cell))} />
            </g>
          );
        })}
      </g>
    </g>
  );
});

function relations(selection: Selection | null, key: string, netIndex: ReturnType<typeof buildNetIndex>) {
  const drivers = new Set<string>(), driven = new Set<string>(), both = new Set<string>();
  if (!selection || selection.kind !== 'cell' || pathKey(selection.path.slice(0, -1)) !== key) return { drivers, driven, both };
  const id = selection.path[selection.path.length - 1];
  for (const idx of netIndex.values()) {
    const sinksMe = idx.sinks.some((s) => s.cell === id);
    const drivesMe = idx.drivers.some((d) => d.cell === id);
    if (sinksMe) for (const d of idx.drivers) if (d.cell && d.cell !== id) drivers.add(d.cell);
    if (drivesMe) for (const s of idx.sinks) if (s.cell && s.cell !== id) driven.add(s.cell);
  }
  for (const d of drivers) if (driven.has(d)) { both.add(d); }
  return { drivers, driven, both };
}

function Wire({ wire, scope, path, showLabels, highlighted }: { wire: WireRoute; scope: string; path: InstancePath; showLabels: boolean; highlighted: boolean }) {
  const bus = (wire.width ?? 1) > 1;
  const d = wire.polylines.map((pl) => pl.map((p, i) => `${i ? 'L' : 'M'}${p.x},${p.y}`).join(' ')).join(' ');
  return (
    <g className={`wire ${bus ? 'bus' : ''} ${highlighted ? 'highlight' : ''}`} data-net={wire.net} data-scope={scope}
      onClick={(e) => { e.stopPropagation(); if (!wire.net.startsWith('$const:')) select({ kind: 'net', path, net: wire.net }); }}>
      <title>{wire.net}{wire.width ? ` [${wire.width}]` : ''}</title>
      <path className="hit" d={d} />
      <path className="line" d={d} />
      {wire.junctions.map((j, i) => <circle key={i} className="junction" cx={j.x} cy={j.y} r={2.4} />)}
      {showLabels && wire.label && <text className="net-label" x={wire.label.x} y={wire.label.y}>{wire.label.text}</text>}
      {showLabels && wire.sliceLabels.map((s, i) => <text key={i} className="slice-label" x={s.x} y={s.y} textAnchor="end">{s.text}</text>)}
    </g>
  );
}

function Terminal({ node, path, mod, showLabels, highlighted }: { node: NodeGeom; path: InstancePath; mod: ModuleDef; showLabels: boolean; highlighted: boolean }) {
  const name = Object.keys(node.ports)[0];
  const port = mod.ports.find((p) => p.name === name);
  const net = port?.net ?? name;
  const { w, h } = node;
  const d = node.kind === 'in' ? `M0,0 L${w - 7},0 L${w},${h / 2} L${w - 7},${h} L0,${h} Z` : `M7,0 L${w},0 L${w},${h} L7,${h} L0,${h / 2} Z`;
  return (
    <g className={`terminal ${node.kind} ${highlighted ? 'highlight' : ''}`} data-terminal={node.id} transform={`translate(${node.x},${node.y})`}
      onClick={(e) => { e.stopPropagation(); select({ kind: 'net', path, net }); }}>
      <title>{port?.direction ?? ''} {name}</title>
      <path d={d} />
      {showLabels && <text x={node.kind === 'in' ? 4 : 10} y={h / 2 + 3.5}>{node.text}</text>}
    </g>
  );
}

function ConstNode({ node, showLabels }: { node: NodeGeom; showLabels: boolean }) {
  return (
    <g className="const" transform={`translate(${node.x},${node.y})`}>
      <rect width={node.w} height={node.h} rx={2} />
      {showLabels && <text x={node.w / 2} y={node.h / 2 + 3.5} textAnchor="middle">{node.text}</text>}
    </g>
  );
}

function CellBody({ cell, node, showLabels, expandable, open, cpath }: { cell: Cell; node: NodeGeom; showLabels: boolean; expandable: boolean; open: boolean; cpath: InstancePath }) {
  const shape = shapeOf(cell);
  const { w, h } = node;
  if (cell.kind === 'instance' || shape === 'box' || shape === 'process' || shape === 'reg') {
    const typeLabel = cell.kind === 'instance' ? cell.module ?? '' : cell.label ?? cell.type ?? '';
    const blackbox = cell.kind === 'instance' && cell.resolved === false;
    return (
      <g className={`box ${cell.kind} ${blackbox ? 'blackbox' : ''} ${shape}`}>
        <rect className="body" width={w} height={h} rx={2} />
        {shape === 'reg' && <path className="clk-mark" d={`M0,${h - 12} L6,${h - 6} L0,${h}`} fill="none" />}
        {expandable ? (
          <g className="header">
            <rect width={w} height={HEADER_H} />
            <g className="toggle" data-toggle={pathKey(cpath)} transform="translate(4,4)"
              onPointerDown={(e) => e.stopPropagation()}
              onClick={(e) => { e.stopPropagation(); toggleExpand(cpath); }}>
              <rect width={12} height={12} rx={1.5} />
              <text x={6} y={9.5} textAnchor="middle">{open ? '−' : '+'}</text>
            </g>
            <text className="name" x={22} y={14}>{cell.id}</text>
            {showLabels && <text className="type" x={w - 6} y={14} textAnchor="end">{typeLabel}</text>}
            {!open && showLabels && <text className="type-center" x={w / 2} y={HEADER_H + (h - HEADER_H) / 2 + 4} textAnchor="middle">{typeLabel}</text>}
          </g>
        ) : (
          showLabels && (
            <g className="labels">
              <text className="name" x={w / 2} y={-4} textAnchor="middle">{cell.kind === 'instance' ? cell.id : cell.id.startsWith('$') ? typeLabel : cell.id}</text>
              {cell.kind === 'instance' && <text className="type-center" x={w / 2} y={h / 2 + 4} textAnchor="middle">{typeLabel}{blackbox ? ' (black box)' : ''}</text>}
              {cell.kind !== 'instance' && !cell.id.startsWith('$') && <text className="type-center" x={w / 2} y={h / 2 + 4} textAnchor="middle">{typeLabel}</text>}
            </g>
          )
        )}
      </g>
    );
  }
  const label = shape === 'circle' ? CIRCLE_TEXT[cell.type ?? ''] ?? cell.label ?? '' : '';
  return (
    <g className={`symbol ${shape}`}>
      <SymbolBody shape={shape} w={w} h={h} className="body" />
      {label && <text className="op" x={w / 2} y={h / 2 + 4.5} textAnchor="middle">{label}</text>}
      {showLabels && shape !== 'circle' && <text className="name" x={w / 2} y={-3} textAnchor="middle">{cell.label ?? cell.type}</text>}
      {shape === 'mux' && showLabels && (
        <>
          <text className="mux-label" x={3} y={h * 0.3 + 3}>1</text>
          <text className="mux-label" x={3} y={h * 0.72 + 3}>0</text>
        </>
      )}
    </g>
  );
}

function Ports({ design, cell, node, showLabels, inside }: { design: Design; cell: Cell; node: NodeGeom; showLabels: boolean; inside: boolean }) {
  const defs = new Map(cellPorts(design, cell).map((p) => [p.name, p]));
  return (
    <g className="ports">
      {Object.values(node.ports).map((a) => {
        const def = defs.get(a.name);
        const text = def && def.width && def.width > 1 ? `${a.name} [${def.width - 1}:0]` : a.name;
        const unconnected = a.open || !(cell.connections[a.name]?.length);
        const hasConst = cell.connections[a.name]?.some(isConst);
        return (
          <g key={a.name} className={`port ${a.side} ${def?.direction ?? ''}`} data-port={a.name}>
            {!unconnected || hasConst ? <rect x={a.x - 2.5} y={a.y - 2.5} width={5} height={5} /> : <circle className="open" cx={a.x} cy={a.y} r={2.5} />}
            {inside && showLabels && a.side === 'west' && <text x={a.x + 6} y={a.y + 3.5}>{text}</text>}
            {inside && showLabels && a.side === 'east' && <text x={a.x - 6} y={a.y + 3.5} textAnchor="end">{text}</text>}
            {inside && showLabels && a.side === 'south' && <text x={a.x} y={a.y - 4} textAnchor="middle">{a.name}</text>}
            {inside && showLabels && a.side === 'north' && <text x={a.x} y={a.y + 11} textAnchor="middle">{a.name}</text>}
            <title>{def?.direction ?? ''} {text}</title>
          </g>
        );
      })}
    </g>
  );
}

function ExpandedContent({ design, cpath, node, layout, failed, scale, highlight }: { design: Design; cpath: InstancePath; node: NodeGeom; layout?: ModuleLayout; failed?: unknown[]; scale: number; highlight: Set<string> }) {
  const pad = 3;
  const vx = pad, vy = HEADER_H + pad, vw = node.w - 2 * pad, vh = node.h - HEADER_H - 2 * pad;
  if (!layout) {
    return (
      <g className="viewport">
        <rect x={vx} y={vy} width={vw} height={vh} />
        <text x={node.w / 2} y={vy + vh / 2} textAnchor="middle" className="hint">{failed ? 'layout failed (see diagnostics)' : 'computing layout…'}</text>
      </g>
    );
  }
  const inner = Math.min(vw / layout.width, vh / layout.height);
  return (
    <g className="viewport">
      <rect x={vx} y={vy} width={vw} height={vh} />
      <svg x={vx} y={vy} width={vw} height={vh} viewBox={`0 0 ${layout.width} ${layout.height}`} preserveAspectRatio="xMidYMid meet" className="nested" data-nested={pathKey(cpath)}>
        <ModuleView design={design} path={cpath} layout={layout} scale={scale * inner} highlight={highlight} />
      </svg>
    </g>
  );
}
