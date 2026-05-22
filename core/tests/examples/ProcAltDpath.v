//=========================================================================
// 5-Stage Fully Bypassed Pipelined Processor Datapath
//=========================================================================

`ifndef LAB2_PROC_PROC_ALT_DPATH_V
`define LAB2_PROC_PROC_ALT_DPATH_V

`include "vc/arithmetic.v"
`include "vc/mem-msgs.v"
`include "vc/muxes.v"
`include "vc/regs.v"
`include "vc/regfiles.v"

`include "lab1_imul/IntMulAlt.v"
`include "lab2_proc/tinyrv2_encoding.v"
`include "lab2_proc/ProcDpathImmGen.v"
`include "lab2_proc/ProcDpathAlu.v"
`include "lab2_proc/BranchTargetBuffer.v"

module lab2_proc_ProcAltDpath
#(
  parameter p_num_cores = 1
)
(
  input  logic         clk,
  input  logic         reset,

  // Instruction Memory Port

  output logic [31:0]  imem_reqstream_msg_addr,
  input  mem_resp_4B_t imem_respstream_msg,

  // Data Memory Port

  output logic [31:0]  dmem_reqstream_msg_addr,
  output logic [31:0]  dmem_reqstream_msg_data,
  output logic [3:0]   dmem_reqstream_msg_type,
  input  logic [31:0]  dmem_respstream_msg_data,

  // mngr communication ports

  input  logic [31:0]  mngr2proc_data,
  output logic [31:0]  proc2mngr_data,

  // multiply ports
  input  logic        imul_req_val_D,
  output logic        imul_req_rdy_D,
  output logic        imul_resp_val_X,
  input  logic        imul_resp_rdy_X,

  // control signals (ctrl->dpath)

  input  logic         imem_respstream_drop,

  input  logic         reg_en_F,
  input  logic [1:0]   pc_sel_F,

  input  logic         reg_en_D,
  input  logic [1:0]   op1_bypass_sel_D,
  input  logic [1:0]   op2_bypass_sel_D,
  input  logic         op1_sel_D,
  input  logic [1:0]   op2_sel_D,
  input  logic [1:0]   csrr_sel_D,
  input  logic [2:0]   imm_type_D,

  input  logic         reg_en_X,
  input  logic         dmem_fn_X,
  input  logic [3:0]   alu_fn_X,
  input  logic [1:0]   ex_result_sel_X,
  input  logic         branch_taken_X,

  input  logic         reg_en_M,
  input  logic         wb_result_sel_M,

  input  logic         reg_en_W,
  input  logic [4:0]   rf_waddr_W,
  input  logic         rf_wen_W,
  input  logic         stats_en_wen_W,

  // status signals (dpath->ctrl)

  output logic         take_branch_F,
  output logic [31:0]  inst_D,
  output logic         br_cond_eq_X,
  output logic         br_cond_lt_X,
  output logic         br_cond_ltu_X,

  // extra ports

  input  logic [31:0]  core_id,
  output logic         stats_en
);

  localparam c_reset_vector = 32'h200;
  localparam c_reset_inst   = 32'h00000000;

  // Fetch address

  assign imem_reqstream_msg_addr = pc_next_F;

  //--------------------------------------------------------------------
  // F stage
  //--------------------------------------------------------------------

  logic [31:0] pc_F;
  logic [31:0] pc_next_F;
  logic [31:0] pc_plus4_F;
  logic [31:0] br_target_X;
  logic [31:0] jal_target_D;

  vc_EnResetReg#(32, c_reset_vector - 32'd4) pc_reg_F
  (
    .clk    (clk),
    .reset  (reset),
    .en     (reg_en_F),
    .d      (pc_next_F),
    .q      (pc_F)
  );

  vc_Incrementer#(32, 4) pc_incr_F
  (
    .in   (pc_F),
    .out  (pc_plus4_F)
  );

  // forward declaration (for btb)
  logic [31:0] pc_X;
  logic [31:0] pc_plus4_X;

  // branch target buffer
  logic [31:0] btb_target_F;

  lab2_proc_BranchTargetBuffer btb
  (
    .clk           (clk),
    .reset         (reset),

    .curr_pc       (pc_F),
    .branch_target (btb_target_F),
    .take_branch   (take_branch_F),

    .branch_pc     (pc_X),
    .branch_dest   (br_target_X),
    .branch_taken  (branch_taken_X)
  );

  // prediction
  logic [31:0] br_predict_F;

  vc_Mux2#(32) br_predict_mux_F
  (
    .in0 (pc_plus4_F),
    .in1 (btb_target_F),
    .sel (take_branch_F),
    .out (br_predict_F)
  );

  // misprediction
  logic [31:0] br_mispredict_X;

  vc_Mux4#(32) pc_sel_mux_F
  (
    .in0  (br_predict_F),
    .in1  (br_mispredict_X),
    .in2  (jal_target_D),
    .in3  (alu_result_X),
    .sel  (pc_sel_F),
    .out  (pc_next_F)
  );

  //--------------------------------------------------------------------
  // D stage
  //--------------------------------------------------------------------

  logic [31:0] pc_D;
  logic [ 4:0] inst_rd_D;
  logic [ 4:0] inst_rs1_D;
  logic [ 4:0] inst_rs2_D;
  logic [31:0] imm_D;

  vc_EnResetReg#(32) pc_reg_D
  (
    .clk    (clk),
    .reset  (reset),
    .en     (reg_en_D),
    .d      (pc_F),
    .q      (pc_D)
  );

  vc_EnResetReg#(32, c_reset_inst) inst_D_reg
  (
    .clk    (clk),
    .reset  (reset),
    .en     (reg_en_D),
    .d      (imem_respstream_msg.data),
    .q      (inst_D)
  );

  lab2_proc_tinyrv2_encoding_InstUnpack inst_unpack
  (
    .opcode   (),
    .inst     (inst_D),
    .rs1      (inst_rs1_D),
    .rs2      (inst_rs2_D),
    .rd       (inst_rd_D),
    .funct3   (),
    .funct7   (),
    .csr      ()
  );

  lab2_proc_ProcDpathImmGen imm_gen_D
  (
    .imm_type (imm_type_D),
    .inst     (inst_D),
    .imm      (imm_D)
  );

  logic [31:0] rf_rdata0_D;
  logic [31:0] rf_rdata1_D;
  logic [31:0] rf_wdata_W;

  vc_Regfile_2r1w_zero rf
  (
    .clk      (clk),
    .reset    (reset),
    .rd_addr0 (inst_rs1_D),
    .rd_data0 (rf_rdata0_D),
    .rd_addr1 (inst_rs2_D),
    .rd_data1 (rf_rdata1_D),
    .wr_en    (rf_wen_W),
    .wr_addr  (rf_waddr_W),
    .wr_data  (rf_wdata_W)
  );

  logic [31:0] csrr_data_D;

  logic [31:0] num_cores;
  assign num_cores = p_num_cores;

  // csrr data select mux
  vc_Mux3#(32) csrr_sel_mux_D
  (
   .in0  (mngr2proc_data),
   .in1  (num_cores),
   .in2  (core_id),
   .sel  (csrr_sel_D),
   .out  (csrr_data_D)
  );

  logic [31:0] op1_bypass_D;
  logic [31:0] op2_bypass_D;

  // bypass mux for op1
  vc_Mux4#(32) op1_bypass_mux_D
  (
    .in0  (rf_rdata0_D),
    .in1  (ex_result_X),
    .in2  (wb_result_M),
    .in3  (wb_result_W),
    .sel  (op1_bypass_sel_D),
    .out  (op1_bypass_D)
  );

  // bypass mux for op2
  vc_Mux4#(32) op2_bypass_mux_D
  (
    .in0  (rf_rdata1_D),
    .in1  (ex_result_X),
    .in2  (wb_result_M),
    .in3  (wb_result_W),
    .sel  (op2_bypass_sel_D),
    .out  (op2_bypass_D)
  );

  logic [31:0] op1_D;
  logic [31:0] op2_D;

  // op1 select mux
  // This mux chooses among PC and RS1
  vc_Mux2#(32) op1_sel_mux_D
  (
    .in0  (pc_D),
    .in1  (op1_bypass_D),
    .sel  (op1_sel_D),
    .out  (op1_D)
  );

  // op2 select mux
  // This mux chooses among RS2, imm, and the output of the above csrr
  // csrr sel mux. Basically we are using two muxes here for pedagogy.
  vc_Mux3#(32) op2_sel_mux_D
  (
    .in0  (op2_bypass_D),
    .in1  (imm_D),
    .in2  (csrr_data_D),
    .sel  (op2_sel_D),
    .out  (op2_D)
  );

  vc_Adder#(32) pc_plus_imm_D
  (
    .in0  (pc_D),
    .in1  (imm_D),
    .cin  (1'b0),
    .out  (jal_target_D),
    .cout ()
  );

  logic [63:0] imul_req_msg;
  logic [31:0] imul_resp_msg;
  assign imul_req_msg = {op1_D, op2_D};

  //--------------------------------------------------------------------
  // Multiplier (between D and X stages)
  //--------------------------------------------------------------------
  lab1_imul_IntMulAlt multiplier
  (
    .clk         (clk),
    .reset       (reset),

    .istream_val (imul_req_val_D),
    .istream_rdy (imul_req_rdy_D),
    .istream_msg (imul_req_msg),

    .ostream_val (imul_resp_val_X),
    .ostream_rdy (imul_resp_rdy_X),
    .ostream_msg (imul_resp_msg)
  );

  //--------------------------------------------------------------------
  // X stage
  //--------------------------------------------------------------------

  logic [31:0] op1_X;
  logic [31:0] op2_X;

  vc_EnResetReg#(32, 0) op1_reg_X
  (
    .clk   (clk),
    .reset (reset),
    .en    (reg_en_X),
    .d     (op1_D),
    .q     (op1_X)
  );

  vc_EnResetReg#(32, 0) op2_reg_X
  (
    .clk   (clk),
    .reset (reset),
    .en    (reg_en_X),
    .d     (op2_D),
    .q     (op2_X)
  );

  vc_EnResetReg#(32, 0) pc_reg_X
  (
    .clk   (clk),
    .reset (reset),
    .en    (reg_en_X),
    .d     (pc_D),
    .q     (pc_X)
  );

  vc_EnResetReg#(32, 0) br_target_reg_X
  (
    .clk   (clk),
    .reset (reset),
    .en    (reg_en_X),
    .d     (jal_target_D),
    .q     (br_target_X)
  );

  vc_Mux2#(32) br_mispredict_mux_X
  (
    .in0 (pc_plus4_X),
    .in1 (br_target_X),
    .sel (branch_taken_X),
    .out (br_mispredict_X)
  );

  vc_EnResetReg#(32, 0) dmem_write_data_reg_X
  (
    .clk   (clk),
    .reset (reset),
    .en    (reg_en_X),
    .d     (op2_bypass_D),
    .q     (dmem_reqstream_msg_data)
  );

  logic [31:0] alu_result_X;
  logic [31:0] ex_result_X;

  lab2_proc_ProcDpathAlu alu
  (
    .in0      (op1_X),
    .in1      (op2_X),
    .fn       (alu_fn_X),
    .out      (alu_result_X),
    .ops_eq   (br_cond_eq_X),
    .ops_lt   (br_cond_lt_X),
    .ops_ltu  (br_cond_ltu_X)
  );

  vc_Incrementer#(32, 4) pc_incr_X
  (
    .in   (pc_X),
    .out  (pc_plus4_X)
  );

  vc_Mux2#(32) dmem_type_sel_mux_X
  (
    .in0  (`VC_MEM_REQ_MSG_TYPE_READ),
    .in1  (`VC_MEM_REQ_MSG_TYPE_WRITE),
    .sel  (dmem_fn_X),
    .out  (dmem_reqstream_msg_type)
  );

  vc_Mux3#(32) ex_result_sel_mux_X
  (
    .in0  (pc_plus4_X),
    .in1  (alu_result_X),
    .in2  (imul_resp_msg),
    .sel  (ex_result_sel_X),
    .out  (ex_result_X)
  );

  assign dmem_reqstream_msg_addr = alu_result_X;

  //--------------------------------------------------------------------
  // M stage
  //--------------------------------------------------------------------

  logic [31:0] ex_result_M;

  vc_EnResetReg#(32, 0) ex_result_reg_M
  (
    .clk    (clk),
    .reset  (reset),
    .en     (reg_en_M),
    .d      (ex_result_X),
    .q      (ex_result_M)
  );

  logic [31:0] dmem_result_M;
  logic [31:0] wb_result_M;

  assign dmem_result_M = dmem_respstream_msg_data;

  vc_Mux2#(32) wb_result_sel_mux_M
  (
    .in0    (ex_result_M),
    .in1    (dmem_result_M),
    .sel    (wb_result_sel_M),
    .out    (wb_result_M)
  );

  //--------------------------------------------------------------------
  // W stage
  //--------------------------------------------------------------------

  logic [31:0] wb_result_W;

  vc_EnResetReg#(32, 0) wb_result_reg_W
  (
    .clk    (clk),
    .reset  (reset),
    .en     (reg_en_W),
    .d      (wb_result_M),
    .q      (wb_result_W)
  );

  assign proc2mngr_data = wb_result_W;

  assign rf_wdata_W = wb_result_W;

  // stats output
  // note the stats en is full 32-bit here but the outside port is one
  // bit.

  logic [31:0] stats_en_W;

  assign stats_en = | stats_en_W;

  vc_EnResetReg#(32, 0) stats_en_reg_W
  (
   .clk    (clk),
   .reset  (reset),
   .en     (stats_en_wen_W),
   .d      (wb_result_W),
   .q      (stats_en_W)
  );

endmodule

`endif /* LAB2_PROC_PROC_ALT_DPATH_V */

