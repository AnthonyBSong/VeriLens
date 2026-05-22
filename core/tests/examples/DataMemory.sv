// Byte-addressable data memory with byte/half/word access.
module DataMemory #(
  parameter DEPTH = 1024
)(
  input  logic        clk,
  input  logic        we,
  input  logic        re,
  input  logic [ 2:0] funct3,
  input  logic [31:0] addr,
  input  logic [31:0] wd,
  output logic [31:0] rd
);
  logic [31:0] mem [0:DEPTH-1];
  logic [31:0] raw;
  assign raw = mem[addr[31:2]];

  always @(*) begin
    case (funct3)
      3'b000: rd = {{24{raw[7]}},  raw[7:0]};
      3'b001: rd = {{16{raw[15]}}, raw[15:0]};
      3'b010: rd = raw;
      3'b100: rd = {24'b0, raw[7:0]};
      3'b101: rd = {16'b0, raw[15:0]};
      default: rd = raw;
    endcase
  end

  always @(posedge clk) begin
    if (we) begin
      case (funct3)
        3'b000: mem[addr[31:2]][7:0]   <= wd[7:0];
        3'b001: mem[addr[31:2]][15:0]  <= wd[15:0];
        3'b010: mem[addr[31:2]]        <= wd;
        default: mem[addr[31:2]]       <= wd;
      endcase
    end
  end
endmodule
