//=========================================================================
// 5-Stage Simple Pipelined Processor
//=========================================================================

`ifndef LAB2_PROC_PROC_BASE_V
`define LAB2_PROC_PROC_BASE_V

`include "vc/mem-msgs.v"
`include "vc/queues.v"
`include "vc/trace.v"

`include "lab2_proc/tinyrv2_encoding.v"
`include "lab2_proc/ProcBaseCtrl.v"
`include "lab2_proc/ProcBaseDpath.v"
`include "lab2_proc/DropUnit.v"

module lab2_proc_ProcBase
#(
  parameter p_num_cores = 1
)
(
  input  logic         clk,
  input  logic         reset,

  // From mngr streaming port

  input  logic [31:0]  mngr2proc_msg,
  input  logic         mngr2proc_val,
  output logic         mngr2proc_rdy,

  // To mngr streaming port

  output logic [31:0]  proc2mngr_msg,
  output logic         proc2mngr_val,
  input  logic         proc2mngr_rdy,

  // Instruction Memory Request Port

  output mem_req_4B_t  imem_reqstream_msg,
  output logic         imem_reqstream_val,
  input  logic         imem_reqstream_rdy,

  // Instruction Memory Response Port

  input  mem_resp_4B_t imem_respstream_msg,
  input  logic         imem_respstream_val,
  output logic         imem_respstream_rdy,

  // Data Memory Request Port

  output mem_req_4B_t  dmem_reqstream_msg,
  output logic         dmem_reqstream_val,
  input  logic         dmem_reqstream_rdy,

  // Data Memory Response Port

  input  mem_resp_4B_t dmem_respstream_msg,
  input  logic         dmem_respstream_val,
  output logic         dmem_respstream_rdy,

  // extra ports; note that core_id is an input port rather than a
  // parameter so that the module only needs to be compiled once. If it
  // were a parameter, each core would be compiled separately.

  input  logic [31:0]  core_id,
  output logic         commit_inst,
  output logic         stats_en

);

  //----------------------------------------------------------------------
  // Instruction Memory Request Bypass Queue
  //----------------------------------------------------------------------

  logic [1:0]  imem_queue_num_free_entries;
  mem_req_4B_t imem_reqstream_enq_msg;
  logic        imem_reqstream_enq_val;
  logic        imem_reqstream_enq_rdy;

  logic [31:0] imem_reqstream_enq_msg_addr;

  assign imem_reqstream_enq_msg.type_  = `VC_MEM_REQ_MSG_TYPE_READ;
  assign imem_reqstream_enq_msg.opaque = 8'b0;
  assign imem_reqstream_enq_msg.addr   = imem_reqstream_enq_msg_addr;
  assign imem_reqstream_enq_msg.len    = 2'd0;
  assign imem_reqstream_enq_msg.data   = 32'bx;

  vc_Queue#(`VC_QUEUE_BYPASS,$bits(mem_req_4B_t),2) imem_queue
  (
    .clk     (clk),
    .reset   (reset),
    .num_free_entries(imem_queue_num_free_entries),

    .enq_msg (imem_reqstream_enq_msg),
    .enq_val (imem_reqstream_enq_val),
    .enq_rdy (imem_reqstream_enq_rdy),

    .deq_msg (imem_reqstream_msg),
    .deq_val (imem_reqstream_val),
    .deq_rdy (imem_reqstream_rdy)
  );

  //----------------------------------------------------------------------
  // Imem Drop Unit
  //----------------------------------------------------------------------

  logic         imem_respstream_drop;
  mem_resp_4B_t imem_respstream_drop_msg;
  logic         imem_respstream_drop_val;
  logic         imem_respstream_drop_rdy;

  lab2_proc_DropUnit #($bits(mem_resp_4B_t)) imem_respstream_drop_unit
  (
    .clk         (clk),
    .reset       (reset),

    .drop        (imem_respstream_drop),

    .istream_msg (imem_respstream_msg),
    .istream_val (imem_respstream_val),
    .istream_rdy (imem_respstream_rdy),

    .ostream_msg (imem_respstream_drop_msg),
    .ostream_val (imem_respstream_drop_val),
    .ostream_rdy (imem_respstream_drop_rdy)
  );

  //----------------------------------------------------------------------
  // Data Memory Request Bypass Queue
  //----------------------------------------------------------------------

  logic        dmem_queue_num_free_entries;
  mem_req_4B_t dmem_reqstream_enq_msg;
  logic        dmem_reqstream_enq_val;
  logic        dmem_reqstream_enq_rdy;

  logic [31:0] dmem_reqstream_enq_msg_addr;
  logic [31:0] dmem_reqstream_enq_msg_data;
  logic [3:0]  dmem_reqstream_enq_msg_type;

  assign dmem_reqstream_enq_msg.type_  = dmem_reqstream_enq_msg_type;
  assign dmem_reqstream_enq_msg.opaque = 8'b0;
  assign dmem_reqstream_enq_msg.addr   = dmem_reqstream_enq_msg_addr;
  assign dmem_reqstream_enq_msg.len    = 2'd0;
  assign dmem_reqstream_enq_msg.data   = dmem_reqstream_enq_msg_data;

  vc_Queue#(`VC_QUEUE_BYPASS,$bits(mem_req_4B_t),1) dmem_queue
  (
    .clk     (clk),
    .reset   (reset),
    .num_free_entries(dmem_queue_num_free_entries),

    .enq_msg (dmem_reqstream_enq_msg),
    .enq_val (dmem_reqstream_enq_val),
    .enq_rdy (dmem_reqstream_enq_rdy),

    .deq_msg (dmem_reqstream_msg),
    .deq_val (dmem_reqstream_val),
    .deq_rdy (dmem_reqstream_rdy)
  );

  //----------------------------------------------------------------------
  // proc2mngr Bypass Queue
  //----------------------------------------------------------------------

  logic        proc2mngr_queue_num_free_entries;
  logic [31:0] proc2mngr_enq_msg;
  logic        proc2mngr_enq_val;
  logic        proc2mngr_enq_rdy;

  vc_Queue#(`VC_QUEUE_BYPASS,32,1) proc2mngr_queue
  (
    .clk     (clk),
    .reset   (reset),
    .num_free_entries(proc2mngr_queue_num_free_entries),

    .enq_msg (proc2mngr_enq_msg),
    .enq_val (proc2mngr_enq_val),
    .enq_rdy (proc2mngr_enq_rdy),

    .deq_msg (proc2mngr_msg),
    .deq_val (proc2mngr_val),
    .deq_rdy (proc2mngr_rdy)
  );

  //----------------------------------------------------------------------
  // Multiplier Ports
  //----------------------------------------------------------------------
  logic         imul_req_val_D;
  logic         imul_req_rdy_D;

  logic         imul_resp_val_X;
  logic         imul_resp_rdy_X;

  //----------------------------------------------------------------------
  // Control/Status Signals
  //----------------------------------------------------------------------

  // control signals (ctrl->dpath)

  logic        reg_en_F;
  logic [1:0]  pc_sel_F;

  logic        reg_en_D;
  logic        op1_sel_D;
  logic [1:0]  op2_sel_D;
  logic [1:0]  csrr_sel_D;
  logic [2:0]  imm_type_D;

  logic        reg_en_X;
  logic        dmem_fn_X;
  logic [3:0]  alu_fn_X;
  logic [1:0]  ex_result_sel_X;

  logic        reg_en_M;
  logic        wb_result_sel_M;

  logic        reg_en_W;
  logic [4:0]  rf_waddr_W;
  logic        rf_wen_W;
  logic        stats_en_wen_W;

  // status signals (dpath->ctrl)

  logic [31:0] inst_D;
  logic        br_cond_eq_X;
  logic        br_cond_lt_X;
  logic        br_cond_ltu_X;

  //----------------------------------------------------------------------
  // Control Unit
  //----------------------------------------------------------------------

  lab2_proc_ProcBaseCtrl ctrl
  (
    // Instruction Memory Port

    .imem_reqstream_val       (imem_reqstream_enq_val),
    .imem_reqstream_rdy       (imem_reqstream_enq_rdy),
    .imem_respstream_val      (imem_respstream_drop_val),
    .imem_respstream_rdy      (imem_respstream_drop_rdy),

    // Data Memory Port

    .dmem_reqstream_val       (dmem_reqstream_enq_val),
    .dmem_reqstream_rdy       (dmem_reqstream_enq_rdy),
    .dmem_respstream_val      (dmem_respstream_val),
    .dmem_respstream_rdy      (dmem_respstream_rdy),

    // mngr communication ports

    .mngr2proc_val            (mngr2proc_val),
    .mngr2proc_rdy            (mngr2proc_rdy),
    .proc2mngr_val            (proc2mngr_enq_val),
    .proc2mngr_rdy            (proc2mngr_enq_rdy),

    // clk/reset/control/status signals

    .*
  );

  //----------------------------------------------------------------------
  // Datapath
  //----------------------------------------------------------------------

  lab2_proc_ProcBaseDpath
  #(
    .p_num_cores              (p_num_cores)
  )
  dpath
  (
    // Instruction Memory Port

    .imem_reqstream_msg_addr  (imem_reqstream_enq_msg_addr),
    .imem_respstream_msg      (imem_respstream_drop_msg),

    // Data Memory Port

    .dmem_reqstream_msg_addr  (dmem_reqstream_enq_msg_addr),
    .dmem_reqstream_msg_data  (dmem_reqstream_enq_msg_data),
    .dmem_reqstream_msg_type  (dmem_reqstream_enq_msg_type),
    .dmem_respstream_msg_data (dmem_respstream_msg.data),

    // mngr communication ports

    .mngr2proc_data           (mngr2proc_msg),
    .proc2mngr_data           (proc2mngr_enq_msg),

    // clk/reset/control/status signals

    .*
  );

  //----------------------------------------------------------------------
  // Line tracing
  //----------------------------------------------------------------------

  `ifndef SYNTHESIS

  lab2_proc_tinyrv2_encoding_InstTasks tinyrv2();

  logic [`VC_TRACE_NBITS-1:0] str;
  `VC_TRACE_BEGIN
  begin

    if ( !ctrl.val_F )
      vc_trace.append_chars( trace_str, " ", 8 );
    else if ( ctrl.squash_F ) begin
      vc_trace.append_str( trace_str, "/" );
      vc_trace.append_chars( trace_str, " ", 8-1 );
    end else if ( ctrl.stall_F ) begin
      vc_trace.append_str( trace_str, "#" );
      vc_trace.append_chars( trace_str, " ", 8-1 );
    end else begin
      $sformat( str, "%x", dpath.pc_F );
      vc_trace.append_str( trace_str, str );
    end

    vc_trace.append_str( trace_str, "|" );

    if ( !ctrl.val_D )
      vc_trace.append_chars( trace_str, " ", 23 );
    else if ( ctrl.squash_D ) begin
      vc_trace.append_str( trace_str, "/" );
      vc_trace.append_chars( trace_str, " ", 23-1 );
    end else if ( ctrl.stall_D ) begin
      vc_trace.append_str( trace_str, "#" );
      vc_trace.append_chars( trace_str, " ", 23-1 );
    end else
      vc_trace.append_str( trace_str, { 3896'b0, tinyrv2.disasm( ctrl.inst_D ) } );

    vc_trace.append_str( trace_str, "|" );

    if ( !ctrl.val_X )
      vc_trace.append_chars( trace_str, " ", 4 );
    else if ( ctrl.stall_X ) begin
      vc_trace.append_str( trace_str, "#" );
      vc_trace.append_chars( trace_str, " ", 4-1 );
    end else
      vc_trace.append_str( trace_str, { 4064'b0, tinyrv2.disasm_tiny( ctrl.inst_X ) } );

    vc_trace.append_str( trace_str, "|" );

    if ( !ctrl.val_M )
      vc_trace.append_chars( trace_str, " ", 4 );
    else if ( ctrl.stall_M ) begin
      vc_trace.append_str( trace_str, "#" );
      vc_trace.append_chars( trace_str, " ", 4-1 );
    end else
      vc_trace.append_str( trace_str, { 4064'b0, tinyrv2.disasm_tiny( ctrl.inst_M ) } );

    vc_trace.append_str( trace_str, "|" );

    if ( !ctrl.val_W )
      vc_trace.append_chars( trace_str, " ", 4 );
    else if ( ctrl.stall_W ) begin
      vc_trace.append_str( trace_str, "#" );
      vc_trace.append_chars( trace_str, " ", 4-1 );
    end else
      vc_trace.append_str( trace_str, { 4064'b0, tinyrv2.disasm_tiny( ctrl.inst_W ) } );

  end
  `VC_TRACE_END

  // These trace modules are useful because they breakout all the
  // individual fields so you can see them in gtkwave

  vc_MemReqMsg4BTrace imem_reqstream_trace
  (
    .clk   (clk),
    .reset (reset),
    .msg   (imem_reqstream_msg),
    .val   (imem_reqstream_val),
    .rdy   (imem_reqstream_rdy)
  );

  vc_MemReqMsg4BTrace dmem_reqstream_trace
  (
    .clk   (clk),
    .reset (reset),
    .msg   (dmem_reqstream_msg),
    .val   (dmem_reqstream_val),
    .rdy   (dmem_reqstream_rdy)
  );

  vc_MemRespMsg4BTrace imem_respstream_trace
  (
    .clk   (clk),
    .reset (reset),
    .msg   (imem_respstream_msg),
    .val   (imem_respstream_val),
    .rdy   (imem_respstream_rdy)
  );

  vc_MemRespMsg4BTrace dmem_respstream_trace
  (
    .clk   (clk),
    .reset (reset),
    .msg   (dmem_respstream_msg),
    .val   (dmem_respstream_val),
    .rdy   (dmem_respstream_rdy)
  );

  `endif

endmodule

`endif
