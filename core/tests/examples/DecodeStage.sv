// ID stage: register file read, immediate generation, and control signals.
module DecodeStage (
  input  logic        clk,
  input  logic        rst,
  input  logic [31:0] instr,
  input  logic [31:0] wb_wd,
  input  logic [ 4:0] wb_rd,
  input  logic        wb_we,
  output logic [31:0] rs1_val,
  output logic [31:0] rs2_val,
  output logic [31:0] imm,
  output logic [ 4:0] rs1,
  output logic [ 4:0] rs2,
  output logic [ 4:0] rd,
  output logic [ 6:0] opcode,
  output logic [ 2:0] funct3,
  output logic [ 6:0] funct7,
  output logic        reg_write,
  output logic        mem_read,
  output logic        mem_write,
  output logic        mem_to_reg,
  output logic        alu_src,
  output logic        branch,
  output logic        jump,
  output logic [ 3:0] alu_op,
  output logic [ 2:0] imm_sel
);
  assign rs1    = instr[19:15];
  assign rs2    = instr[24:20];
  assign rd     = instr[11:7];
  assign opcode = instr[6:0];
  assign funct3 = instr[14:12];
  assign funct7 = instr[31:25];

  RegFile rf (
    .clk (clk),
    .we  (wb_we),
    .rs1 (rs1),
    .rs2 (rs2),
    .rd  (wb_rd),
    .wd  (wb_wd),
    .rd1 (rs1_val),
    .rd2 (rs2_val)
  );

  ImmExtend ie (
    .instr (instr),
    .sel   (imm_sel),
    .imm   (imm)
  );

  ControlUnit cu (
    .opcode     (opcode),
    .funct3     (funct3),
    .funct7     (funct7),
    .reg_write  (reg_write),
    .mem_read   (mem_read),
    .mem_write  (mem_write),
    .mem_to_reg (mem_to_reg),
    .alu_src    (alu_src),
    .branch     (branch),
    .jump       (jump),
    .alu_op     (alu_op),
    .imm_sel    (imm_sel)
  );
endmodule
