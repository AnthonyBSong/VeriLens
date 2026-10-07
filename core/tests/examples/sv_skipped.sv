// Constructs that are skipped on purpose and reported in Module::notes (TODO.md group 3),
// plus interface ports (phase 0: honest unknown-type ports, interface instance as black box).
interface sv_bus_if (input logic clk);
  logic d;
  modport mst (output d);
  modport slv (input  d);
endinterface

module sv_skipped_leaf (sv_bus_if.slv s, sv_bus_if t, input logic a, output logic y);
  assign y = a;
endmodule

module sv_skipped (input logic clk, a, output logic y, z);
  timeunit 1ns; timeprecision 1ps;
  import sv_types_pkg::*;
  logic [7:0] mem [4];
  event ev;
  real  r;
  time  tm;
  let both(x, w) = x & w;

  initial begin
    $readmemh("mem.hex", mem);
  end
  final $display("done");

  function automatic logic f(input logic x);
    return x;
  endfunction : f
  task automatic t();
  endtask : t

  specify
    (a => y) = 1;
  endspecify

  default clocking cb @(posedge clk); endclocking
  clocking cb2 @(posedge clk); endclocking
  covergroup cg @(posedge clk);
    coverpoint a;
  endgroup

  property p_hold; @(posedge clk) a |-> ##1 y; endproperty
  sequence s_ab; a ##1 y; endsequence
  assert property (@(posedge clk) a |-> y);
  ap_hold: assert property (p_hold) else $error("hold");
  assume property (@(posedge clk) a);
  cover property (s_ab);
  always @(posedge clk) assert (a) else $error("bad");

  sv_bus_if bus (.clk(clk));
  sv_skipped_leaf u_leaf (.s(bus), .t(bus), .a(a), .y(y));
  assign z = f(a);
endmodule
