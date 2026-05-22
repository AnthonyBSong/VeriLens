// Handles load sign-extension and store byte-enable generation.
module LoadStoreUnit (
  input  logic [ 2:0] funct3,
  input  logic [31:0] addr,
  input  logic [31:0] store_data,
  output logic [ 3:0] byte_en,
  output logic [31:0] aligned_data
);
  always @(*) begin
    case (funct3)
      3'b000: begin byte_en = 4'b0001 << addr[1:0]; aligned_data = {4{store_data[7:0]}};  end // SB
      3'b001: begin byte_en = 4'b0011 << addr[1:0]; aligned_data = {2{store_data[15:0]}}; end // SH
      3'b010: begin byte_en = 4'b1111;               aligned_data = store_data;             end // SW
      default:begin byte_en = 4'b1111;               aligned_data = store_data;             end
    endcase
  end
endmodule
