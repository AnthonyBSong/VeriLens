// WB stage: mux between ALU result and memory data, drives register file write.
module WritebackStage (
  input  logic [31:0] alu_result,
  input  logic [31:0] read_data,
  input  logic        mem_to_reg,
  output logic [31:0] wb_data
);
  assign wb_data = mem_to_reg ? read_data : alu_result;
endmodule
