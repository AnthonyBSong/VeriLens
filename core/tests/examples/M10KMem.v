// Intel/Altera M10K synchronous dual-address RAM block.
// Used by ColumnWrapper to store the u_n and u_{n-1} wave state vectors.
module M10KMem #(
  parameter DEPTH = 256
)(
  input                         clk,
  input                         we,
  input  [$clog2(DEPTH)-1:0]    write_address,
  input  [$clog2(DEPTH)-1:0]    read_address,
  input  signed [17:0]          d,
  output reg signed [17:0]      q
);
  always @(posedge clk) begin
    if (we)
      q <= d;
  end
endmodule
