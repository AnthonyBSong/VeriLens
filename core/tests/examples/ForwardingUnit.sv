// MEM and WB forwarding for EX-stage operands.
module ForwardingUnit (
  input  logic [4:0] ex_mem_rd,
  input  logic [4:0] mem_wb_rd,
  input  logic       ex_mem_reg_write,
  input  logic       mem_wb_reg_write,
  input  logic [4:0] id_ex_rs1,
  input  logic [4:0] id_ex_rs2,
  output logic [1:0] fwd_a,
  output logic [1:0] fwd_b
);
  always @(*) begin
    fwd_a = 2'b00;
    fwd_b = 2'b00;
    if (ex_mem_reg_write && ex_mem_rd != 5'b0 && ex_mem_rd == id_ex_rs1) fwd_a = 2'b10;
    else if (mem_wb_reg_write && mem_wb_rd != 5'b0 && mem_wb_rd == id_ex_rs1) fwd_a = 2'b01;
    if (ex_mem_reg_write && ex_mem_rd != 5'b0 && ex_mem_rd == id_ex_rs2) fwd_b = 2'b10;
    else if (mem_wb_reg_write && mem_wb_rd != 5'b0 && mem_wb_rd == id_ex_rs2) fwd_b = 2'b01;
  end
endmodule
