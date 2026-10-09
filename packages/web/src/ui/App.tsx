import { useEffect } from 'react';
import { bootFromEmbedded, fetchSession, loadSample, loadSession, openFiles } from '../app/io';
import { fitDesign, focusModule, select, setDesignError, useStore } from '../state/store';
import { Canvas } from './Canvas';
import { Inspector } from './Inspector';
import { Sidebar } from './Sidebar';
import { StatusBar } from './StatusBar';
import { Toolbar } from './Toolbar';

export function App() {
  const leftOpen = useStore((s) => s.leftOpen);
  const rightOpen = useStore((s) => s.rightOpen);
  useEffect(() => {
    const params = new URLSearchParams(location.search);
    const boot = async () => {
      if (bootFromEmbedded()) return;
      const session = params.get('sample') ? null : await fetchSession();
      if (session) {
        try { loadSession(session); } catch (e) { setDesignError(`session: ${(e as Error).message}`); }
      } else {
        const which = params.get('sample');
        loadSample(which === 'layout' || which === 'conflict' ? which : 'auto');
      }
      const focus = params.get('focus');
      if (focus) focusModule(focus.split('/'), false);
    };
    void boot();
    const onKey = (e: KeyboardEvent) => {
      if ((e.target as HTMLElement)?.tagName === 'INPUT' || (e.target as HTMLElement)?.tagName === 'TEXTAREA') return;
      if (e.key === 'Escape') select(null);
      if (e.key === 'f') fitDesign();
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, []);
  return (
    <div className={`app ${leftOpen ? '' : 'no-left'} ${rightOpen ? '' : 'no-right'}`}
      onDragOver={(e) => e.preventDefault()}
      onDrop={(e) => { e.preventDefault(); if (e.dataTransfer.files.length) void openFiles(Array.from(e.dataTransfer.files)); }}>
      <Toolbar />
      {leftOpen && <Sidebar />}
      <Canvas />
      {rightOpen && <Inspector />}
      <StatusBar />
    </div>
  );
}
