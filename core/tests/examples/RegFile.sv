// 32-entry x 32-bit register file with two async read ports and one sync write port.
// Register x0 is hardwired to zero.
module RegFile (
  input  logic        clk,
  input  logic        we,
  input  logic [ 4:0] rs1,
  input  logic [ 4:0] rs2,
  input  logic [ 4:0] rd,
  input  logic [31:0] wd,
  output logic [31:0] rd1,
  output logic [31:0] rd2
);
  // Array declarations (reg [W] name [D]) are not yet parsed;
  // behavioral body omitted — port interface is what matters for structural analysis.
  always @(posedge clk) begin
    if (we && rd != 5'b0)
      rd1 <= wd;
  end
  assign rd2 = rd1;
endmodule
