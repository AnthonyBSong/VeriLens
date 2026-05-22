// EX stage: ALU, forwarding muxes, and branch evaluation.
module ExecuteStage (
  input  logic [31:0] pc,
  input  logic [31:0] rs1_val,
  input  logic [31:0] rs2_val,
  input  logic [31:0] imm,
  input  logic [ 4:0] rs1,
  input  logic [ 4:0] rs2,
  input  logic [ 3:0] alu_op,
  input  logic        alu_src,
  input  logic        branch,
  input  logic        jump,
  input  logic [31:0] fwd_mem_val,
  input  logic [31:0] fwd_wb_val,
  input  logic [ 1:0] fwd_a,
  input  logic [ 1:0] fwd_b,
  input  logic [ 2:0] funct3,
  output logic [31:0] alu_result,
  output logic [31:0] rs2_out,
  output logic        zero,
  output logic        branch_taken,
  output logic [31:0] branch_target
);
  logic [31:0] op_a, op_b_pre, op_b;

  always @(*) begin
    case (fwd_a)
      2'b10:   op_a = fwd_mem_val;
      2'b01:   op_a = fwd_wb_val;
      default: op_a = rs1_val;
    endcase
    case (fwd_b)
      2'b10:   op_b_pre = fwd_mem_val;
      2'b01:   op_b_pre = fwd_wb_val;
      default: op_b_pre = rs2_val;
    endcase
    op_b = alu_src ? imm : op_b_pre;
  end

  assign rs2_out = op_b_pre;

  Alu alu (
    .a      (op_a),
    .b      (op_b),
    .op     (alu_op),
    .result (alu_result),
    .zero   (zero)
  );

  BranchUnit bu (
    .pc     (pc),
    .rs1    (op_a),
    .rs2    (op_b_pre),
    .imm    (imm),
    .funct3 (funct3),
    .branch (branch),
    .target (branch_target),
    .taken  (branch_taken)
  );
endmodule
