//=========================================================================
// Base Blocking Cache Datapath
//=========================================================================

`ifndef LAB3_MEM_CACHE_BASE_DPATH_V
`define LAB3_MEM_CACHE_BASE_DPATH_V

`include "vc/arithmetic.v"
`include "vc/muxes.v"
`include "vc/mem-msgs.v"
`include "vc/srams.v"
`include "vc/regs.v"
`include "lab3_mem/CacheSubmodules.v"


module lab3_mem_CacheBaseDpath
#(
  parameter p_num_banks = 1
)
(
  input  logic          clk,
  input  logic          reset,

  // Processor <-> Cache Interface

  input  mem_req_4B_t   proc2cache_reqstream_msg,
  output mem_resp_4B_t  proc2cache_respstream_msg,

  // Cache <-> Memory Interface

  output mem_req_16B_t  cache2mem_reqstream_msg,
  input  mem_resp_16B_t cache2mem_respstream_msg,

  // Datapath -> Control Unit
  output logic [3:0]    cachereq_type,
  output logic [3:0]    index,
  output logic          tag_match,

  // Control Unit -> Datapath
  input  logic          cachereq_en,
  input  logic          memresp_en,
  input  logic          write_data_mux_sel,
  input  logic          tag_array_ren,
  input  logic          tag_array_wen,
  input  logic          data_array_ren,
  input  logic          data_array_wen,
  input  logic          refill,
  input  logic          read_data_zero_mux_sel,
  input  logic          read_data_reg_en,
  input  logic          evict_addr_reg_en,
  input  logic          memreq_addr_mux_sel,
  input  logic [3:0]    cacheresp_type,
  input  logic [3:0]    memreq_type,
  input  logic          hit
);

  localparam c_bank_bits = $clog2(p_num_banks);

  // register cache request message

  logic [7:0] cachereq_opaque;
  
  vc_EnReg #(8) cachereq_opaque_reg
  (
    .clk   (clk),
    .reset (reset),
    .d     (proc2cache_reqstream_msg.opaque),
    .q     (cachereq_opaque),
    .en    (cachereq_en)
  );

  vc_EnReg #(4) cachereq_type_reg
  (
    .clk   (clk),
    .reset (reset),
    .d     (proc2cache_reqstream_msg.type_),
    .q     (cachereq_type),
    .en    (cachereq_en)
  );

  logic [31:0] cachereq_addr;

  vc_EnReg #(32) cachereq_addr_reg
  (
    .clk   (clk),
    .reset (reset),
    .d     (proc2cache_reqstream_msg.addr),
    .q     (cachereq_addr),
    .en    (cachereq_en)
  );

  logic [31:0] cachereq_data;

  vc_EnReg #(32) cachereq_data_reg
  (
    .clk   (clk),
    .reset (reset),
    .d     (proc2cache_reqstream_msg.data),
    .q     (cachereq_data),
    .en    (cachereq_en)
  );

  // register memory response
  
  logic [127:0] memresp_data;

  vc_EnReg #(128) memresp_data_reg
  (
    .clk   (clk),
    .reset (reset),
    .d     (cache2mem_respstream_msg.data),
    .q     (memresp_data),
    .en    (memresp_en)
  );

  // write data mux

  logic [127:0] data_array_write_data;

  vc_Mux2 #(128) write_data_mux
  (
    .in0 ({4{cachereq_data}}), // replicated data
    .in1 (memresp_data),
    .sel (write_data_mux_sel),
    .out (data_array_write_data)
  );

  // writeback enable decoder

  logic [15:0] cache_write_wb_en;
  lab3_mem_Wben_Dec wben_dec
  (
    .addr  (cachereq_addr),
    .wb_en (cache_write_wb_en)
  );

  // writeback enable mux

  logic [15:0] data_array_wb_en;
  vc_Mux2 #(16) wben_mux
  (
    .in0 (cache_write_wb_en),
    .in1 (16'hffff),
    .sel (refill),
    .out (data_array_wb_en)
  );

  // cache address breakdown
  
  logic [1:0]  offset;
  logic [23-c_bank_bits:0] tag;

  assign offset = cachereq_addr[3:2];
  assign index  = cachereq_addr[7+c_bank_bits:4+c_bank_bits];
  assign tag    = cachereq_addr[31:8+c_bank_bits];

  // tag array SRAM

  logic [23-c_bank_bits:0] tag_array_read_data;

  vc_CombinationalBitSRAM_1rw 
  #(
    .p_data_nbits  (24-c_bank_bits),
    .p_num_entries (16)
  ) tag_array
  (
    .clk        (clk),
    .reset      (reset),

    // read ports
    .read_en    (tag_array_ren),
    .read_addr  (index),
    .read_data  (tag_array_read_data),

    // write ports
    .write_en   (tag_array_wen),
    .write_addr (index),
    .write_data (tag)
  );

  // data array SRAM

  logic [127:0] data_array_read_data;

  vc_CombinationalSRAM_1rw
  #(
    .p_data_nbits  (128),
    .p_num_entries (16)
  ) data_array
  (
    .clk           (clk),
    .reset         (reset),

    // read ports
    .read_en       (data_array_ren),
    .read_addr     (index),
    .read_data     (data_array_read_data),

    // write ports
    .write_en      (data_array_wen),
    .write_byte_en (data_array_wb_en),
    .write_addr    (index),
    .write_data    (data_array_write_data)
  );

  // tag comparator

  vc_EqComparator #(24-c_bank_bits) tag_comparator
  (
    .in0 (tag),
    .in1 (tag_array_read_data),
    .out (tag_match)
  );

  // evict address mkaddr

  logic [31:0] evict_mkaddr_addr;

  lab3_mem_Mkaddr #(p_num_banks, 4) evict_mkaddr
  (
    .tag           (tag_array_read_data),
    .index         (index),
    .cachereq_addr (cachereq_addr),
    .memreq_addr   (evict_mkaddr_addr)
  );

  // evict address register

  logic [31:0] evict_addr;

  vc_EnReg #(32) evict_addr_reg
  (
    .clk   (clk),
    .reset (reset),
    .d     (evict_mkaddr_addr),
    .q     (evict_addr),
    .en    (evict_addr_reg_en)
  );

  // current address mkaddr
  logic [31:0] curr_addr;

  lab3_mem_Mkaddr #(p_num_banks, 4) curr_mkaddr
  (
    .tag           (tag),
    .index         (index),
    .cachereq_addr (cachereq_addr),
    .memreq_addr   (curr_addr)
  );

  // memreq address mux

  logic [31:0] memreq_addr;

  vc_Mux2 #(32) memreq_addr_mux
  (
    .in0 (evict_addr),
    .in1 (curr_addr),
    .sel (memreq_addr_mux_sel),
    .out (memreq_addr)
  );

  // read data zero mux

  logic [127:0] read_data_mux_data;

  vc_Mux2 #(128) read_data_zero_mux
  (
    .in0 (data_array_read_data),
    .in1 (128'b0),
    .sel (read_data_zero_mux_sel),
    .out (read_data_mux_data)
  );

  // read data register

  logic [127:0] memreq_data;

  vc_EnReg #(128) read_data_reg
  (
    .clk   (clk),
    .reset (reset),
    .d     (read_data_mux_data),
    .q     (memreq_data),
    .en    (read_data_reg_en)
  );

  // cache data mux

  logic [31:0] cacheresp_data;

  vc_Mux4 #(32) cache_data_mux
  (
    .in0 (memreq_data[31:0]),
    .in1 (memreq_data[63:32]),
    .in2 (memreq_data[95:64]),
    .in3 (memreq_data[127:96]),
    .sel (offset), // offset
    .out (cacheresp_data)
  );

  // construct cache response message
  always_comb begin
    proc2cache_respstream_msg.opaque = cachereq_opaque;
    proc2cache_respstream_msg.type_  = cacheresp_type;
    proc2cache_respstream_msg.len    = 2'b0;
    proc2cache_respstream_msg.test   = hit;
    proc2cache_respstream_msg.data   = cacheresp_data;
  end

  // construct memory request message
  always_comb begin
    cache2mem_reqstream_msg.type_  = memreq_type;
    cache2mem_reqstream_msg.len    = 4'b0;
    cache2mem_reqstream_msg.addr   = memreq_addr;
    cache2mem_reqstream_msg.data   = memreq_data;
    cache2mem_reqstream_msg.opaque = 8'b0;
  end

endmodule

`endif
