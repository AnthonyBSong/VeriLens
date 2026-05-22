// ID/EX pipeline register.
module PipelineReg_ID_EX (
  input  logic        clk,
  input  logic        rst,
  input  logic        flush,
  input  logic [31:0] id_pc,
  input  logic [31:0] id_rs1_val,
  input  logic [31:0] id_rs2_val,
  input  logic [31:0] id_imm,
  input  logic [ 4:0] id_rs1,
  input  logic [ 4:0] id_rs2,
  input  logic [ 4:0] id_rd,
  input  logic [ 3:0] id_alu_op,
  input  logic        id_alu_src,
  input  logic        id_mem_read,
  input  logic        id_mem_write,
  input  logic        id_reg_write,
  input  logic        id_mem_to_reg,
  input  logic        id_branch,
  input  logic        id_jump,
  output logic [31:0] ex_pc,
  output logic [31:0] ex_rs1_val,
  output logic [31:0] ex_rs2_val,
  output logic [31:0] ex_imm,
  output logic [ 4:0] ex_rs1,
  output logic [ 4:0] ex_rs2,
  output logic [ 4:0] ex_rd,
  output logic [ 3:0] ex_alu_op,
  output logic        ex_alu_src,
  output logic        ex_mem_read,
  output logic        ex_mem_write,
  output logic        ex_reg_write,
  output logic        ex_mem_to_reg,
  output logic        ex_branch,
  output logic        ex_jump
);
  always @(posedge clk) begin
    if (rst || flush) begin
      ex_pc <= 0; ex_rs1_val <= 0; ex_rs2_val <= 0; ex_imm <= 0;
      ex_rs1 <= 0; ex_rs2 <= 0; ex_rd <= 0; ex_alu_op <= 0;
      ex_alu_src <= 0; ex_mem_read <= 0; ex_mem_write <= 0;
      ex_reg_write <= 0; ex_mem_to_reg <= 0; ex_branch <= 0; ex_jump <= 0;
    end else begin
      ex_pc <= id_pc; ex_rs1_val <= id_rs1_val; ex_rs2_val <= id_rs2_val;
      ex_imm <= id_imm; ex_rs1 <= id_rs1; ex_rs2 <= id_rs2; ex_rd <= id_rd;
      ex_alu_op <= id_alu_op; ex_alu_src <= id_alu_src;
      ex_mem_read <= id_mem_read; ex_mem_write <= id_mem_write;
      ex_reg_write <= id_reg_write; ex_mem_to_reg <= id_mem_to_reg;
      ex_branch <= id_branch; ex_jump <= id_jump;
    end
  end
endmodule
