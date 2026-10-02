import { pathKey } from '../model/design';
import type { Diagnostic } from '../layout/types';
import { setUi, useStore, visibleScopes } from '../state/store';

export function StatusBar() {
  const selection = useStore((s) => s.selection);
  const focus = useStore((s) => s.focus);
  const layoutDiagnostics = useStore((s) => s.layoutDiagnostics);
  const layouts = useStore((s) => s.layouts);
  const failed = useStore((s) => s.failed);
  const diagOpen = useStore((s) => s.diagOpen);
  const designError = useStore((s) => s.designError);
  const state = useStore((s) => s);
  const diags: Diagnostic[] = [...layoutDiagnostics];
  for (const p of visibleScopes(state)) {
    const k = pathKey(p);
    if (layouts[k]) diags.push(...layouts[k].diagnostics);
    if (failed[k]) diags.push(...failed[k]);
  }
  if (designError) diags.unshift({ severity: 'error', message: designError });
  const errors = diags.filter((d) => d.severity === 'error').length;
  const warnings = diags.filter((d) => d.severity === 'warning').length;
  const selText = !selection ? 'nothing selected' : selection.kind === 'cell' ? `cell ${pathKey(selection.path)}` : `net ${selection.net} in ${pathKey(selection.path)}`;
  return (
    <>
      {diagOpen && (
        <div className="diag-drawer" data-testid="diagnostics">
          {diags.length === 0 && <div className="hint">No diagnostics.</div>}
          <ul>{diags.map((d, i) => (
            <li key={i} className={d.severity}>
              <b>{d.severity}</b> {d.scope && <code>{d.scope}</code>} {d.constraints?.length ? <code>[{d.constraints.join(', ')}]</code> : null} {d.message}
            </li>
          ))}</ul>
        </div>
      )}
      <footer className="statusbar">
        <span data-testid="status-selection">{selText}</span>
        <span className="sep" />
        <span>focus: {pathKey(focus) || '—'}</span>
        <span className="grow" />
        <button className={`diag-btn ${errors ? 'has-errors' : warnings ? 'has-warnings' : ''}`} onClick={() => setUi({ diagOpen: !diagOpen })}>
          {errors} errors · {warnings} warnings · {diags.length - errors - warnings} info
        </button>
      </footer>
    </>
  );
}
