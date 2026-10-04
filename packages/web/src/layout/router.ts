// Orthogonal channel router. Runs against final node geometry only.
// Forward nets: driver stub -> vertical trunk in the channel after the driver's
// layer -> horizontal lanes through intermediate layers (at dummy-node y) ->
// sink stubs. Backward (feedback) nets drop to a lane below the drawing and
// come back up in the channel before the sink's layer. Junction dots are placed
// only where a net actually branches; plain crossings get nothing.
import type { GNode } from './layered';
import type { Point, WireRoute } from './types';

export interface RouteSink { node: string; port: string; slice?: string }
export interface RouteNet {
  net: string;
  width: number | null;
  label?: string;
  driver: { node: string; port: string; slice?: string };
  sinks: RouteSink[];
}

export interface RouterInput {
  nodes: Map<string, GNode>;
  channels: { x0: number; x1: number }[];
  dummies: Map<string, GNode>;
  nets: RouteNet[];
  trackPitch: number;
  /** y below which feedback lanes may run */
  bottomY: number;
}

/** A straight bar (vertical or horizontal) with attachments; emits its line and junctions. */
class Bar {
  private att: { at: number; side: 0 | 1 }[] = [];
  private vertical: boolean;
  private pos: number;
  constructor(vertical: boolean, pos: number) { this.vertical = vertical; this.pos = pos; }
  attach(at: number, side: 0 | 1) { this.att.push({ at, side }); }
  emit(lines: Point[][], junctions: Point[]) {
    if (!this.att.length) return;
    const ats = this.att.map((a) => a.at);
    const lo = Math.min(...ats), hi = Math.max(...ats);
    const pt = (t: number): Point => this.vertical ? { x: this.pos, y: t } : { x: t, y: this.pos };
    if (hi - lo > 1e-6) lines.push([pt(lo), pt(hi)]);
    const byAt = new Map<number, { at: number; sides: Set<number> }>();
    for (const a of this.att) {
      const k = Math.round(a.at * 1000) / 1000;
      if (!byAt.has(k)) byAt.set(k, { at: a.at, sides: new Set() });
      byAt.get(k)!.sides.add(a.side);
    }
    for (const { at, sides } of byAt.values()) {
      const ends = (at - lo > 1e-6 ? 1 : 0) + (hi - at > 1e-6 ? 1 : 0);
      if (ends + sides.size >= 3) junctions.push(pt(at));
    }
  }
}

/** Channels each net needs: used for channel width before x placement. */
export function channelUsage(nets: RouteNet[], nodes: Map<string, GNode>, channelCount: number): number[] {
  const use: Set<string>[] = Array.from({ length: channelCount }, () => new Set());
  const mark = (c: number, net: string) => { if (c + 1 >= 0 && c + 1 < use.length) use[c + 1].add(net); };
  for (const net of nets) {
    const d = nodes.get(net.driver.node); if (!d) continue;
    const dl = d.layer;
    let maxL = dl;
    for (const s of net.sinks) {
      const sn = nodes.get(s.node); if (!sn) continue;
      if (sn.layer > dl) maxL = Math.max(maxL, sn.layer);
      else { mark(dl, net.net); mark(sn.layer - 1, net.net); }
    }
    for (let c = dl; c < maxL; c++) mark(c, net.net);
  }
  return use.map((s) => s.size);
}

