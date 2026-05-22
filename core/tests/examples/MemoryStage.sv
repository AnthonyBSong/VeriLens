// MEM stage: data memory access via LoadStoreUnit.
module MemoryStage (
  input  logic        clk,
  input  logic [31:0] alu_result,
  input  logic [31:0] rs2_val,
  input  logic        mem_read,
  input  logic        mem_write,
  input  logic [ 2:0] funct3,
  output logic [31:0] read_data
);
  logic [ 3:0] byte_en;
  logic [31:0] aligned_data;

  LoadStoreUnit lsu (
    .funct3      (funct3),
    .addr        (alu_result),
    .store_data  (rs2_val),
    .byte_en     (byte_en),
    .aligned_data(aligned_data)
  );

  DataMemory #(.DEPTH(1024)) dmem (
    .clk    (clk),
    .we     (mem_write),
    .re     (mem_read),
    .funct3 (funct3),
    .addr   (alu_result),
    .wd     (aligned_data),
    .rd     (read_data)
  );
endmodule
