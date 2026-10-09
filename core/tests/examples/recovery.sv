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

module bad_header #(parameter) (input logic a, output logic y);   // header error: the module survives
  )                                 // stray token at module level
  assign y = ;                      // expected an expression
  defparam nothere.W = 4;           // instance does not exist
  defparam u.v.W = 4;               // hierarchical: not applied
  default disable iff (a);
  always_comb begin
    if (a) y = 1'b1; else else y = 1'b0;      // 'else else': one statement dropped
    case (a) 1'b1: y = ; default: y = 1'b0; endcase   // one case item dropped
  end
endmodule