export function route(input: RouterInput): WireRoute[] {
  const { nodes, channels, dummies, nets, trackPitch, bottomY } = input;
  const anchor = (node: string, port: string): Point | undefined => {
    const n = nodes.get(node); const a = n?.ports[port];
    return n && a ? { x: n.x + a.x, y: n.y + a.y } : undefined;
  };
  // track assignment per channel: order by driver y for a mostly nesting layout
  const perChannel = new Map<number, { net: string; y: number }[]>();
  const want = (c: number, net: string, y: number) => {
    if (!perChannel.has(c)) perChannel.set(c, []);
    const arr = perChannel.get(c)!;
    if (!arr.some((e) => e.net === net)) arr.push({ net, y });
  };
  const plans = nets.map((net) => {
    const D = anchor(net.driver.node, net.driver.port);
    const dn = nodes.get(net.driver.node);
    if (!D || !dn) return null;
    const dl = dn.layer;
    const fwd: { s: RouteSink; p: Point; layer: number }[] = [];
    const back: { s: RouteSink; p: Point; layer: number }[] = [];
    for (const s of net.sinks) {
      const p = anchor(s.node, s.port); const sn = nodes.get(s.node);
      if (!p || !sn) continue;
      (sn.layer > dl ? fwd : back).push({ s, p, layer: sn.layer });
    }
    const maxL = fwd.reduce((m, f) => Math.max(m, f.layer), dl);
    for (let c = dl; c < maxL; c++) want(c, net.net, D.y);
    if (back.length) { want(dl, net.net, D.y); for (const b of back) want(b.layer - 1, net.net, b.p.y); }
    return { net, D, dl, fwd, back, maxL };
  });
  const trackX = new Map<string, number>();
  for (const [c, arr] of perChannel) {
    arr.sort((a, b) => a.y - b.y);
    const ch = channels[c + 1];
    const width = ch.x1 - ch.x0;
    const pitch = Math.min(trackPitch, width / (arr.length + 1));
    const start = ch.x0 + (width - pitch * (arr.length + 1)) / 2;
    arr.forEach((e, i) => trackX.set(`${e.net}:${c}`, start + pitch * (i + 1)));
  }
  const tx = (net: string, c: number) => trackX.get(`${net}:${c}`)!;

  let feedbackLane = 0;
  const out: WireRoute[] = [];
  for (const plan of plans) {
    if (!plan) continue;
    const { net, D, dl, fwd, back, maxL } = plan;
    const lines: Point[][] = [];
    const junctions: Point[] = [];
    const sliceLabels: WireRoute['sliceLabels'] = [];
    const bars = new Map<string, Bar>();
    const vbar = (c: number) => { const k = `v${c}`; if (!bars.has(k)) bars.set(k, new Bar(true, tx(net.net, c))); return bars.get(k)!; };
    const hline = (x0: number, x1: number, y: number) => { if (Math.abs(x1 - x0) > 1e-6) lines.push([{ x: x0, y }, { x: x1, y }]); };

    if (!fwd.length && !back.length) {
      // dangling driver: short stub
      hline(D.x, D.x + 10, D.y);
    }
    // forward tree
    if (fwd.length) {
      for (let c = dl; c < maxL; c++) {
        const bar = vbar(c);
        const x = tx(net.net, c);
        if (c === dl) { hline(D.x, x, D.y); bar.attach(D.y, 0); }
        else {
          const dy = dummies.get(`${net.net}:${c}`)!.y;
          hline(tx(net.net, c - 1), x, dy); bar.attach(dy, 0);
        }
        for (const f of fwd) if (f.layer === c + 1) { hline(x, f.p.x, f.p.y); bar.attach(f.p.y, 1); }
        if (c + 1 < maxL) { const dn = dummies.get(`${net.net}:${c + 1}`); if (dn) { bar.attach(dn.y, 1); vbar(c + 1).attach(dn.y, 0); } }
      }
    }
    // feedback lane
    if (back.length) {
      const laneY = bottomY + 14 + feedbackLane++ * trackPitch;
      const trunk = vbar(dl);
      const x = tx(net.net, dl);
      if (!fwd.length) hline(D.x, x, D.y);
      trunk.attach(D.y, 0);
      trunk.attach(laneY, 0);
      const lane = new Bar(false, laneY);
      lane.attach(x, 1);
      const byLayer = new Map<number, typeof back>();
      for (const b of back) { if (!byLayer.has(b.layer)) byLayer.set(b.layer, []); byLayer.get(b.layer)!.push(b); }
      for (const [sl, bs] of byLayer) {
        const rx = tx(net.net, sl - 1);
        lane.attach(rx, 0);
        const rbar = new Bar(true, rx);
        rbar.attach(laneY, 1);
        for (const b of bs) { hline(rx, b.p.x, b.p.y); rbar.attach(b.p.y, 1); }
        bars.set(`r${sl}`, rbar);
      }
      bars.set('lane', lane);
    }
    for (const b of bars.values()) b.emit(lines, junctions);
    for (const s of [...fwd, ...back]) if (s.s.slice) sliceLabels.push({ x: s.p.x - 3, y: s.p.y - 3, text: s.s.slice });
    const labelText = net.label !== undefined ? net.label : undefined;
    out.push({
      net: net.net, polylines: lines, junctions, sliceLabels, width: net.width,
      label: labelText ? { x: D.x + 4, y: D.y - 3, text: labelText + (net.driver.slice ? ' ' + net.driver.slice : '') } : undefined,
    });
  }
  return out;
}
