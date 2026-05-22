//=========================================================================
// 4-entry Branch Target Buffer (FIFO replacement)
//=========================================================================

`ifndef LAB2_PROC_BRANCH_TARGET_BUFFER
`define LAB2_PROC_BRANCH_TARGET_BUFFER

module lab2_proc_BranchTargetBuffer
(
  input  logic        clk,
  input  logic        reset,

  // current pc
  input  logic [31:0] curr_pc,
  output logic [31:0] branch_target,
  output logic        take_branch,

  // branch result
  input  logic [31:0] branch_pc,
  input  logic [31:0] branch_dest,
  input  logic        branch_taken
);

  // buffer and valid bits
  logic [31:0] tags   [4];
  logic [31:0] buffer [4];
  logic [3:0]  valid;
  logic [3:0]  taken; // whether or not branch should be taken

  // fifo replacement
  logic [1:0] curr;

  // output branch target
  always_comb begin
    branch_target = 32'b0;
    take_branch   = 1'b1;

    case ({2'b11, curr_pc})
      {taken[0], valid[0], tags[0]}: branch_target = buffer[0];
      {taken[1], valid[1], tags[1]}: branch_target = buffer[1];
      {taken[2], valid[2], tags[2]}: branch_target = buffer[2];
      {taken[3], valid[3], tags[3]}: branch_target = buffer[3];
      default: take_branch = 1'b0; // don't take if no match
    endcase
  end

  logic match;
  assign match = ((branch_pc == tags[0]) && valid[0]) |
                 ((branch_pc == tags[1]) && valid[1]) |
                 ((branch_pc == tags[2]) && valid[2]) |
                 ((branch_pc == tags[3]) && valid[3]) ;

  // update buffers
  always_ff @( posedge clk ) begin
    if ( reset ) begin
      valid <= 4'b0;
      taken <= 4'b0;
      curr  <= 2'b0;
    end else if ( match ) // if match, update taken status
      case ({1'b1, branch_pc})
        {valid[0], tags[0]}: taken[0] <= branch_taken;
        {valid[1], tags[1]}: taken[1] <= branch_taken;
        {valid[2], tags[2]}: taken[2] <= branch_taken;
        {valid[3], tags[3]}: taken[3] <= branch_taken;
      endcase
    else if ( branch_taken ) begin // o/w if taken, add to btb
      valid[curr]  <= 1'b1;
      tags[curr]   <= branch_pc;
      taken[curr]  <= 1'b1;
      buffer[curr] <= branch_dest;
      curr         <= curr + 1; // increment ptr
    end
  end

endmodule

`endif
