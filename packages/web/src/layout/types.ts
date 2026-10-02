// Computed geometry. Produced by the layout engine, consumed by the renderer.
// World units are SVG user units in the owning module's local coordinate system.

export type Side = 'west' | 'east' | 'north' | 'south';

export interface Point { x: number; y: number }

export interface PortAnchor { x: number; y: number; side: Side; name: string; /** no wire attached */ open?: boolean }

export interface NodeGeom {
  id: string;
  /** 'cell' = design cell, 'in'/'out' = boundary terminal for a module port, 'const' = constant driver. */
  kind: 'cell' | 'in' | 'out' | 'const';
  x: number; y: number; w: number; h: number;
  layer: number;
  ports: Record<string, PortAnchor>;
  /** const/terminal nodes: the displayed text. */
  text?: string;
}

export interface WireRoute {
  net: string;
  /** Orthogonal polylines. */
  polylines: Point[][];
  /** Branch points where the net splits (3+ segment ends meet). Never drawn at plain crossings. */
  junctions: Point[];
  /** Label placed at the driver stub. */
  label?: { x: number; y: number; text: string };
  /** Endpoint slice labels e.g. [3:0] at a sink port. */
  sliceLabels: { x: number; y: number; text: string }[];
  width: number | null;
}

export interface Diagnostic {
  severity: 'error' | 'warning' | 'info';
  scope?: string;
  constraints?: string[];
  message: string;
  /** For preferred-constraint violations: by how much (world units). */
  amount?: number;
}

export interface ModuleLayout {
  module: string;
  scope: string;
  ok: boolean;
  width: number;
  height: number;
  nodes: Record<string, NodeGeom>;
  wires: WireRoute[];
  diagnostics: Diagnostic[];
  /** Nets flagged undriven / multiply driven, for the inspector. */
  undriven: string[];
}

export interface LayoutDefaults {
  direction: 'right';
  nodeGap: number;
  layerGap: number;
  routing: 'orthogonal';
}

export const DEFAULTS: LayoutDefaults = { direction: 'right', nodeGap: 24, layerGap: 72, routing: 'orthogonal' };
