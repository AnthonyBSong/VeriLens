// Expression forms (TODO.md group 1)
module sv_expressions (
  input  logic        clk,
  input  logic [7:0]  a, b,
  input  logic [3:0]  sel,
  output logic        y,
  output logic [7:0]  z,
  output logic [1:0][3:0] pat,
  output logic [3:0]  cnt
);
  function automatic logic [7:0] inc(input logic [7:0] x);
    return x + 8'd1;
  endfunction : inc

  reg [7:0] mem [0:3];

  assign y = (a inside {8'd1, [8'd4:8'd7]}) | (a ==? 8'b1???_0000) | (a !=? 8'b0) | (a ** 2 == 4);
  assign z = inc(a) + 8'(b) + int'(a) + signed'(b) + $clog2(8) + a[3-:2] + b[0+:4] + a[sel+:2] + mem[1][2];
  assign pat = '{4'd1, 4'd2};

  always_comb begin
    unique casez (sel)
      4'b1???: cnt = 4'd1;
      4'b01?0: cnt = 4'd2;
      default: cnt = 4'd0;
    endcase
    priority if (a[0]) cnt = 4'd3;
    case (sel) inside
      [0:1]: cnt = 4'd4;
      2, [3:3]: cnt = 4'd5;
      default: ;
    endcase
  end

  always_ff @(posedge clk) begin
    mem[0] <= inc(b);
    mem[1][3:0] <= 4'd1;
    cnt += 1;
    cnt++;
    cnt <<= 1;
    {y, z} <= {1'b0, a};
    foreach (mem[i]) mem[i] <= 8'd0;
  end
endmodule
