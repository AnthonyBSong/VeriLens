import { useEffect } from 'react';
import { bootFromEmbedded, loadSample } from '../app/io';
import { fitDesign, focusModule, select, useStore } from '../state/store';
import { Canvas } from './Canvas';
import { Inspector } from './Inspector';
import { Sidebar } from './Sidebar';
import { StatusBar } from './StatusBar';
import { Toolbar } from './Toolbar';

export function App() {
  const leftOpen = useStore((s) => s.leftOpen);
  const rightOpen = useStore((s) => s.rightOpen);
  useEffect(() => {
    if (!bootFromEmbedded()) {
      const params = new URLSearchParams(location.search);
      const which = params.get('sample');
      loadSample(which === 'layout' || which === 'conflict' ? which : 'auto');
      const focus = params.get('focus');
      if (focus) focusModule(focus.split('/'), false);
    }
    const onKey = (e: KeyboardEvent) => {
      if ((e.target as HTMLElement)?.tagName === 'INPUT' || (e.target as HTMLElement)?.tagName === 'TEXTAREA') return;
      if (e.key === 'Escape') select(null);
      if (e.key === 'f') fitDesign();
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, []);
  return (
    <div className={`app ${leftOpen ? '' : 'no-left'} ${rightOpen ? '' : 'no-right'}`}>
      <Toolbar />
      {leftOpen && <Sidebar />}
      <Canvas />
      {rightOpen && <Inspector />}
      <StatusBar />
    </div>
  );
}
