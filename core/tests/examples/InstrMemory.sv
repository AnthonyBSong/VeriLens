// Synchronous instruction memory (read-only in normal operation).
module InstrMemory #(
  parameter DEPTH = 1024
)(
  input  logic        clk,
  input  logic [31:0] addr,
  output logic [31:0] instr
);
  logic [31:0] mem [0:DEPTH-1];
  always @(posedge clk)
    instr <= mem[addr[31:2]];
endmodule
