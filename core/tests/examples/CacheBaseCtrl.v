//=========================================================================
// Base Blocking Cache Control
//=========================================================================

`ifndef LAB3_MEM_CACHE_BASE_CTRL_V
`define LAB3_MEM_CACHE_BASE_CTRL_V

`include "vc/mem-msgs.v"
`include "vc/regfiles.v"


module lab3_mem_CacheBaseCtrl
#(
  parameter p_num_banks = 1
)
(
  input  logic        clk,
  input  logic        reset,

  // Processor <-> Cache Interface

  input  logic        proc2cache_reqstream_val,
  output logic        proc2cache_reqstream_rdy,

  output logic        proc2cache_respstream_val,
  input  logic        proc2cache_respstream_rdy,

  // Cache <-> Memory Interface

  output logic        cache2mem_reqstream_val,
  input  logic        cache2mem_reqstream_rdy,

  input  logic        cache2mem_respstream_val,
  output logic        cache2mem_respstream_rdy,

  // Control Unit -> Datapath
  output logic        cachereq_en,
  output logic        memresp_en,
  output logic        write_data_mux_sel,
  output logic        tag_array_ren,
  output logic        tag_array_wen,
  output logic        data_array_ren,
  output logic        data_array_wen,
  output logic        refill,
  output logic        read_data_zero_mux_sel,
  output logic        read_data_reg_en,
  output logic        evict_addr_reg_en,
  output logic        memreq_addr_mux_sel,
  output logic [3:0]  cacheresp_type,
  output logic [3:0]  memreq_type,
  output logic        hit,

  // Datapath -> Control Unit
  input  logic [3:0]  cachereq_type,
  input  logic [3:0]  index,
  input  logic        tag_match
);

  //----------------------------------------------------------------------
  // State Definitions
  //----------------------------------------------------------------------

  localparam STATE_IDLE              = 5'd0;
  localparam STATE_TAG_CHECK         = 5'd1;
  localparam STATE_INIT_DATA_ACCESS  = 5'd2;
  localparam STATE_READ_DATA_ACCESS  = 5'd3;
  localparam STATE_WRITE_DATA_ACCESS = 5'd4;
  localparam STATE_REFILL_REQUEST    = 5'd5;
  localparam STATE_REFILL_WAIT       = 5'd6;
  localparam STATE_REFILL_UPDATE     = 5'd7;
  localparam STATE_EVICT_PREPARE     = 5'd8;
  localparam STATE_EVICT_REQUEST     = 5'd9;
  localparam STATE_EVICT_WAIT        = 5'd10;
  localparam STATE_WAIT              = 5'd11;

  // manage state

  logic [4:0] state_reg;
  logic [4:0] state_next;

  always_ff @( posedge clk ) begin
    if ( reset ) begin
      state_reg <= STATE_IDLE;
    end
    else begin
      state_reg <= state_next;
    end
  end

  // state transitions
  logic valid, dirty, hit_TC;

  logic init, read, write;
  logic memreq_rdy, memresp_val, cachereq_val, cacheresp_rdy;

  assign init   = (cachereq_type == `VC_MEM_REQ_MSG_TYPE_WRITE_INIT);
  assign read   = (cachereq_type == `VC_MEM_REQ_MSG_TYPE_READ);
  assign write  = (cachereq_type == `VC_MEM_REQ_MSG_TYPE_WRITE);
  assign hit_TC = (tag_match && valid);
  
  assign memreq_rdy    = cache2mem_reqstream_rdy;
  assign memresp_val   = cache2mem_respstream_val;
  assign cachereq_val  = proc2cache_reqstream_val;
  assign cacheresp_rdy = proc2cache_respstream_rdy;

  always_comb begin
    state_next = state_reg;

    case ( state_reg )
      STATE_IDLE: if ( cachereq_val ) state_next = STATE_TAG_CHECK;
      STATE_TAG_CHECK: begin
        if      ( init ) state_next = STATE_INIT_DATA_ACCESS;
        else if ( read && hit_TC    ) state_next = STATE_READ_DATA_ACCESS;
        else if ( write && hit_TC   ) state_next = STATE_WRITE_DATA_ACCESS;
        else if ( !hit_TC && !dirty ) state_next = STATE_REFILL_REQUEST;
        else if ( !hit_TC && dirty  ) state_next = STATE_EVICT_PREPARE;
      end
      STATE_INIT_DATA_ACCESS: state_next = STATE_WAIT;
      STATE_READ_DATA_ACCESS: state_next = STATE_WAIT;
      STATE_WRITE_DATA_ACCESS: state_next = STATE_WAIT;
      STATE_REFILL_REQUEST: if ( memreq_rdy ) state_next = STATE_REFILL_WAIT;
      STATE_REFILL_WAIT: if ( memresp_val ) state_next = STATE_REFILL_UPDATE;
      STATE_REFILL_UPDATE: if ( read ) state_next = STATE_READ_DATA_ACCESS;
                           else        state_next = STATE_WRITE_DATA_ACCESS;
      STATE_EVICT_PREPARE: state_next = STATE_EVICT_REQUEST;
      STATE_EVICT_REQUEST: if ( memreq_rdy ) state_next = STATE_EVICT_WAIT;
      STATE_EVICT_WAIT: if ( memresp_val ) state_next = STATE_REFILL_REQUEST;
      STATE_WAIT: if ( cacheresp_rdy ) state_next = STATE_IDLE;
      default: state_next = STATE_IDLE;
    endcase

  end

  // state outputs
  logic set_valid;
  logic set_dirty;
  logic dirty_wdata;

  function void cs
  (
    input logic        cs_preq_rdy,
    input logic        cs_prsp_val,
    input logic        cs_mreq_val,
    input logic        cs_mrsp_rdy,
    input logic        cs_cachereq_en,
    input logic        cs_memresp_en,
    input logic        cs_write_data_mux_sel,
    input logic        cs_tag_array_ren,
    input logic        cs_tag_array_wen,
    input logic        cs_data_array_ren,
    input logic        cs_data_array_wen,
    input logic        cs_refill,
    input logic        cs_set_valid,
    input logic        cs_set_dirty,
    input logic        cs_dirty_wdata,
    input logic        cs_read_data_zero_mux_sel,
    input logic        cs_read_data_reg_en,
    input logic        cs_evict_addr_reg_en,
    input logic        cs_memreq_addr_mux_sel,
    input logic [3:0]  cs_memreq_type
  );
  begin
    proc2cache_reqstream_rdy  = cs_preq_rdy;
    proc2cache_respstream_val = cs_prsp_val;
    cache2mem_reqstream_val   = cs_mreq_val;
    cache2mem_respstream_rdy  = cs_mrsp_rdy;
    cachereq_en               = cs_cachereq_en;
    memresp_en                = cs_memresp_en;
    write_data_mux_sel        = cs_write_data_mux_sel;
    tag_array_ren             = cs_tag_array_ren;
    tag_array_wen             = cs_tag_array_wen;
    data_array_ren            = cs_data_array_ren;
    data_array_wen            = cs_data_array_wen;
    refill                    = cs_refill;
    set_valid                 = cs_set_valid;
    set_dirty                 = cs_set_dirty;
    dirty_wdata               = cs_dirty_wdata;
    read_data_zero_mux_sel    = cs_read_data_zero_mux_sel;
    read_data_reg_en          = cs_read_data_reg_en;
    evict_addr_reg_en         = cs_evict_addr_reg_en;
    memreq_addr_mux_sel       = cs_memreq_addr_mux_sel;
    memreq_type               = cs_memreq_type;
  end
  endfunction

  // Generic Parameters -- yes or no or don't care
  localparam n = 1'b0;
  localparam y = 1'b1;
  localparam d = 1'bx;

  // Memory types
  localparam rd = `VC_MEM_REQ_MSG_TYPE_READ;
  localparam wr = `VC_MEM_REQ_MSG_TYPE_WRITE;
  localparam in = `VC_MEM_REQ_MSG_TYPE_WRITE_INIT;

  always_comb begin
    cs( 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
    case ( state_reg )
      //                          preq prsp mreq mrsp creq mresp wdm tarr tarr darr darr ref set set dir rdzm rdr ear mreq mreq
      //                          rdy  val  val  rdy  en   en    sel ren  wen  ren  wen  ill val dir dat sel  en  reg sel  type
      STATE_IDLE:              cs( y,  n,   n,   n,   y,   n,    d,  n,   n,   n,   n,   n,  n,  n,  d,   d,   n,  n,  d,   'x  );
      STATE_TAG_CHECK:         cs( n,  n,   n,   n,   n,   n,    d,  y,   n,   n,   n,   n,  n,  n,  d,   1,   y,  n,  d,   'x  );
      STATE_INIT_DATA_ACCESS:  cs( n,  n,   n,   n,   n,   n,    0,  n,   y,   n,   y,   n,  y,  y,  n,   d,   n,  n,  d,   'x  );
      STATE_READ_DATA_ACCESS:  cs( n,  n,   n,   n,   n,   n,    d,  n,   n,   y,   n,   n,  n,  n,  d,   0,   y,  n,  n,   'x  );
      STATE_WRITE_DATA_ACCESS: cs( n,  n,   n,   n,   n,   n,    0,  n,   y,   n,   y,   n,  y,  y,  y,   d,   n,  n,  d,   'x  );
      STATE_REFILL_REQUEST:    cs( n,  n,   y,   n,   n,   n,    n,  n,   n,   n,   n,   n,  n,  n,  d,   d,   n,  n,  1,   rd  );
      STATE_REFILL_WAIT:       cs( n,  n,   n,   y,   n,   y,    n,  n,   n,   n,   n,   n,  n,  n,  d,   d,   n,  n,  n,   'x  );
      STATE_REFILL_UPDATE:     cs( n,  n,   n,   n,   n,   n,    1,  n,   y,   n,   y,   y,  y,  y,  n,   d,   n,  n,  n,   'x  );
      STATE_EVICT_PREPARE:     cs( n,  n,   n,   n,   n,   n,    d,  y,   n,   y,   n,   n,  n,  n,  d,   0,   y,  y,  d,   'x  );
      STATE_EVICT_REQUEST:     cs( n,  n,   y,   n,   n,   n,    d,  n,   n,   n,   n,   n,  n,  n,  d,   d,   n,  n,  0,   wr  );
      STATE_EVICT_WAIT:        cs( n,  n,   n,   y,   n,   n,    d,  n,   n,   n,   n,   n,  n,  n,  d,   1,   y,  n,  d,   'x  );
      STATE_WAIT:              cs( n,  y,   n,   n,   n,   n,    n,  n,   n,   n,   n,   n,  n,  n,  d,   n,   n,  n,  d,   'x  );

      default: cs('x, 'x, 'x, 'x, 'x, 'x, 'x, 'x, 'x, 'x, 'x, 'x, 'x, 'x, 'x, 'x, 'x, 'x, 'x, 'x );
    endcase
  end

  // update cache response message
  always_ff @( posedge clk ) begin
    if (reset) begin
      cacheresp_type <= 4'b0;
      hit            <= 1'b0;
    end else if (state_reg == STATE_TAG_CHECK) begin
      cacheresp_type <= cachereq_type;
      hit            <= (cachereq_type == in) ? 1'b0 : hit_TC;
    end
  end

  // valid bit register file
  vc_ResetRegfile_1r1w
  #(
    .p_data_nbits  (1),
    .p_num_entries (16),
    .p_reset_value (0)
  ) valid_bits
  (
    .clk        (clk),
    .reset      (reset),

    // read ports
    .read_addr  (index),
    .read_data  (valid),

    // write ports
    .write_en   (set_valid),
    .write_addr (index),
    .write_data (y)
  );

  // dirty bit register file
  vc_ResetRegfile_1r1w
  #(
    .p_data_nbits  (1),
    .p_num_entries (16),
    .p_reset_value (0)
  ) dirty_bits
  (
    .clk        (clk),
    .reset      (reset),

    // read ports
    .read_addr  (index),
    .read_data  (dirty),

    // write ports
    .write_en   (set_dirty),
    .write_addr (index),
    .write_data (dirty_wdata)
  );


endmodule

`endif
