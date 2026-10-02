// Main schematic canvas: owns the camera (pan/zoom/fit). Nothing here moves
// design geometry; the only transform is the camera group.
import { useEffect, useMemo, useRef } from 'react';
import { pathKey, traceNet } from '../model/design';
import type { ModuleLayout } from '../layout/types';
import { consumeFit, focusModule, select, setCamera, setViewport, useStore, type Camera } from '../state/store';
import { ModuleView } from './ModuleView';

const MIN_K = 0.05, MAX_K = 8;

export function Canvas() {
  const design = useStore((s) => s.design);
  const focus = useStore((s) => s.focus);
  const camera = useStore((s) => s.camera);
  const layouts = useStore((s) => s.layouts);
  const failed = useStore((s) => s.failed);
  const selection = useStore((s) => s.selection);
  const pendingFit = useStore((s) => s.pendingFit);
  const viewport = useStore((s) => s.viewport);
  const svgRef = useRef<SVGSVGElement>(null);
  const focusKey = pathKey(focus);
  const layout: ModuleLayout | undefined = layouts[focusKey];

  const highlight = useMemo(() => {
    const set = new Set<string>();
    if (design && selection?.kind === 'net') for (const t of traceNet(design, selection.path, selection.net)) set.add(`${pathKey(t.path)}:${t.net}`);
    return set;
  }, [design, selection]);

  // viewport size tracking
  useEffect(() => {
    const el = svgRef.current; if (!el) return;
    const ro = new ResizeObserver(() => setViewport(el.clientWidth, el.clientHeight));
    ro.observe(el);
    setViewport(el.clientWidth, el.clientHeight);
    return () => ro.disconnect();
  }, []);

  // wheel zoom around the pointer (native listener: React's is passive)
  useEffect(() => {
    const el = svgRef.current; if (!el) return;
    const onWheel = (e: WheelEvent) => {
      e.preventDefault();
      const rect = el.getBoundingClientRect();
      const px = e.clientX - rect.left, py = e.clientY - rect.top;
      const cam = camRef.current;
      const f = Math.exp(-e.deltaY * 0.0015);
      const k = Math.min(MAX_K, Math.max(MIN_K, cam.k * f));
      const r = k / cam.k;
      setCamera({ k, x: px - (px - cam.x) * r, y: py - (py - cam.y) * r });
    };
    el.addEventListener('wheel', onWheel, { passive: false });
    return () => el.removeEventListener('wheel', onWheel);
  }, []);
  const camRef = useRef<Camera>(camera);
  camRef.current = camera;

  // fit requests
  useEffect(() => {
    if (!pendingFit || !layout) return;
    let box = { x: 0, y: 0, w: layout.width, h: layout.height };
    if (pendingFit === 'selection' && selection) {
      if (selection.kind === 'cell' && pathKey(selection.path.slice(0, -1)) === focusKey) {
        const n = layout.nodes[selection.path[selection.path.length - 1]];
        if (n) box = { x: n.x, y: n.y, w: n.w, h: n.h };
      } else if (selection.kind === 'net' && pathKey(selection.path) === focusKey) {
        const w = layout.wires.find((r) => r.net === selection.net);
        const pts = w?.polylines.flat() ?? [];
        if (pts.length) {
          const xs = pts.map((p) => p.x), ys = pts.map((p) => p.y);
          box = { x: Math.min(...xs), y: Math.min(...ys), w: Math.max(...xs) - Math.min(...xs), h: Math.max(...ys) - Math.min(...ys) };
        }
      }
    }
    const margin = 40;
    const k = Math.min(MAX_K, Math.max(MIN_K, Math.min((viewport.w - margin) / Math.max(box.w, 1), (viewport.h - margin) / Math.max(box.h, 1), pendingFit === 'selection' ? 2.5 : 1.5)));
    consumeFit({ k, x: (viewport.w - box.w * k) / 2 - box.x * k, y: (viewport.h - box.h * k) / 2 - box.y * k });
  }, [pendingFit, layout, viewport, selection, focusKey]);

  // pan: only from the background
  const pan = useRef<{ sx: number; sy: number; cam: Camera; moved: boolean } | null>(null);
  const onBgPointerDown = (e: React.PointerEvent) => {
    if (e.button !== 0) return;
    pan.current = { sx: e.clientX, sy: e.clientY, cam: camRef.current, moved: false };
    (e.currentTarget as Element).setPointerCapture(e.pointerId);
  };
  const onPointerMove = (e: React.PointerEvent) => {
    const p = pan.current; if (!p) return;
    const dx = e.clientX - p.sx, dy = e.clientY - p.sy;
    if (Math.abs(dx) + Math.abs(dy) > 2) p.moved = true;
    if (p.moved) setCamera({ k: p.cam.k, x: p.cam.x + dx, y: p.cam.y + dy });
  };
  const onPointerUp = () => {
    const p = pan.current; pan.current = null;
    if (p && !p.moved) select(null);
  };

  const crumbs = focus.map((_, i) => focus.slice(0, i + 1));
  return (
    <div className="canvas-wrap">
      <svg ref={svgRef} className="canvas" data-testid="canvas" onPointerMove={onPointerMove} onPointerUp={onPointerUp} onPointerCancel={onPointerUp}>
        <rect className="bg" width="100%" height="100%" onPointerDown={onBgPointerDown} />
        <g id="camera" transform={`translate(${camera.x},${camera.y}) scale(${camera.k})`}>
          {design && layout && <ModuleView design={design} path={focus} layout={layout} scale={camera.k} highlight={highlight} />}
        </g>
      </svg>
      {design && (
        <div className="breadcrumbs">
          {crumbs.map((p, i) => (
            <span key={pathKey(p)}>
              {i > 0 && <span className="sep">/</span>}
              <button className={i === crumbs.length - 1 ? 'current' : ''} onClick={() => focusModule(p)}>{p[p.length - 1]}</button>
            </span>
          ))}
        </div>
      )}
      {design && !layout && (
        <div className="canvas-message">
          {failed[focusKey] ? (
            <div className="error">Layout for <b>{focusKey}</b> failed:<ul>{failed[focusKey].map((d, i) => <li key={i}>{d.message}</li>)}</ul></div>
          ) : 'computing layout…'}
        </div>
      )}
      {!design && <div className="canvas-message">Open a design (VeriLens AST JSON or normalized design JSON) or pick a sample.</div>}
    </div>
  );
}
