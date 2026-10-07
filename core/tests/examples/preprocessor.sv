// Compiler directives: evaluated by core/Preprocessor.cpp (TODO.md group 1)
`timescale 1ns / 1ps
`default_nettype none
`include "nonexistent_defs.svh"
`define W 8
`define ADD(a, b) ((a) + (b))
`define MUX(s, x, y) \
  ((s) ? (x) : \
         (y))
`define FEATURE
`undef FEATURE

`ifdef FEATURE
module preprocessor (input wire a, output wire y);
  assign y = ~a;
`elsif OTHER
module preprocessor (input wire a, output wire y);
  assign y = a;
`else
module preprocessor (
  input  wire [`W-1:0] a,
  input  wire          s,
  output wire [`W-1:0] y, z
);
  assign y = `ADD(a, 8'd1);
`ifndef SYNTHESIS
  assign z = `MUX(s, a, `UNDEFINED_MACRO);
`else
  assign z = a;
`endif
`endif
endmodule
`default_nettype wire
