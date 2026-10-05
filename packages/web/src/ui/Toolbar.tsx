import { useMemo, useRef, useState } from 'react';
import { allInstancePaths, moduleAt, pathKey } from '../model/design';
import { exportHtml, loadSample, openFile } from '../app/io';
import { fitDesign, fitSelection, forceRelayout, goBack, locateInstance, locateNet, setUi, useStore } from '../state/store';

export function Toolbar() {
  const design = useStore((s) => s.design);
  const designName = useStore((s) => s.designName);
  const history = useStore((s) => s.history);
  const leftOpen = useStore((s) => s.leftOpen);
  const rightOpen = useStore((s) => s.rightOpen);
  const fileRef = useRef<HTMLInputElement>(null);
  return (
    <header className="toolbar">
      <span className="brand">VeriLens</span>
      <button onClick={() => fileRef.current?.click()} title="Open a design JSON (gen_ast output or normalized) or a layout YAML">Open…</button>
      <input ref={fileRef} type="file" accept=".json,.yaml,.yml" hidden onChange={(e) => { const f = e.target.files?.[0]; if (f) void openFile(f); e.target.value = ''; }} />
      <select value="" onChange={(e) => { if (e.target.value) loadSample(e.target.value as 'auto'); e.target.value = ''; }} title="Load a sample">
        <option value="">Samples…</option>
        <option value="auto">demo · automatic layout</option>
        <option value="layout">demo · with layout rules</option>
        <option value="conflict">demo · conflicting rules</option>
      </select>
      <span className="sep" />
      <button onClick={goBack} disabled={!history.length} title="Restore the previous view">◀ Back</button>
      <button onClick={fitDesign} disabled={!design} title="Fit the whole schematic">Fit design</button>
      <button onClick={fitSelection} disabled={!design} title="Fit the selection">Fit selection</button>
      <button onClick={forceRelayout} disabled={!design} title="Recompute geometry from the current rules">Re-layout</button>
      <span className="sep" />
      <Search />
      <span className="grow" />
      <span className="design-name" title={designName}>{designName}</span>
      <button onClick={exportHtml} disabled={!design} title="Download a self-contained interactive HTML file">Export HTML</button>
      <span className="sep" />
      <button className={leftOpen ? 'active' : ''} onClick={() => setUi({ leftOpen: !leftOpen })} title="Toggle hierarchy panel">⫷</button>
      <button className={rightOpen ? 'active' : ''} onClick={() => setUi({ rightOpen: !rightOpen })} title="Toggle inspector">⫸</button>
    </header>
  );
}

function Search() {
  const design = useStore((s) => s.design);
  const [q, setQ] = useState('');
  const [open, setOpen] = useState(false);
  const results = useMemo(() => {
    if (!design || q.trim().length < 1) return [];
    const needle = q.trim().toLowerCase();
    const out: { label: string; sub: string; go: () => void }[] = [];
    const paths = allInstancePaths(design);
    for (const p of paths) {
      const key = pathKey(p);
      if (key.toLowerCase().includes(needle)) out.push({ label: key, sub: moduleAt(design, p)?.name ?? '', go: () => locateInstance(p) });
    }
    for (const p of paths) {
      const mod = moduleAt(design, p); if (!mod) continue;
      for (const n of mod.nets) if (!n.id.startsWith('$') && n.id.toLowerCase().includes(needle)) out.push({ label: n.id, sub: `net in ${pathKey(p)}`, go: () => locateNet(p, n.id) });
      if (out.length > 40) break;
    }
    return out.slice(0, 12);
  }, [design, q]);
  return (
    <div className="search">
      <input value={q} placeholder="search instance path or net" disabled={!design} data-testid="search"
        onChange={(e) => { setQ(e.target.value); setOpen(true); }} onFocus={() => setOpen(true)} onBlur={() => setTimeout(() => setOpen(false), 150)}
        onKeyDown={(e) => { if (e.key === 'Enter' && results[0]) { results[0].go(); setOpen(false); } if (e.key === 'Escape') setOpen(false); }} />
      {open && results.length > 0 && (
        <ul className="results">
          {results.map((r) => <li key={r.sub + r.label}><button onMouseDown={(e) => e.preventDefault()} onClick={() => { r.go(); setOpen(false); }}><b>{r.label}</b> <span>{r.sub}</span></button></li>)}
        </ul>
      )}
    </div>
  );
}
