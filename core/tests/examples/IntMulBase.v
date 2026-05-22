//========================================================================
// Integer Multiplier Fixed-Latency Implementation
//========================================================================

`ifndef LAB1_IMUL_INT_MUL_BASE_V
`define LAB1_IMUL_INT_MUL_BASE_V

`include "vc/trace.v"
`include "vc/arithmetic.v"
`include "vc/regs.v"
`include "vc/muxes.v"
`include "vc/counters.v"

module lab1_imul_IntMulBaseDpath
(
  input  logic        clk,
  input  logic        reset,

  // data signals
  input  logic [63:0] istream_msg,
  output logic [31:0] ostream_msg,

  // control signals
  input  logic        a_mux_sel,
  input  logic        b_mux_sel,
  input  logic        result_mux_sel,
  input  logic        add_mux_sel,
  input  logic        result_en,

  // status signals
  output logic         b_lsb
);

  // split operands
  logic [31:0] istream_msg_a;
  assign istream_msg_a = istream_msg[63:32];

  logic [31:0] istream_msg_b;
  assign istream_msg_b = istream_msg[31:0];

  // a mux
  logic [31:0] a_shift_out;
  logic [31:0] a_mux_out;

  vc_Mux2#(32) a_mux
  (
    .sel (a_mux_sel),
    .in0 (a_shift_out),
    .in1 (istream_msg_a),
    .out (a_mux_out)
  );

  // a register
  logic [31:0] a_reg_out;

  vc_ResetReg#(32) a_reg
  (
    .clk   (clk),
    .reset (reset),
    .d     (a_mux_out),
    .q     (a_reg_out)
  );

  // a shifter

  vc_LeftLogicalShifter#(32, 1) a_shift
  (
    .in    (a_reg_out),
    .shamt (1'b1),
    .out   (a_shift_out)
  );

  // b mux
  logic [31:0] b_shift_out;
  logic [31:0] b_mux_out;

  vc_Mux2#(32) b_mux
  (
    .sel (b_mux_sel),
    .in0 (b_shift_out),
    .in1 (istream_msg_b),
    .out (b_mux_out)
  );

  // b register
  logic [31:0] b_reg_out;

  vc_ResetReg#(32) b_reg
  (
    .clk   (clk),
    .reset (reset),
    .d     (b_mux_out),
    .q     (b_reg_out)
  );

  // drive status signal
  assign b_lsb = b_reg_out[0];

  // b shifter

  vc_RightLogicalShifter#(32, 1) b_shift
  (
    .in    (b_reg_out),
    .shamt (1'b1),
    .out   (b_shift_out)
  );

  // result mux
  logic [31:0] add_mux_out;
  logic [31:0] result_mux_out;

  vc_Mux2#(32) result_mux
  (
    .sel (result_mux_sel),
    .in0 (add_mux_out),
    .in1 (32'b0),
    .out (result_mux_out)
  );

  // result reg
  logic [31:0] result_reg_out;

  vc_EnReg#(32) result_reg
  (
    .clk   (clk),
    .reset (reset),
    .en    (result_en),
    .d     (result_mux_out),
    .q     (result_reg_out)
  );

  // adder
  logic [31:0] adder_out;

  vc_SimpleAdder#(32) adder
  (
    .in0 (a_reg_out),
    .in1 (result_reg_out),
    .out (adder_out)
  );

  // adder mux

  vc_Mux2#(32) adder_mux
  (
    .sel (add_mux_sel),
    .in0 (adder_out),
    .in1 (result_reg_out),
    .out (add_mux_out)
  );

  // output port
  assign ostream_msg = result_reg_out;

endmodule

module lab1_imul_IntMulBaseCtrl
(
  input  logic        clk,
  input  logic        reset,

  // dataflow signals
  input  logic        istream_val,
  output logic        istream_rdy,
  output logic        ostream_val,
  input  logic        ostream_rdy,

  // control signals
  output logic        a_mux_sel,
  output logic        b_mux_sel,
  output logic        result_mux_sel,
  output logic        add_mux_sel,
  output logic        result_en,

  // data signals
  input logic         b_lsb
);

  // state definitions
  typedef enum logic [1:0] {
    STATE_IDLE, 
    STATE_CALC,
    STATE_DONE
  } state_t;

  // state

  state_t state_reg;
  state_t state_next;

  always_ff @( posedge clk ) begin
    if ( reset ) begin
      state_reg <= STATE_IDLE;
    end
    else begin
      state_reg <= state_next;
    end
  end

  // state transitions

  logic req_go;
  logic resp_go;
  logic calc_done;

  assign req_go  = istream_val && istream_rdy;
  assign resp_go = ostream_val && ostream_rdy;
  // calc_done calculated with counter...

  always_comb begin
    state_next = state_reg;

    case ( state_reg )
      STATE_IDLE: if ( req_go    ) state_next = STATE_CALC;
      STATE_CALC: if ( calc_done ) state_next = STATE_DONE;
      STATE_DONE: if ( resp_go   ) state_next = STATE_IDLE;
      default: state_next = STATE_IDLE;
    endcase
  end

  // counter
  logic [4:0] count;
  logic _count_is_zero; // dummy

  vc_BasicCounter#(5, 0, 31) counter
  (
    .clk (clk),
    .reset (reset),
    .clear (req_go),
    .increment (1'b1),
    .decrement (1'b0),
    .count (count),
    .count_is_zero (_count_is_zero),
    .count_is_max (calc_done)
  );

  // state outputs

  function void cs
  (
    input logic       cs_istream_rdy,
    input logic       cs_ostream_val,
    input logic       cs_a_mux_sel,
    input logic       cs_b_mux_sel,
    input logic       cs_result_mux_sel,
    input logic       cs_add_mux_sel,
    input logic       cs_result_en,
  );
  begin
    istream_rdy    = cs_istream_rdy;
    ostream_val    = cs_ostream_val;

    a_mux_sel      = cs_a_mux_sel;
    b_mux_sel      = cs_b_mux_sel;
    result_mux_sel = cs_result_mux_sel;
    add_mux_sel    = cs_add_mux_sel;
    result_en      = cs_result_en;
  end
  endfunction

  logic do_add;
  assign do_add = b_lsb;

  always_comb begin
    cs( 0, 0, 0, 0, 0, 0, 0);

    case ( state_reg )
      //                           istream ostream a_mux b_mux result  add_mux result
      //                           rdy     val     sel   sel   mux_sel sel     en
      STATE_IDLE:               cs( 1,     0,      1,    1,    1,      0,      1 );
      STATE_CALC: if ( do_add ) cs( 0,     0,      0,    0,    0,      0,      1 );
                  else          cs( 0,     0,      0,    0,    0,      1,      1 );
      STATE_DONE:               cs( 0,     1,      0,    0,    1,      0,      0 );
      default: cs('x, 'x, 'x, 'x, 'x, 'x, 'x);
    endcase
  end

endmodule

//========================================================================
// Integer Multiplier Fixed-Latency Implementation
//========================================================================

module lab1_imul_IntMulBase
(
  input  logic        clk,
  input  logic        reset,

  input  logic        istream_val,
  output logic        istream_rdy,
  input  logic [63:0] istream_msg,

  output logic        ostream_val,
  input  logic        ostream_rdy,
  output logic [31:0] ostream_msg
);

  // control Signals

  logic a_mux_sel;
  logic b_mux_sel;
  logic result_mux_sel;
  logic add_mux_sel;
  logic result_en;

  // data Signals

  logic b_lsb;

  lab1_imul_IntMulBaseCtrl ctrl
  (
    .*
  );

  lab1_imul_IntMulBaseDpath dpath
  (
    .*
  );

  //----------------------------------------------------------------------
  // Line Tracing
  //----------------------------------------------------------------------

  `ifndef SYNTHESIS

  logic [`VC_TRACE_NBITS-1:0] str;
  `VC_TRACE_BEGIN
  begin

    $sformat( str, "%x:%x", istream_msg[63:32], istream_msg[31:0] );
    vc_trace.append_val_rdy_str( trace_str, istream_val, istream_rdy, str );

    vc_trace.append_str( trace_str, "(" );

    $sformat( str, "%x", dpath.a_reg_out );
    vc_trace.append_str( trace_str, str );
    vc_trace.append_str( trace_str, " " );

    $sformat( str, "%x", dpath.b_reg_out );
    vc_trace.append_str( trace_str, str );
    vc_trace.append_str( trace_str, " " );

    case ( ctrl.state_reg )

      ctrl.STATE_IDLE:
        vc_trace.append_str( trace_str, "I " );

      ctrl.STATE_CALC:
      begin
        if ( ctrl.do_add )
          vc_trace.append_str( trace_str, "C+" );
        else
          vc_trace.append_str( trace_str, "C " );
      end

      ctrl.STATE_DONE:
        vc_trace.append_str( trace_str, "D " );

      default:
        vc_trace.append_str( trace_str, "? " );

    endcase

    vc_trace.append_str( trace_str, ")" );

    $sformat( str, "%x", ostream_msg );
    vc_trace.append_val_rdy_str( trace_str, ostream_val, ostream_rdy, str );

  end
  `VC_TRACE_END

  `endif /* SYNTHESIS */

endmodule

`endif /* LAB1_IMUL_INT_MUL_BASE_V */

