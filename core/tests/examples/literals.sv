// Literal forms and constant folding in ranges.
module literals (
  input  wire              clk,
  inout  wire [7:0]        data,     // ANSI inout port
  input  wire [4*2-1:0]    a,        // folded: *
  input  wire [8/2-1:0]    b,        // folded: /
  input  wire [(1<<3)-1:0] c,        // folded: <<
  input  wire [(16>>1)-1:0] d,       // folded: >>
  output wire [7:0]        y
);
  parameter PI   = 3.14;             // real literals
  parameter BIG  = 1.2e+5;
  parameter TINY = 2E-3;
  parameter K    = 1e3;
  wire       ub = '1;                // unsized based literals
  wire       uz = 'x;
  wire [3:0] uh = 'hF;
  wire [3:0] m  = a[3:0] % 4'd3;     // modulo
  assign y    = {uh, m} ^ d ^ {4'd0, b} ^ {ub, uz, 6'd0};
  assign data = c;
endmodule
