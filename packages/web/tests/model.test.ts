import { readFileSync } from 'node:fs';
import { describe, expect, test } from 'vitest';
import { buildNetIndex, pathKey, traceNet, validateDesign } from '../src/model/design';
import { fromVerilensAst, type VlModule } from '../src/model/adapters/verilens';

const ast = JSON.parse(readFileSync(new URL('../samples/demo.ast.json', import.meta.url), 'utf8')) as VlModule[];
const design = validateDesign(fromVerilensAst(ast));

describe('VeriLens AST adapter', () => {
  test('picks the pragma top and keeps the hierarchy', () => {
    expect(design.top).toBe('top');
    expect(design.modules.top.cells.map((c) => c.id)).toEqual(['input_fifo', 'controller', 'compute', 'output_fifo']);
    expect(design.modules.compute.cells.filter((c) => c.kind === 'instance').map((c) => `${c.id}:${c.module}`)).toEqual(['pe0:pe', 'pe1:pe']);
  });

  test('lowers connection expressions to bit-level segments', () => {
    const pe1 = design.modules.compute.cells.find((c) => c.id === 'pe1')!;
    expect(pe1.connections.b).toEqual([{ net: 'acc', msb: 15, lsb: 0 }]);
    expect(pe1.params).toEqual({ WIDTH: '32' });
    const coeffBuf = design.modules.compute.cells.find((c) => c.type === 'buf' && c.connections.Y[0] && 'net' in c.connections.Y[0] && c.connections.Y[0].net === 'coeff')!;
    expect(coeffBuf.connections.A).toEqual([{ const: "12'd0" }, { net: 'coeff_sel' }]);
  });

  test('turns assigns into operator cells and always blocks into process cells', () => {
    const c = design.modules.compute;
    expect(c.cells.some((x) => x.type === 'add')).toBe(true);
    expect(c.cells.some((x) => x.type === 'mux')).toBe(true);
    const proc = c.cells.find((x) => x.type === 'process')!;
    expect(proc.attrs?.seq).toBe(true);
    const ports = proc.ports!;
    expect(ports.filter((p) => p.direction === 'output').map((p) => p.name).sort()).toEqual(['acc', 'data_out', 'done', 'valid_out']);
    expect(ports.filter((p) => p.direction === 'input').map((p) => p.name)).toContain('clk');
    // a signal both read and written is state, not an input
    expect(ports.filter((p) => p.name === 'acc')).toHaveLength(1);
  });

  test('handles positional connections, wildcards and unresolved modules', () => {
    const mini: VlModule[] = [
      { name: 'leaf', source_file: 'l.v', parameters: [], ports: [{ name: 'p', direction: 'input', type: 'logic', msb: 0, lsb: 0, scalar: true }, { name: 'q', direction: 'output', type: 'logic', msb: 0, lsb: 0, scalar: true }], net_decls: [], instances: [], gate_primitives: [], assigns: [], always_blocks: [] },
      { name: 'root', source_file: 'r.v', parameters: [], ports: [{ name: 'p', direction: 'input', type: 'logic', msb: 0, lsb: 0, scalar: true }, { name: 'q', direction: 'output', type: 'logic', msb: 0, lsb: 0, scalar: true }], net_decls: [{ name: 'w', type: 'wire', msb: 0, lsb: 0, scalar: true }],
        instances: [
          { module: 'leaf', instance: 'u0', resolved: true, wildcard: false, parameters: [], connections: [{ port: '', signal: 'p', expr: { kind: 'IDENTIFIER', name: 'p' } }, { port: '', signal: 'w', expr: { kind: 'IDENTIFIER', name: 'w' } }] },
          { module: 'leaf', instance: 'u1', resolved: true, wildcard: true, parameters: [], connections: [] },
          { module: 'ext', instance: 'bb', resolved: false, wildcard: false, parameters: [], connections: [{ port: 'x', signal: 'w', expr: { kind: 'IDENTIFIER', name: 'w' } }] },
        ], gate_primitives: [{ type: 'and', instance: 'g1', ports: ['q', 'p', 'w'] }], assigns: [], always_blocks: [] },
    ];
    const d = validateDesign(fromVerilensAst(mini));
    expect(d.top).toBe('root');
    const root = d.modules.root;
    expect(root.cells.find((c) => c.id === 'u0')!.connections).toEqual({ p: [{ net: 'p' }], q: [{ net: 'w' }] });
    expect(root.cells.find((c) => c.id === 'u1')!.connections).toEqual({ p: [{ net: 'p' }], q: [{ net: 'q' }] });
    const bb = root.cells.find((c) => c.id === 'bb')!;
    expect(bb.resolved).toBe(false);
    expect(bb.ports).toEqual([{ name: 'x', direction: 'inout', width: null }]);
    const g = root.cells.find((c) => c.id === 'g1')!;
    expect(g.connections).toEqual({ Y: [{ net: 'q' }], A1: [{ net: 'p' }], A2: [{ net: 'w' }] });
  });
});

