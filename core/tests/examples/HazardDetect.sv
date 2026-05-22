// Load-use hazard detection: stalls the pipeline for one cycle.
module HazardDetect (
  input  logic       id_ex_mem_read,
  input  logic [4:0] id_ex_rd,
  input  logic [4:0] if_id_rs1,
  input  logic [4:0] if_id_rs2,
  output logic       stall
);
  assign stall = id_ex_mem_read &&
                 ((id_ex_rd == if_id_rs1) || (id_ex_rd == if_id_rs2));
endmodule
