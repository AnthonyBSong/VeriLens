//=========================================================================
// Cache Submodules
//=========================================================================

`ifndef LAB3_MEM_CACHE_SUBMODULES_V
`define LAB3_MEM_CACHE_SUBMODULES_V

module lab3_mem_Wben_Dec
(
  input  logic [31:0] addr,
  output logic [15:0] wb_en
);
  always_comb begin
    // case offset
    case (addr[3:2])
      2'b00: wb_en = 16'h000F;
      2'b01: wb_en = 16'h00F0;
      2'b10: wb_en = 16'h0F00;
      2'b11: wb_en = 16'hF000;
      default: wb_en = 'x;
    endcase
  end
endmodule

module lab3_mem_Mkaddr
#(
  parameter p_num_banks = 1,
  parameter index_width = 4,

  localparam c_bank_bits = $clog2(p_num_banks)
)
(
  input  [27-index_width-c_bank_bits:0] tag,
  input  [index_width-1:0]  index,
  input  [31:0]             cachereq_addr,
  output [31:0]             memreq_addr
);
  generate
    if ( c_bank_bits > 0 ) begin
      logic [c_bank_bits-1:0] bank;
      assign bank = cachereq_addr[3+c_bank_bits:4];
      assign memreq_addr = {tag, index, bank, 4'b0};
    end else
      assign memreq_addr = {tag, index, 4'b0};
  endgenerate
endmodule

`endif