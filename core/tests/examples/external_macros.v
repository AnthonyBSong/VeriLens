//========================================================================
// Macros from a header that is not on the include path
//========================================================================
// `include is not followed yet (TODO.md), so every macro below is undefined.
// Expected handling: a macro at module level is reported and ignored, a
// macro inside an expression stays as a literal with the macro's name, and
// a procedural block left behind by an undefined wrapper macro is skipped
// as one unit with a warning.

`ifndef EXT_COUNTER_V
`define EXT_COUNTER_V

`include "ext/defs.vh"

module ext_counter
#(
  parameter p_nbits = 8
)(
  input  wire               clk,
  input  wire               reset,
  input  wire [p_nbits-1:0] limit,
  output reg  [p_nbits-1:0] count,
  output wire               over,
  output wire               msb
);

  localparam c_width = $bits( count );

  assign over = count > limit;
  assign msb  = count[c_width-1];

  always @( posedge clk ) begin
    if ( reset )
      count <= `EXT_ZERO;
    else if ( count > limit )
      count <= {p_nbits{1'b0}};
    else
      count <= count + 1'b1;
  end

  //----------------------------------------------------------------------
  // Line tracing: procedural block wrapped by macros from the missing header
  //----------------------------------------------------------------------

  `EXT_TRACE_BEGIN
  begin
    $sformat( str, "%x", count );
    ext_trace.append_str( trace_str, str );
  end
  `EXT_TRACE_END

endmodule

`endif /* EXT_COUNTER_V */
