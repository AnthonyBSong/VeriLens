// Top-level 5-stage pipelined RISC-V RV32I CPU.
// Instantiates all pipeline stages plus hazard detection and forwarding.
module PipelinedCPU (
  input  logic clk,
  input  logic rst
);
  // IF stage outputs
  logic [31:0] if_pc, if_instr;
  // IF/ID register outputs
  logic [31:0] id_pc, id_instr;
  // ID stage outputs
  logic [31:0] id_rs1_val, id_rs2_val, id_imm;
  logic [ 4:0] id_rs1, id_rs2, id_rd;
  logic [ 6:0] id_opcode, id_funct7;
  logic [ 2:0] id_funct3;
  logic        id_reg_write, id_mem_read, id_mem_write;
  logic        id_mem_to_reg, id_alu_src, id_branch, id_jump;
  logic [ 3:0] id_alu_op;
  logic [ 2:0] id_imm_sel;
  // ID/EX register outputs
  logic [31:0] ex_pc, ex_rs1_val, ex_rs2_val, ex_imm;
  logic [ 4:0] ex_rs1, ex_rs2, ex_rd;
  logic [ 3:0] ex_alu_op;
  logic        ex_alu_src, ex_mem_read, ex_mem_write;
  logic        ex_reg_write, ex_mem_to_reg, ex_branch, ex_jump;
  // EX stage outputs
  logic [31:0] ex_alu_result, ex_rs2_out;
  logic        ex_zero, ex_branch_taken;
  logic [31:0] ex_branch_target;
  // EX/MEM register outputs
  logic [31:0] mem_alu_result, mem_rs2_val;
  logic [ 4:0] mem_rd;
  logic        mem_mem_read, mem_mem_write, mem_reg_write, mem_mem_to_reg, mem_zero;
  // MEM stage outputs
  logic [31:0] mem_read_data;
  // MEM/WB register outputs
  logic [31:0] wb_alu_result, wb_read_data;
  logic [ 4:0] wb_rd;
  logic        wb_reg_write, wb_mem_to_reg;
  // WB stage outputs
  logic [31:0] wb_data;
  // Hazard and forwarding
  logic        stall;
  logic [ 1:0] fwd_a, fwd_b;

  HazardDetect hazard (
    .id_ex_mem_read (ex_mem_read),
    .id_ex_rd       (ex_rd),
    .if_id_rs1      (id_rs1),
    .if_id_rs2      (id_rs2),
    .stall          (stall)
  );

  ForwardingUnit fwd (
    .ex_mem_rd        (mem_rd),
    .mem_wb_rd        (wb_rd),
    .ex_mem_reg_write (mem_reg_write),
    .mem_wb_reg_write (wb_reg_write),
    .id_ex_rs1        (ex_rs1),
    .id_ex_rs2        (ex_rs2),
    .fwd_a            (fwd_a),
    .fwd_b            (fwd_b)
  );

  FetchStage fetch (
    .clk           (clk),
    .rst           (rst),
    .stall         (stall),
    .branch_taken  (ex_branch_taken),
    .branch_target (ex_branch_target),
    .pc            (if_pc),
    .instr         (if_instr)
  );

  PipelineReg_IF_ID if_id (
    .clk     (clk), .rst (rst),
    .stall   (stall), .flush (ex_branch_taken),
    .if_pc   (if_pc), .if_instr (if_instr),
    .id_pc   (id_pc), .id_instr (id_instr)
  );

  DecodeStage decode (
    .clk (clk), .rst (rst),
    .instr (id_instr),
    .wb_wd (wb_data), .wb_rd (wb_rd), .wb_we (wb_reg_write),
    .rs1_val (id_rs1_val), .rs2_val (id_rs2_val), .imm (id_imm),
    .rs1 (id_rs1), .rs2 (id_rs2), .rd (id_rd),
    .opcode (id_opcode), .funct3 (id_funct3), .funct7 (id_funct7),
    .reg_write (id_reg_write), .mem_read (id_mem_read), .mem_write (id_mem_write),
    .mem_to_reg (id_mem_to_reg), .alu_src (id_alu_src),
    .branch (id_branch), .jump (id_jump), .alu_op (id_alu_op), .imm_sel (id_imm_sel)
  );

  PipelineReg_ID_EX id_ex (
    .clk (clk), .rst (rst), .flush (stall),
    .id_pc (id_pc), .id_rs1_val (id_rs1_val), .id_rs2_val (id_rs2_val),
    .id_imm (id_imm), .id_rs1 (id_rs1), .id_rs2 (id_rs2), .id_rd (id_rd),
    .id_alu_op (id_alu_op), .id_alu_src (id_alu_src),
    .id_mem_read (id_mem_read), .id_mem_write (id_mem_write),
    .id_reg_write (id_reg_write), .id_mem_to_reg (id_mem_to_reg),
    .id_branch (id_branch), .id_jump (id_jump),
    .ex_pc (ex_pc), .ex_rs1_val (ex_rs1_val), .ex_rs2_val (ex_rs2_val),
    .ex_imm (ex_imm), .ex_rs1 (ex_rs1), .ex_rs2 (ex_rs2), .ex_rd (ex_rd),
    .ex_alu_op (ex_alu_op), .ex_alu_src (ex_alu_src),
    .ex_mem_read (ex_mem_read), .ex_mem_write (ex_mem_write),
    .ex_reg_write (ex_reg_write), .ex_mem_to_reg (ex_mem_to_reg),
    .ex_branch (ex_branch), .ex_jump (ex_jump)
  );

  ExecuteStage execute (
    .pc (ex_pc), .rs1_val (ex_rs1_val), .rs2_val (ex_rs2_val),
    .imm (ex_imm), .rs1 (ex_rs1), .rs2 (ex_rs2),
    .alu_op (ex_alu_op), .alu_src (ex_alu_src),
    .branch (ex_branch), .jump (ex_jump),
    .fwd_mem_val (mem_alu_result), .fwd_wb_val (wb_data),
    .fwd_a (fwd_a), .fwd_b (fwd_b), .funct3 (ex_imm[2:0]),
    .alu_result (ex_alu_result), .rs2_out (ex_rs2_out),
    .zero (ex_zero), .branch_taken (ex_branch_taken),
    .branch_target (ex_branch_target)
  );

  PipelineReg_EX_MEM ex_mem (
    .clk (clk), .rst (rst),
    .ex_alu_result (ex_alu_result), .ex_rs2_val (ex_rs2_out),
    .ex_rd (ex_rd), .ex_mem_read (ex_mem_read), .ex_mem_write (ex_mem_write),
    .ex_reg_write (ex_reg_write), .ex_mem_to_reg (ex_mem_to_reg), .ex_zero (ex_zero),
    .mem_alu_result (mem_alu_result), .mem_rs2_val (mem_rs2_val),
    .mem_rd (mem_rd), .mem_mem_read (mem_mem_read), .mem_mem_write (mem_mem_write),
    .mem_reg_write (mem_reg_write), .mem_mem_to_reg (mem_mem_to_reg), .mem_zero (mem_zero)
  );

  MemoryStage memory (
    .clk (clk),
    .alu_result (mem_alu_result), .rs2_val (mem_rs2_val),
    .mem_read (mem_mem_read), .mem_write (mem_mem_write),
    .funct3 (3'b010), .read_data (mem_read_data)
  );

  PipelineReg_MEM_WB mem_wb (
    .clk (clk), .rst (rst),
    .mem_alu_result (mem_alu_result), .mem_read_data (mem_read_data),
    .mem_rd (mem_rd), .mem_reg_write (mem_reg_write), .mem_mem_to_reg (mem_mem_to_reg),
    .wb_alu_result (wb_alu_result), .wb_read_data (wb_read_data),
    .wb_rd (wb_rd), .wb_reg_write (wb_reg_write), .wb_mem_to_reg (wb_mem_to_reg)
  );

  WritebackStage writeback (
    .alu_result (wb_alu_result),
    .read_data  (wb_read_data),
    .mem_to_reg (wb_mem_to_reg),
    .wb_data    (wb_data)
  );
endmodule
