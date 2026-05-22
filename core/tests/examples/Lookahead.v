//========================================================================
// Priority Encoder + Encoded Shifter Implementation (zero lookahead)
//========================================================================

`ifndef LAB1_IMUL_LOOKAHEAD_V
`define LAB1_IMUL_LOOKAHEAD_V

`include "vc/trace.v"

// priority encoder

module lab1_imul_PriorityEncoder (
  input  logic [31:0] in_,
  output logic [31:0] out
);

  logic [32:0] seen;
  assign seen[0] = 1'b0;

  genvar i;
  generate
    for (i = 0; i < 32; i++) begin : gen
      assign out[i]  = in_[i] & ~seen[i];     // fire only on first '1'
      assign seen[i+1] = seen[i] | in_[i];    // track if any '1' so far
    end
  endgenerate

endmodule

module lab1_imul_EncodedLeftShifter
(
  input  logic [31:0] in_,
  input  logic [31:0] shamt,
  output logic [31:0] out
);

  logic [31:0] shifted [32];
  logic [31:0] and_out [32];
  logic [31:0] or_out  [33];

  genvar i;
  generate
    for (i = 0; i < 32; i = i + 1) begin : gen
      assign shifted[i]  = in_ << (i + 1);
      assign and_out[i]  = shifted[i] & {32{shamt[i]}};
      assign or_out[i+1] = or_out[i] | and_out[i];
    end
  endgenerate

  assign out = or_out[32];

endmodule

module lab1_imul_EncodedRightShifter
(
  input  logic [31:0] in_,
  input  logic [31:0] shamt,
  output logic [31:0] out
);

  logic [31:0] shifted [32];
  logic [31:0] and_out [32];
  logic [31:0] or_out  [33];

  genvar i;
  generate
    for (i = 0; i < 32; i = i + 1) begin : gen
      assign shifted[i]  = in_ >> (i + 1);
      assign and_out[i]  = shifted[i] & {32{shamt[i]}};
      assign or_out[i+1] = or_out[i] | and_out[i];
    end
  endgenerate

  assign out = or_out[32];

endmodule

`endif