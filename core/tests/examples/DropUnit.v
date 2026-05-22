//========================================================================
// Verilog Components: Drop Unit
//========================================================================
// Drop unit allows dropping a packet when the drop signal is high. This
// is useful especially in pipelined processor, when a squash should drop
// a late arriving memory response.

`ifndef LAB2_PROC_DROP_UNIT_V
`define LAB2_PROC_DROP_UNIT_V

module lab2_proc_DropUnit
#(
  parameter p_msg_nbits = 1
)
(
  input  logic                   clk,
  input  logic                   reset,

  // the drop signal will drop the next arriving packet

  input  logic                   drop,

  input  logic [p_msg_nbits-1:0] istream_msg,
  input  logic                   istream_val,
  output logic                   istream_rdy,

  output logic [p_msg_nbits-1:0] ostream_msg,
  output logic                   ostream_val,
  input  logic                   ostream_rdy
);

  localparam c_state_pass = 1'b0;
  localparam c_state_drop = 1'b1;

  logic state;
  logic next_state;
  logic istream_go;

  assign istream_go = istream_rdy && istream_val;

  // assign output message same as input message

  assign ostream_msg = istream_msg;

  // next state

  always_comb begin
    if ( state == c_state_pass ) begin

      // we only go to drop state if there is a drop request and we cannot
      // drop it right away (!istream_en)
      if ( drop && !istream_go )
        next_state = c_state_drop;
      else
        next_state = c_state_pass;

    end else begin

      // if we are in the drop mode and a message arrives, we can go back
      // to pass state
      if ( istream_go )
        next_state = c_state_pass;
      else
        next_state = c_state_drop;

    end
  end

  // state outputs

  always_comb begin
    if ( state == c_state_pass ) begin

      // we combinationally take care of dropping if the packet is already
      // available
      ostream_val = istream_val && !drop;
      istream_rdy  = ostream_rdy;

    end else begin

      // we just drop the packet
      ostream_val = 1'b0;
      istream_rdy  = 1'b1;

    end
  end

  // state transitions

  always_ff @( posedge clk ) begin

    if ( reset )
      state <= c_state_pass;
    else
      state <= next_state;

  end

endmodule

`endif /* LAB2_PROC_DROP_UNIT_V */

