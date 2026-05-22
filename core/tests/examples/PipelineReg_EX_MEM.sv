// EX/MEM pipeline register.
module PipelineReg_EX_MEM (
  input  logic        clk,
  input  logic        rst,
  input  logic [31:0] ex_alu_result,
  input  logic [31:0] ex_rs2_val,
  input  logic [ 4:0] ex_rd,
  input  logic        ex_mem_read,
  input  logic        ex_mem_write,
  input  logic        ex_reg_write,
  input  logic        ex_mem_to_reg,
  input  logic        ex_zero,
  output logic [31:0] mem_alu_result,
  output logic [31:0] mem_rs2_val,
  output logic [ 4:0] mem_rd,
  output logic        mem_mem_read,
  output logic        mem_mem_write,
  output logic        mem_reg_write,
  output logic        mem_mem_to_reg,
  output logic        mem_zero
);
  always @(posedge clk) begin
    if (rst) begin
      mem_alu_result <= 0; mem_rs2_val <= 0; mem_rd <= 0;
      mem_mem_read <= 0; mem_mem_write <= 0; mem_reg_write <= 0;
      mem_mem_to_reg <= 0; mem_zero <= 0;
    end else begin
      mem_alu_result <= ex_alu_result; mem_rs2_val <= ex_rs2_val;
      mem_rd <= ex_rd; mem_mem_read <= ex_mem_read;
      mem_mem_write <= ex_mem_write; mem_reg_write <= ex_reg_write;
      mem_mem_to_reg <= ex_mem_to_reg; mem_zero <= ex_zero;
    end
  end
endmodule
