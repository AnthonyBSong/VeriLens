module traffic_light(clk, rst, state);
  input clk;
  input rst;
  output reg [1:0] state;

  always @(posedge clk) begin
    if (rst)
      state <= 2'b00;
    else
      case (state)
        2'b00: state <= 2'b01;
        2'b01: state <= 2'b10;
        2'b10: state <= 2'b00;
        default: state <= 2'b00;
      endcase
  end
endmodule