describe('VeriLens AST adapter: SystemVerilog additions', () => {
  const base = { source_file: 'x.sv', parameters: [], net_decls: [], instances: [], gate_primitives: [], assigns: [], always_blocks: [] };
  const port = (name: string, direction: 'input' | 'output'): VlModule['ports'][number] => ({ name, direction, type: 'logic', msb: 0, lsb: 0, scalar: true });

  test('function calls become call cells and never implicit nets', () => {
    const m: VlModule = { ...base, name: 'm', ports: [port('a', 'input'), port('b', 'input'), port('y', 'output')],
      assigns: [{ lhs: { kind: 'IDENTIFIER', name: 'y' }, rhs: { kind: 'BINARY_OP', op: 'call', lhs: { kind: 'IDENTIFIER', name: 'f' }, rhs: { kind: 'CONCAT', parts: [{ kind: 'IDENTIFIER', name: 'a' }, { kind: 'IDENTIFIER', name: 'b' }] } } }],
      always_blocks: [{ sensitivity: '*', body: { kind: 'BLOCKING_ASSIGN', lhs: { kind: 'IDENTIFIER', name: 'y' }, rhs: { kind: 'BINARY_OP', op: 'call', lhs: { kind: 'IDENTIFIER', name: '$clog2' }, rhs: { kind: 'CONCAT', parts: [{ kind: 'IDENTIFIER', name: 'a' }] } } } }] };
    const d = validateDesign(fromVerilensAst([m]));
    const call = d.modules.m.cells.find((c) => c.type === 'call')!;
    expect(call.label).toBe('f()');
    expect(call.connections).toEqual({ A1: [{ net: 'a' }], A2: [{ net: 'b' }], Y: [{ net: 'y' }] });
    expect(d.modules.m.nets.map((n) => n.id)).not.toContain('f');
    expect(d.modules.m.nets.map((n) => n.id)).not.toContain('$clog2');
    const proc = d.modules.m.cells.find((c) => c.type === 'process')!;
    expect(proc.ports!.map((p) => p.name)).toEqual(['a', 'y']);
  });

  test('instance arrays keep one cell with the range, notes land in module attrs', () => {
    const leaf: VlModule = { ...base, name: 'leaf', ports: [port('i', 'input'), port('o', 'output')] };
    const m: VlModule = { ...base, name: 'm', ports: [port('a', 'input'), port('y', 'output')],
      instances: [{ module: 'leaf', instance: 'u', resolved: true, wildcard: false, array: '3:0', parameters: [], connections: [{ port: 'i', signal: 'a', expr: { kind: 'IDENTIFIER', name: 'a' } }, { port: 'o', signal: 'y', expr: { kind: 'IDENTIFIER', name: 'y' } }] }],
      gate_primitives: [{ type: 'buf', instance: 'b', ports: ['y', 'a'], array: '1:0' }],
      notes: [{ severity: 'error', line: 7, message: "dropped: expected SEMICOLON, got '#'" }] };
    const d = validateDesign(fromVerilensAst([leaf, m]));
    expect(d.modules.m.cells.find((c) => c.id === 'u')!.attrs).toEqual({ array: '3:0' });
    expect(d.modules.m.cells.find((c) => c.id === 'b')!.attrs).toEqual({ array: '1:0' });
    expect(d.modules.m.attrs?.notes).toEqual(m.notes);
  });
});

describe('design model', () => {
  test('validation reports dangling references', () => {
    expect(() => validateDesign({ version: 1, top: 'x', modules: {} })).toThrow(/top module 'x' not found/);
    expect(() => validateDesign({ version: 1, top: 'm', modules: { m: { name: 'm', ports: [], nets: [], cells: [{ id: 'c', kind: 'instance', module: 'nope', connections: {} }] } } })).toThrow(/unknown module 'nope'/);
  });

  test('net index derives drivers and sinks from port directions', () => {
    const idx = buildNetIndex(design, design.modules.top);
    const fifo = idx.get('fifo_data')!;
    expect(fifo.drivers.map((e) => `${e.cell}.${e.port}`)).toEqual(['input_fifo.rd_data']);
    expect(fifo.sinks.map((e) => `${e.cell}.${e.port}`)).toEqual(['compute.data_in']);
    const clk = idx.get('clk')!;
    expect(clk.drivers[0].cell).toBeUndefined();
    expect(clk.sinks.length).toBe(4);
  });

  test('net tracing follows real connectivity across module boundaries both ways', () => {
    const down = traceNet(design, ['top'], 'fifo_data').map((t) => `${pathKey(t.path)}:${t.net}`);
    expect(down).toContain('top/input_fifo:rd_data');
    expect(down).toContain('top/compute:data_in');
    expect(down).toContain('top/compute/pe0:a');
    expect(down).not.toContain('top/compute/pe1:a');
    const up = traceNet(design, ['top', 'compute', 'pe0'], 'y').map((t) => `${pathKey(t.path)}:${t.net}`);
    expect(up).toContain('top/compute:pe0_out');
    expect(up).toContain('top/compute/pe1:a');
    expect(up).not.toContain('top:fifo_data');
  });
});
