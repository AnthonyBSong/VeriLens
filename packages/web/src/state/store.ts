// Application state. Four separate concerns, never mixed:
//   design (what exists) | layout rules (LayoutFile) | geometry (ModuleLayout cache) | UI state.
// Geometry changes only on: design load, layout-file apply, explicit re-layout.
import { useSyncExternalStore } from 'react';
import type { Design, InstancePath } from '../model/design';
import { moduleAt, pathKey } from '../model/design';
import { parseLayoutFile, type LayoutFile } from '../layout/dsl';
import { layoutModule } from '../layout/engine';
import { LayoutJobs } from '../layout/jobs';
import type { Diagnostic, ModuleLayout } from '../layout/types';

export interface Camera { x: number; y: number; k: number }
export type Selection = { kind: 'cell'; path: InstancePath } | { kind: 'net'; path: InstancePath; net: string };

export interface State {
  design: Design | null;
  designName: string;
  designError: string | null;
  layoutText: string;
  layoutFile: LayoutFile | null;
  layoutDiagnostics: Diagnostic[];
  /** Last valid layout per scope key. */
  layouts: Record<string, ModuleLayout>;
  /** Diagnostics of the latest failed attempt per scope key (last valid layout is retained). */
  failed: Record<string, Diagnostic[]>;
  focus: InstancePath;
  history: { focus: InstancePath; camera: Camera }[];
  camera: Camera;
  viewport: { w: number; h: number };
  expanded: Record<string, true>;
  selection: Selection | null;
  pendingFit: 'design' | 'selection' | null;
  leftOpen: boolean;
  rightOpen: boolean;
  diagOpen: boolean;
  sidebarTab: 'hierarchy' | 'modules' | 'layout';
  layoutGeneration: number;
}

const initial: State = {
  design: null, designName: '', designError: null,
  layoutText: '', layoutFile: null, layoutDiagnostics: [],
  layouts: {}, failed: {},
  focus: [], history: [],
  camera: { x: 0, y: 0, k: 1 }, viewport: { w: 800, h: 600 },
  expanded: {}, selection: null, pendingFit: null,
  leftOpen: true, rightOpen: true, diagOpen: false, sidebarTab: 'hierarchy',
  layoutGeneration: 0,
};

let state: State = initial;
const listeners = new Set<() => void>();
export const jobs = new LayoutJobs();

export function getState() { return state; }
function set(patch: Partial<State> | ((s: State) => Partial<State>)) {
  const p = typeof patch === 'function' ? patch(state) : patch;
  state = { ...state, ...p };
  listeners.forEach((l) => l());
}
export function subscribe(l: () => void) { listeners.add(l); return () => listeners.delete(l); }
export function useStore<T>(sel: (s: State) => T): T {
  return useSyncExternalStore(subscribe, () => sel(state), () => sel(state));
}
/** Test hook: reset everything. */
export function resetStore() { state = initial; jobs.reset(); listeners.forEach((l) => l()); }

// ---------- layout jobs ----------

/** Compute (or reuse) the layout for a scope. Async and versioned: stale results are dropped. */
export function ensureLayout(path: InstancePath, force = false): Promise<void> {
  const key = pathKey(path);
  const { design, layoutFile } = state;
  if (!design) return Promise.resolve();
  if (!force && state.layouts[key]) return Promise.resolve();
  const mod = moduleAt(design, path);
  if (!mod) return Promise.resolve();
  const file = layoutFile;
  return jobs.run(key, () => layoutModule(design, path, mod, file)).then((res) => {
    if (!res) return; // stale
    if (res.layout.ok) set((s) => ({ layouts: { ...s.layouts, [key]: res.layout }, failed: omit(s.failed, key), layoutGeneration: s.layoutGeneration + 1 }));
    else set((s) => ({ failed: { ...s.failed, [key]: res.layout.diagnostics } }));
  });
}
function omit<T>(r: Record<string, T>, k: string): Record<string, T> { const { [k]: _drop, ...rest } = r; void _drop; return rest; }

