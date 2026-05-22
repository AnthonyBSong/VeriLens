// IF stage: PC register and instruction memory interface.
module FetchStage (
  input  logic        clk,
  input  logic        rst,
  input  logic        stall,
  input  logic        branch_taken,
  input  logic [31:0] branch_target,
  output logic [31:0] pc,
  output logic [31:0] instr
);
  logic [31:0] pc_reg;
  assign pc = pc_reg;

  InstrMemory #(.DEPTH(1024)) imem (
    .clk   (clk),
    .addr  (pc_reg),
    .instr (instr)
  );

  always @(posedge clk) begin
    if (rst)
      pc_reg <= 32'b0;
    else if (branch_taken)
      pc_reg <= branch_target;
    else if (!stall)
      pc_reg <= pc_reg + 32'd4;
  end
endmodule
