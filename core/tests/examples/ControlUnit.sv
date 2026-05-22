// Main RISC-V RV32I control unit — decodes opcode/funct into control signals.
module ControlUnit (
  input  logic [ 6:0] opcode,
  input  logic [ 2:0] funct3,
  input  logic [ 6:0] funct7,
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
  always @(*) begin
    reg_write  = 1'b0; mem_read  = 1'b0; mem_write = 1'b0;
    mem_to_reg = 1'b0; alu_src   = 1'b0; branch    = 1'b0;
    jump       = 1'b0; alu_op    = 4'b0; imm_sel   = 3'b0;
    case (opcode)
      7'b0110011: begin reg_write = 1; alu_src = 0; mem_to_reg = 0; end // R-type
      7'b0010011: begin reg_write = 1; alu_src = 1; imm_sel = 3'b000; end // I-type ALU
      7'b0000011: begin reg_write = 1; mem_read = 1; alu_src = 1; mem_to_reg = 1; imm_sel = 3'b000; end // load
      7'b0100011: begin mem_write = 1; alu_src = 1; imm_sel = 3'b001; end // store
      7'b1100011: begin branch = 1; imm_sel = 3'b010; end // branch
      7'b1101111: begin jump = 1; reg_write = 1; imm_sel = 3'b100; end // JAL
      7'b0110111: begin reg_write = 1; alu_src = 1; imm_sel = 3'b011; end // LUI
      default: ;
    endcase
  end
endmodule
