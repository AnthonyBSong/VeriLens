import { readFileSync } from 'node:fs';
import { describe, expect, test } from 'vitest';
import { parseVerilog } from '../src/app/parser';
import { designFromJson } from '../src/app/io';

const demo = readFileSync(new URL('../samples/demo/demo.sv', import.meta.url), 'utf8');

describe('browser parser (wasm)', () => {
  test('parses and links sources into the same AST shape gen_ast emits', async () => {
    const r = await parseVerilog([{ name: 'demo.sv', text: demo }, { name: 'extra.sv', text: 'module extra(input a, output b); assign b = a; endmodule' }]);
    const golden = JSON.parse(readFileSync(new URL('../samples/demo.ast.json', import.meta.url), 'utf8')) as { name: string }[];
    const names = (r.ast as { name: string }[]).map((m) => m.name);
    expect(names).toEqual([...golden.map((m) => m.name), 'extra']);
    expect(designFromJson(JSON.stringify(r.ast)).top).toBe(designFromJson(JSON.stringify(golden)).top);
  });

  test('a source that fails to parse is reported in diagnostics, never aborts the module', async () => {
    const r = await parseVerilog([{ name: 'bad.sv', text: 'module m(input a; assign = ; endmodule' }, { name: 'ok.sv', text: 'module ok; endmodule' }]);
    expect((r.ast as { name: string }[]).map((m) => m.name)).toContain('ok');
    expect(r.diagnostics).toMatch(/bad\.sv|error|warning/i);
  });
});
