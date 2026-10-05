// SVG bodies for primitive symbols. Shapes follow netlistsvg's default skin;
// ports are positioned by layout/symbols.ts so the two must agree on sizes.
import type { SymbolShape } from '../layout/symbols';

export function SymbolBody({ shape, w, h, className }: { shape: SymbolShape; w: number; h: number; className: string }) {
  switch (shape) {
    case 'and':
    case 'nand':
      return (
        <g className={className}>
          <path d={`M0,0 L0,${h} L${w * 0.45},${h} A${w * 0.55} ${h / 2} 0 0 0 ${w * 0.45},0 Z`} />
          {shape === 'nand' && <circle cx={w + 3} cy={h / 2} r={3} />}
        </g>
      );
    case 'or':
    case 'nor':
      return (
        <g className={className}>
          <path d={`M0,0 A${w} ${h} 0 0 1 0,${h} A${w} ${h} 0 0 0 ${w},${h / 2} A${w} ${h} 0 0 0 0,0`} />
          {shape === 'nor' && <circle cx={w + 3} cy={h / 2} r={3} />}
        </g>
      );
    case 'xor':
    case 'xnor':
      return (
        <g className={className}>
          <path d={`M3,0 A${w} ${h} 0 0 1 3,${h} A${w} ${h} 0 0 0 ${w},${h / 2} A${w} ${h} 0 0 0 3,0`} />
          <path d={`M0,0 A${w} ${h} 0 0 1 0,${h}`} fill="none" />
          {shape === 'xnor' && <circle cx={w + 3} cy={h / 2} r={3} />}
        </g>
      );
    case 'not':
    case 'buf':
      return (
        <g className={className}>
          <path d={`M0,0 L0,${h} L${w - 8},${h / 2} Z`} />
          {shape === 'not' && <circle cx={w - 4} cy={h / 2} r={3} />}
        </g>
      );
    case 'mux':
      return (
        <g className={className}>
          <path d={`M0,0 L${w},${h * 0.25} L${w},${h * 0.75} L0,${h} Z`} />
        </g>
      );
    case 'circle':
      return (
        <g className={className}>
          <circle cx={w / 2} cy={h / 2} r={w / 2} />
        </g>
      );
    case 'reg':
      return (
        <g className={className}>
          <rect x={0} y={0} width={w} height={h} rx={2} />
          <path d={`M0,${h - 12} L6,${h - 6} L0,${h}`} fill="none" />
        </g>
      );
    default:
      return (
        <g className={className}>
          <rect x={0} y={0} width={w} height={h} rx={2} />
        </g>
      );
  }
}
