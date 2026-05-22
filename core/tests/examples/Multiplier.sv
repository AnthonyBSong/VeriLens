// 32x32 sequential multiplier using shift-and-add (33 cycles).
module Multiplier (
  input  logic        clk,
  input  logic        rst,
  input  logic        start,
  input  logic [31:0] a,
  input  logic [31:0] b,
  output logic [63:0] product,
  output logic        done
);
  logic [63:0] acc;
  logic [31:0] shifter;
  logic [ 5:0] count;

  always @(posedge clk) begin
    if (rst || start) begin
      acc     <= 64'b0;
      shifter <= a;
      count   <= 6'd0;
      done    <= 1'b0;
      product <= 64'b0;
    end else if (count < 32) begin
      if (shifter[0]) acc <= acc + ({32'b0, b} << count);
      shifter <= shifter >> 1;
      count   <= count + 1;
    end else begin
      product <= acc;
      done    <= 1'b1;
    end
  end
endmodule
