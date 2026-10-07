// Generate forms, instance arrays, defparam, nested module, assign delays (TODO.md groups 1-2)
module sv_gen_leaf #(parameter W = 1, D = 2) (input logic clk, input logic i, output logic o);
  assign o = i;
endmodule

module sv_generate #(parameter P = 1) (
  input  logic       clk,
  input  logic [3:0] a,
  output logic [3:0] y, z, g,
  output wire        d1, d2, d3
);
  module sv_inner (input logic i, output logic o);
    assign o = ~i;
  endmodule

  sv_gen_leaf u_arr [3:0] (.clk, .i(a), .o(y));
  buf b_arr [3:0] (g, a);
  sv_inner u_in (.i(a[0]), .o(z[0]));
  sv_gen_leaf #(.D(5)) u_dp (.clk(clk), .i(a[1]), .o(z[1]));
  defparam u_dp.W = 4;
  defparam u_dp.D = 6;

  for (genvar k = 2; k < 4; k++) begin : g_for
    sv_gen_leaf u_k (.clk, .i(a[k]), .o(z[k]));
  end
  if (P == 1) begin : g_if
    assign d1 = a[0];
  end else if (P == 2) begin
    assign d1 = a[1];
  end else begin : g_else
    assign d1 = a[2];
  end
  generate
    case (P)
      1: begin : g_case assign d2 = a[0]; end
      2, 3: assign d2 = a[1];
      default: begin assign d2 = 1'b0; end
    endcase
  endgenerate
  genvar i, j;
  generate
    for (i = 0; i < 2; i = i + 1) begin : g_outer
      for (j = 0; j < 2; j = j + 1) begin : g_inner
        assign d3 = a[i];
      end
    end
  endgenerate
  assign (strong1, weak0) #1 d3 = a[3];
  wire #2 w_delay = a[0];
endmodule
