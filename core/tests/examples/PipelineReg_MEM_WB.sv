// MEM/WB pipeline register.
module PipelineReg_MEM_WB (
  input  logic        clk,
  input  logic        rst,
  input  logic [31:0] mem_alu_result,
  input  logic [31:0] mem_read_data,
  input  logic [ 4:0] mem_rd,
  input  logic        mem_reg_write,
  input  logic        mem_mem_to_reg,
  output logic [31:0] wb_alu_result,
  output logic [31:0] wb_read_data,
  output logic [ 4:0] wb_rd,
  output logic        wb_reg_write,
  output logic        wb_mem_to_reg
);
  always @(posedge clk) begin
    if (rst) begin
      wb_alu_result <= 0; wb_read_data <= 0; wb_rd <= 0;
      wb_reg_write <= 0; wb_mem_to_reg <= 0;
    end else begin
      wb_alu_result <= mem_alu_result; wb_read_data <= mem_read_data;
      wb_rd <= mem_rd; wb_reg_write <= mem_reg_write;
      wb_mem_to_reg <= mem_mem_to_reg;
    end
  end
endmodule
