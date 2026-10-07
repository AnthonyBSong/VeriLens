// Error recovery: unsupported syntax drops one statement, not the file (TODO.md group 0)
module recovery (input logic clk, a, b, output logic y, z, w, v);
  assign y = a;
  assign z = a ### b;          // not Verilog: dropped with an error note
  assign w = ~a;
  always_ff @(posedge clk) begin
    v <= a;
    v <= (a;                   // dropped; the rest of the block survives
    v <= b;
  end
  always_comb begin
    unique casez ({a, b})      // used to hang the parser
      2'b1?: y = 1'b1;
      default: y = 1'b0;
    endcase
  end
endmodule