/** Every scope currently visible: the focus module plus expanded descendants. */
export function visibleScopes(s: State = state): InstancePath[] {
  const out: InstancePath[] = [];
  if (!s.design || !s.focus.length) return out;
  const walk = (path: InstancePath) => {
    out.push(path);
    const mod = moduleAt(s.design!, path);
    for (const c of mod?.cells ?? []) {
      if (c.kind === 'instance' && s.expanded[pathKey([...path, c.id])]) walk([...path, c.id]);
    }
  };
  walk(s.focus);
  return out;
}

async function relayoutVisible(force: boolean) {
  await Promise.all(visibleScopes().map((p) => ensureLayout(p, force)));
}

// ---------- actions ----------

export function loadDesign(design: Design, name: string) {
  jobs.reset();
  set({
    design, designName: name, designError: null, layouts: {}, failed: {},
    focus: [design.top], history: [], expanded: {}, selection: null, pendingFit: 'design',
    camera: { x: 0, y: 0, k: 1 },
  });
  // re-validate the current layout text against the new design, then lay out the top
  applyLayoutText(state.layoutText, false);
  void relayoutVisible(true);
}

export function setDesignError(message: string | null) { set({ designError: message }); }

export function setLayoutText(text: string) { set({ layoutText: text }); }

/** Validate + apply the layout DSL. On errors, rules are rejected and the last valid layouts stay. */
export function applyLayoutText(text: string, relayout = true): boolean {
  const { design } = state;
  if (!design) { set({ layoutText: text, layoutFile: null, layoutDiagnostics: [] }); return true; }
  if (!text.trim()) { set({ layoutText: text, layoutFile: null, layoutDiagnostics: [] }); if (relayout) forceRelayout(); return true; }
  const res = parseLayoutFile(text, design);
  set({ layoutText: text, layoutDiagnostics: res.diagnostics, layoutFile: res.file ?? state.layoutFile, diagOpen: res.file ? state.diagOpen : true });
  if (res.file && relayout) forceRelayout();
  return !!res.file;
}

/** Explicit Re-layout: recompute all visible scopes from the current rules. */
export function forceRelayout() {
  jobs.reset();
  set({ layouts: {}, failed: {} });
  void relayoutVisible(true);
}

export function setViewport(w: number, h: number) { if (w !== state.viewport.w || h !== state.viewport.h) set({ viewport: { w, h } }); }
export function setCamera(camera: Camera) { set({ camera }); }

export function fitDesign() { set({ pendingFit: 'design' }); }
export function fitSelection() { set({ pendingFit: state.selection ? 'selection' : 'design' }); }
export function consumeFit(camera: Camera) { set({ camera, pendingFit: null }); }

export function focusModule(path: InstancePath, pushHistory = true) {
  if (pathKey(path) === pathKey(state.focus)) return;
  set((s) => ({
    focus: path,
    history: pushHistory ? [...s.history, { focus: s.focus, camera: s.camera }] : s.history,
    pendingFit: 'design',
  }));
  void relayoutVisible(false);
}

export function goBack() {
  const h = state.history[state.history.length - 1];
  if (!h) return;
  set((s) => ({ focus: h.focus, camera: h.camera, history: s.history.slice(0, -1), pendingFit: null }));
  void relayoutVisible(false);
}

export function toggleExpand(path: InstancePath) {
  const key = pathKey(path);
  set((s) => {
    const expanded = { ...s.expanded };
    if (expanded[key]) delete expanded[key]; else expanded[key] = true;
    return { expanded };
  });
  if (state.expanded[key]) void ensureLayout(path);
}

export function select(selection: Selection | null) { set({ selection }); }

/** Locate an instance: show its parent module and select it. */
export function locateInstance(path: InstancePath) {
  if (path.length <= 1) { focusModule(path); set({ selection: null }); return; }
  const parent = path.slice(0, -1);
  focusModule(parent);
  set({ selection: { kind: 'cell', path }, pendingFit: 'selection' });
}

export function locateNet(path: InstancePath, net: string) {
  focusModule(path);
  set({ selection: { kind: 'net', path, net }, pendingFit: 'selection' });
}

export function setUi(patch: Partial<Pick<State, 'leftOpen' | 'rightOpen' | 'diagOpen' | 'sidebarTab'>>) { set(patch); }
