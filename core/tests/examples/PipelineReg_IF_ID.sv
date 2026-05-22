// IF/ID pipeline register with stall and flush.
module PipelineReg_IF_ID (
  input  logic        clk,
  input  logic        rst,
  input  logic        stall,
  input  logic        flush,
  input  logic [31:0] if_pc,
  input  logic [31:0] if_instr,
  output logic [31:0] id_pc,
  output logic [31:0] id_instr
);
  always @(posedge clk) begin
    if (rst || flush) begin
      id_pc    <= 32'b0;
      id_instr <= 32'h00000013; // NOP (addi x0, x0, 0)
    end else if (!stall) begin
      id_pc    <= if_pc;
      id_instr <= if_instr;
    end
  end
endmodule
