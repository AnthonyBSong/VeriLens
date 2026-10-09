// Loading designs (auto-detected format), samples, and self-contained HTML export.
import { parseVerilog } from './parser';
import type { Design } from '../model/design';
import { validateDesign } from '../model/design';
import { fromVerilensAst, isVerilensAst } from '../model/adapters/verilens';
import demoAst from '../../samples/demo.ast.json';
import demoLayout from '../../samples/demo.layout.yaml?raw';
import conflictLayout from '../../samples/demo-conflict.layout.yaml?raw';
import { applyLayoutText, getState, loadDesign, setDesignError, setLayoutText } from '../state/store';

export const SAMPLE_LAYOUT: string = demoLayout;
export const SAMPLE_CONFLICT_LAYOUT: string = conflictLayout;

/** Accepts VeriLens gen_ast output (array of modules) or a normalized design object. */
export function designFromJson(text: string): Design {
  const obj: unknown = JSON.parse(text);
  if (isVerilensAst(obj)) return validateDesign(fromVerilensAst(obj));
  return validateDesign(obj);
}

export function loadSample(which: 'auto' | 'layout' | 'conflict') {
  const design = validateDesign(fromVerilensAst(demoAst as Parameters<typeof fromVerilensAst>[0]));
  setLayoutText(which === 'layout' ? SAMPLE_LAYOUT : which === 'conflict' ? SAMPLE_CONFLICT_LAYOUT : '');
  loadDesign(design, `demo (${which === 'auto' ? 'automatic layout' : which === 'layout' ? 'with layout rules' : 'conflicting rules'})`);
}

const isVerilog = (name: string) => /\.s?v$/i.test(name);

/** Open/drop: .v/.sv sources are parsed in the browser (parser.ts); JSON is a
 *  design; YAML is layout rules. Several sources at once are linked as one design. */
export async function openFiles(files: File[]) {
  const sources = files.filter((f) => isVerilog(f.name));
  try {
    if (sources.length) {
      const data = await parseVerilog(await Promise.all(sources.map(async (f) => ({ name: f.name, text: await f.text() }))));
      if (data.diagnostics) console.warn(data.diagnostics);
      loadDesign(designFromJson(JSON.stringify(data.ast)), sources.map((f) => f.name).join(' + '));
    }
    for (const f of files) {
      if (isVerilog(f.name)) continue;
      const text = await f.text();
      if (/\.ya?ml$/i.test(f.name)) { setLayoutText(text); applyLayoutText(text); continue; }
      loadDesign(designFromJson(text), f.name);
    }
  } catch (e) {
    setDesignError(`${files.map((f) => f.name).join(', ')}: ${(e as Error).message}`);
  }
}

const DATA_ID = 'verilens-data';

export interface Session { ast: unknown; layoutText: string; top?: string; name: string; layoutPath?: string }

/** Dev server only: the launcher (`./verilens --dev`) serves the generated AST + layout here. */
export async function fetchSession(): Promise<Session | null> {
  try {
    const r = await fetch('/__verilens/session.json', { cache: 'no-store' });
    if (!r.ok) return null;
    return (await r.json()) as Session;
  } catch { return null; }
}

export function loadSession(s: Session) {
  const design = isVerilensAst(s.ast) ? validateDesign(fromVerilensAst(s.ast, { top: s.top })) : validateDesign(s.top ? { ...(s.ast as object), top: s.top } : s.ast);
  setLayoutText(s.layoutText ?? '');
  loadDesign(design, s.name);
}

/** Re-read the launcher session (picks up edits to the layout file on disk). */
export async function reloadSession(): Promise<boolean> {
  const s = await fetchSession();
  if (!s) return false;
  try { loadSession(s); return true; } catch (e) { setDesignError((e as Error).message); return false; }
}

/** On boot: a self-contained export embeds design + rules in a JSON script tag. */
export function bootFromEmbedded(): boolean {
  const el = document.getElementById(DATA_ID);
  if (!el?.textContent) return false;
  try {
    const data = JSON.parse(el.textContent) as { design: Design; layoutText?: string; name?: string };
    setLayoutText(data.layoutText ?? '');
    loadDesign(validateDesign(data.design), data.name ?? 'embedded design');
    return true;
  } catch (e) {
    setDesignError(`embedded data: ${(e as Error).message}`);
    return false;
  }
}

/** Serialize the running page (built as a single file) with the current design + rules embedded. */
export function exportHtml() {
  const s = getState();
  if (!s.design) return;
  const doc = document.documentElement.cloneNode(true) as HTMLElement;
  doc.querySelector('#' + DATA_ID)?.remove();
  const root = doc.querySelector('#root'); if (root) root.innerHTML = '';
  const script = document.createElement('script');
  script.type = 'application/json'; script.id = DATA_ID;
  // '</script' inside JSON would end the tag early
  script.textContent = JSON.stringify({ design: s.design, layoutText: s.layoutText, name: s.designName }).replace(/<\/script/gi, '<\\/script');
  doc.querySelector('head')?.appendChild(script);
  const html = '<!doctype html>\n' + doc.outerHTML;
  const blob = new Blob([html], { type: 'text/html' });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = (s.designName.replace(/[^\w.-]+/g, '_') || 'design') + '.verilens.html';
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 1000);
}
