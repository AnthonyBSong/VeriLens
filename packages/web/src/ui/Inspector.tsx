import { useMemo } from 'react';
import type { Design, Segment } from '../model/design';
import { buildNetIndex, cellPorts, isConst, isExpandable, moduleAt, pathKey, traceNet } from '../model/design';
import { fitSelection, focusModule, locateNet, select, toggleExpand, useStore } from '../state/store';

const segText = (s: Segment) => isConst(s) ? s.const : s.msb === undefined ? s.net : s.msb === s.lsb ? `${s.net}[${s.msb}]` : `${s.net}[${s.msb}:${s.lsb}]`;

export function Inspector() {
  const design = useStore((s) => s.design);
  const selection = useStore((s) => s.selection);
  const focus = useStore((s) => s.focus);
  const layouts = useStore((s) => s.layouts);
  const expanded = useStore((s) => s.expanded);
  if (!design) return <aside className="sidebar right"><div className="panel-body hint">No design loaded.</div></aside>;

  let body: React.ReactNode;
  if (selection?.kind === 'cell') {
    const parentPath = selection.path.slice(0, -1);
    const mod = moduleAt(design, parentPath);
    const cell = mod?.cells.find((c) => c.id === selection.path[selection.path.length - 1]);
    if (mod && cell) {
      const key = pathKey(selection.path);
      const expandable = isExpandable(design, cell);
      body = (
        <>
          <h3>{cell.id}</h3>
          <div className="kv"><span>path</span><code>{key}</code></div>
          <div className="kv"><span>kind</span><span>{cell.kind === 'instance' ? `instance of ${cell.module}${cell.resolved === false ? ' (black box)' : ''}` : `primitive ${cell.type}`}</span></div>
          {cell.source?.file && <div className="kv"><span>source</span><code>{cell.source.file}{cell.source.line ? `:${cell.source.line}` : ''}</code></div>}
          {cell.attrs?.sensitivity ? <div className="kv"><span>sensitivity</span><code>{String(cell.attrs.sensitivity)}</code></div> : null}
          {cell.attrs?.array ? <div className="kv"><span>array</span><span>one cell stands for <code>{cell.id}[{String(cell.attrs.array)}]</code></span></div> : null}
          {cell.params && Object.keys(cell.params).length > 0 && (
            <div className="kv"><span>params</span><span>{Object.entries(cell.params).map(([k, v]) => `${k}=${v}`).join(', ')}</span></div>
          )}
          <div className="row">
            {expandable && <button onClick={() => focusModule(selection.path)}>Focus module</button>}
            {expandable && <button onClick={() => toggleExpand(selection.path)}>{expanded[key] ? 'Collapse' : 'Expand'}</button>}
            <button onClick={() => fitSelection()}>Fit</button>
          </div>
          <table className="ports">
            <thead><tr><th>port</th><th>dir</th><th>w</th><th>net</th></tr></thead>
            <tbody>
              {cellPorts(design, cell).map((p) => {
                const segs = cell.connections[p.name] ?? [];
                return (
                  <tr key={p.name}>
                    <td>{p.name}</td><td>{p.direction}</td><td>{p.width ?? p.widthExpr ?? '?'}</td>
                    <td>{segs.length ? segs.map((s, i) => isConst(s)
                      ? <code key={i}>{s.const}</code>
                      : <button key={i} className="link" onClick={() => select({ kind: 'net', path: parentPath, net: s.net })}>{segText(s)}</button>)
                      .reduce<React.ReactNode[]>((acc, el, i) => (i ? [...acc, ', ', el] : [el]), [])
                      : <em>unconnected</em>}</td>
                  </tr>
                );
              })}
            </tbody>
          </table>
        </>
      );
    }
  } else if (selection?.kind === 'net') {
    body = <NetDetails design={design} path={selection.path} net={selection.net} />;
  }
  if (!body) {
    const mod = moduleAt(design, focus);
    const layout = layouts[pathKey(focus)];
    body = mod && (
      <>
        <h3>{mod.name}</h3>
        <div className="kv"><span>scope</span><code>{pathKey(focus)}</code></div>
        {mod.source?.file && <div className="kv"><span>source</span><code>{mod.source.file}{mod.source.line ? `:${mod.source.line}` : ''}</code></div>}
        <div className="kv"><span>cells</span><span>{mod.cells.length} ({mod.cells.filter((c) => c.kind === 'instance').length} instances)</span></div>
        <div className="kv"><span>nets</span><span>{mod.nets.length}</span></div>
        {layout && <div className="kv"><span>layout</span><span>{Math.round(layout.width)}×{Math.round(layout.height)}, {layout.wires.length} wires, {layout.wires.reduce((s, w) => s + w.junctions.length, 0)} junctions</span></div>}
        {layout && layout.undriven.length > 0 && <div className="kv"><span>undriven</span><span>{layout.undriven.join(', ')}</span></div>}
        <table className="ports">
          <thead><tr><th>port</th><th>dir</th><th>w</th></tr></thead>
          <tbody>{mod.ports.map((p) => (
            <tr key={p.name}><td><button className="link" onClick={() => select({ kind: 'net', path: focus, net: p.net ?? p.name })}>{p.name}</button></td><td>{p.direction}</td><td>{p.width ?? p.widthExpr ?? '?'}</td></tr>
          ))}</tbody>
        </table>
      </>
    );
  }
  return <aside className="sidebar right"><div className="panel-body" data-testid="inspector">{body}</div></aside>;
}

function NetDetails({ design, path, net }: { design: Design; path: string[]; net: string }) {
  const mod = moduleAt(design, path);
  const idx = useMemo(() => (mod ? buildNetIndex(design, mod) : null), [design, mod]);
  const trace = useMemo(() => traceNet(design, path, net), [design, path, net]);
  const info = idx?.get(net);
  const n = mod?.nets.find((x) => x.id === net);
  const ep = (e: { cell?: string; port: string; segment: Segment }) => (
    <li key={(e.cell ?? '') + ':' + e.port}>
      {e.cell ? <button className="link" onClick={() => select({ kind: 'cell', path: [...path, e.cell!] })}>{e.cell}</button> : <em>port</em>}.{e.port}
      {!isConst(e.segment) && e.segment.msb !== undefined ? <code> {segText(e.segment).slice(net.length)}</code> : null}
    </li>
  );
  return (
    <>
      <h3>net {net}</h3>
      <div className="kv"><span>scope</span><code>{pathKey(path)}</code></div>
      <div className="kv"><span>width</span><span>{n?.width ?? n?.widthExpr ?? '?'}{n?.implicit ? ' (implicit)' : ''}</span></div>
      <div className="row"><button onClick={() => fitSelection()}>Fit</button></div>
      <h4>drivers ({info?.drivers.length ?? 0})</h4>
      <ul className="endpoints">{info?.drivers.map(ep)}</ul>
      <h4>sinks ({info?.sinks.length ?? 0})</h4>
      <ul className="endpoints">{info?.sinks.map(ep)}</ul>
      {trace.length > 1 && (
        <>
          <h4>across hierarchy</h4>
          <ul className="endpoints">
            {trace.filter((t) => pathKey(t.path) !== pathKey(path) || t.net !== net).map((t) => (
              <li key={pathKey(t.path) + ':' + t.net}><button className="link" onClick={() => locateNet(t.path, t.net)}>{pathKey(t.path)}</button> : {t.net}</li>
            ))}
          </ul>
        </>
      )}
    </>
  );
}
