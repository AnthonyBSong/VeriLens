// 32-bit non-restoring integer divider.
module Divider (
  input  logic        clk,
  input  logic        rst,
  input  logic        start,
  input  logic [31:0] dividend,
  input  logic [31:0] divisor,
  output logic [31:0] quotient,
  output logic [31:0] remainder,
  output logic        done,
  output logic        div_by_zero
);
  logic [63:0] partial;
  logic [31:0] q_shift;
  logic [ 5:0] count;

  assign div_by_zero = (divisor == 32'b0);

  always @(posedge clk) begin
    if (rst || start) begin
      partial <= {32'b0, dividend};
      q_shift <= 32'b0;
      count   <= 6'd0;
      done    <= 1'b0;
    end else if (!div_by_zero && count < 32) begin
      if (partial[63:32] >= divisor) begin
        partial <= {(partial[63:32] - divisor), partial[31:0], 1'b1};
        q_shift <= {q_shift[30:0], 1'b1};
      end else begin
        partial <= {partial[62:0], 1'b0};
        q_shift <= {q_shift[30:0], 1'b0};
      end
      count <= count + 1;
    end else if (count == 32) begin
      quotient  <= q_shift;
      remainder <= partial[63:32];
      done      <= 1'b1;
    end
  end
endmodule
