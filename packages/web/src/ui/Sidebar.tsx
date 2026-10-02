import { useMemo, useState } from 'react';
import type { Design, InstancePath } from '../model/design';
import { allInstancePaths, moduleAt, pathKey } from '../model/design';
import { applyLayoutText, focusModule, locateInstance, setLayoutText, setUi, useStore } from '../state/store';
import { SAMPLE_LAYOUT, SAMPLE_CONFLICT_LAYOUT, reloadSession } from '../app/io';

export function Sidebar() {
  const tab = useStore((s) => s.sidebarTab);
  const design = useStore((s) => s.design);
  return (
    <aside className="sidebar left">
      <div className="tabs">
        {(['hierarchy', 'modules', 'layout'] as const).map((t) => (
          <button key={t} className={t === tab ? 'active' : ''} onClick={() => setUi({ sidebarTab: t })}>{t}</button>
        ))}
      </div>
      <div className="panel-body">
        {!design && <div className="hint">No design loaded.</div>}
        {design && tab === 'hierarchy' && <HierarchyTree design={design} />}
        {design && tab === 'modules' && <ModulesList design={design} />}
        {tab === 'layout' && <LayoutEditor />}
      </div>
    </aside>
  );
}

function HierarchyTree({ design }: { design: Design }) {
  const focus = useStore((s) => s.focus);
  const selection = useStore((s) => s.selection);
  const [collapsed, setCollapsed] = useState<Record<string, boolean>>({});
  const selKey = selection?.kind === 'cell' ? pathKey(selection.path) : '';
  const focusKey = pathKey(focus);
  const Node = ({ path }: { path: InstancePath }) => {
    const mod = moduleAt(design, path);
    if (!mod) return null;
    const key = pathKey(path);
    const children = mod.cells.filter((c) => c.kind === 'instance' && c.module && design.modules[c.module]);
    const isCollapsed = collapsed[key] ?? path.length > 2;
    return (
      <li>
        <div className={`tree-row ${key === selKey ? 'selected' : ''} ${key === focusKey ? 'focused' : ''}`} data-tree={key}>
          <button className="caret" disabled={!children.length} onClick={() => setCollapsed((c) => ({ ...c, [key]: !isCollapsed }))}>{children.length ? (isCollapsed ? '▸' : '▾') : '·'}</button>
          <button className="label" onClick={() => locateInstance(path)} title={key}>
            <span className="inst">{path[path.length - 1]}</span> <span className="mod">{mod.name}</span>
          </button>
          <button className="focus-btn" title="Focus module" onClick={() => focusModule(path)}>⤢</button>
        </div>
        {!isCollapsed && children.length > 0 && (
          <ul>{children.map((c) => <Node key={c.id} path={[...path, c.id]} />)}</ul>
        )}
      </li>
    );
  };
  return <ul className="tree"><Node path={[design.top]} /></ul>;
}

function ModulesList({ design }: { design: Design }) {
  const paths = useMemo(() => allInstancePaths(design), [design]);
  const focus = useStore((s) => s.focus);
  const focusMod = moduleAt(design, focus)?.name;
  return (
    <ul className="modules">
      {Object.values(design.modules).map((m) => {
        const inst = paths.filter((p) => moduleAt(design, p)?.name === m.name);
        return (
          <li key={m.name} className={m.name === focusMod ? 'focused' : ''}>
            <button className="label" disabled={!inst.length} onClick={() => inst[0] && focusModule(inst[0])} title={inst.map(pathKey).join('\n') || 'not instantiated under top'}>
              <span className="inst">{m.name}</span>
              <span className="mod">{inst.length} inst · {m.cells.length} cells · {m.ports.length} ports{m.blackbox ? ' · black box' : ''}</span>
            </button>
          </li>
        );
      })}
    </ul>
  );
}

function LayoutEditor() {
  const text = useStore((s) => s.layoutText);
  const diags = useStore((s) => s.layoutDiagnostics);
  const file = useStore((s) => s.layoutFile);
  const errors = diags.filter((d) => d.severity === 'error');
  return (
    <div className="layout-editor">
      <div className="row">
        <button onClick={() => applyLayoutText(text)} title="Validate and apply the rules (recomputes geometry)">Apply</button>
        <button onClick={() => { setLayoutText(SAMPLE_LAYOUT); applyLayoutText(SAMPLE_LAYOUT); }}>Sample</button>
        <button onClick={() => { setLayoutText(SAMPLE_CONFLICT_LAYOUT); applyLayoutText(SAMPLE_CONFLICT_LAYOUT); }}>Conflicting</button>
        <button onClick={() => { setLayoutText(''); applyLayoutText(''); }}>Clear</button>
        <button onClick={() => void reloadSession().then((ok) => { if (!ok) alert('No launcher session: start with ./verilens <inputs> --dev'); })} title="Re-read the design and layout file given to ./verilens --dev">Reload files</button>
      </div>
      <textarea spellCheck={false} value={text} onChange={(e) => setLayoutText(e.target.value)} placeholder={'version: 1\nscopes:\n  - scope: top\n    constraints: []'} data-testid="layout-text" />
      <div className="status-line">{errors.length ? `${errors.length} error(s): rules rejected, previous layout kept` : file ? `${file.scopes.length} scope(s) active` : 'no rules: automatic layout'}</div>
      {errors.length > 0 && (
        <ul className="diag-list" data-testid="layout-errors">{errors.map((d, i) => <li key={i} className="error">{d.message}</li>)}</ul>
      )}
    </div>
  );
}
