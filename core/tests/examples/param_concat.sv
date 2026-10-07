// Parameter values that contain commas inside braces or brackets must stay in one piece.
module slave #(parameter [31:0] MASK = 32'hffff_0000, parameter N = 1) (input logic clk);
endmodule

// verilens: top
module param_concat (input logic clk);
  localparam logic [1:0][31:0] TABLE = {32'h1, 32'h2};
  slave #(.MASK({32'hf000_0000, 32'hffff_0000}), .N(2)) s0 (.clk(clk));
  slave #(.MASK(TABLE[0]), .N(3)) s1 (.clk(clk));
  and a0 (y, {x0, x1}[0], clk);
endmodule
