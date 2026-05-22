// Evaluates branch conditions and computes the target PC.
module BranchUnit (
  input  logic [31:0] pc,
  input  logic [31:0] rs1,
  input  logic [31:0] rs2,
  input  logic [31:0] imm,
  input  logic [ 2:0] funct3,
  input  logic        branch,
  output logic [31:0] target,
  output logic        taken
);
  assign target = pc + imm;
  always @(*) begin
    taken = 1'b0;
    if (branch) begin
      case (funct3)
        3'b000: taken = (rs1 == rs2);
        3'b001: taken = (rs1 != rs2);
        3'b100: taken = ($signed(rs1) <  $signed(rs2));
        3'b101: taken = ($signed(rs1) >= $signed(rs2));
        3'b110: taken = (rs1 < rs2);
        3'b111: taken = (rs1 >= rs2);
        default: taken = 1'b0;
      endcase
    end
  end
endmodule
