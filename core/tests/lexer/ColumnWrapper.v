//========================================================================
// Column Wrapper
//========================================================================
// Connects the control unit, datapath, and memory

`ifndef COLUMN_WRAPPER_V
`define COLUMN_WRAPPER_V

`include "NodeColumnCtrl.v"
`include "NodeColumnDpath.v"
`include "M10KMem.v"

module ColumnWrapper #(
  parameter NUM_ROWS = 30
)(
  input clk,
  input rst,
  input start,

  input signed [17:0] rho,
  input signed [17:0] u_left,
  input signed [17:0] u_right,
  
  // for initializing memory
  input                        mem_n_we_ovrd,
  input [$clog2(NUM_ROWS)-1:0] mem_n_waddr_ovrd,
  input signed [17:0]          mem_n_wdata_ovrd,
  input                        mem_nm1_we_ovrd,
  input [$clog2(NUM_ROWS)-1:0] mem_nm1_waddr_ovrd,
  input signed [17:0]          mem_nm1_wdata_ovrd,

  // to adjacent columns
  output signed [17:0] u_center
);
  // declare wires
  wire mem_n_we;
  wire mem_nm1_we;
  wire [$clog2(NUM_ROWS)-1:0] mem_n_raddr;
  wire [$clog2(NUM_ROWS)-1:0] mem_nm1_raddr;
  wire [$clog2(NUM_ROWS)-1:0] mem_n_waddr;
  wire [$clog2(NUM_ROWS)-1:0] mem_nm1_waddr;

  wire ctrl_at_bottom;
  wire ctrl_at_top;
  wire ctrl_calc_en;

  wire signed [17:0] u_up;
  wire signed [17:0] u_prev;

  wire signed [17:0] u_to_mem_n;
  wire signed [17:0] u_to_mem_nm1;
  wire signed [17:0] u_next;

  // Control Unit
  NodeColumnCtrl #(NUM_ROWS) ctrl_unit (
    .clk            (clk),
    .rst            (rst),
    .start          (start),

    .mem_n_we       (mem_n_we),
    .mem_nm1_we     (mem_nm1_we),
    .mem_n_raddr    (mem_n_raddr),
    .mem_nm1_raddr  (mem_nm1_raddr),
    .mem_n_waddr    (mem_n_waddr),
    .mem_nm1_waddr  (mem_nm1_waddr),

    .ctrl_at_bottom (ctrl_at_bottom),
    .ctrl_at_top    (ctrl_at_top),
    .ctrl_calc_en   (ctrl_calc_en)
  );

  // Datapath
  NodeColumnDpath #(NUM_ROWS) dpath (
    .clk            (clk),
    .rst            (rst),

    .rho            (rho),
    .u_left         (u_left),
    .u_right        (u_right),
    .u_up           (u_up),
    .u_prev         (u_prev),

    .ctrl_at_bottom (ctrl_at_bottom),
    .ctrl_at_top    (ctrl_at_top),
    .ctrl_calc_en   (ctrl_calc_en),

    .u_next         (u_next),
    .u_center       (u_center),
    .u_to_mem_n     (u_to_mem_n),
    .u_to_mem_nm1   (u_to_mem_nm1)
  );

  wire                        mem_n_we_muxed;
  wire [$clog2(NUM_ROWS)-1:0] mem_n_waddr_muxed;
  wire signed [17:0]          mem_n_wdata_muxed;

  assign mem_n_we_muxed = mem_n_we_ovrd | mem_n_we;
  assign mem_n_waddr_muxed = mem_n_we_ovrd ? mem_n_waddr_ovrd : mem_n_waddr;
  assign mem_n_wdata_muxed = mem_n_we_ovrd ? mem_n_wdata_ovrd : u_to_mem_n;

  // Memory u_n
  M10KMem #(NUM_ROWS) mem_n (
    .clk            (clk),
    .we             (mem_n_we_muxed),
    .write_address  (mem_n_waddr_muxed),
    .read_address   (mem_n_raddr),
    .d              (mem_n_wdata_muxed),
    .q              (u_up)
  );

  wire                        mem_nm1_we_muxed;
  wire [$clog2(NUM_ROWS)-1:0] mem_nm1_waddr_muxed;
  wire signed [17:0]          mem_nm1_wdata_muxed;

  assign mem_nm1_we_muxed = mem_nm1_we_ovrd | mem_nm1_we;
  assign mem_nm1_waddr_muxed = mem_nm1_we_ovrd ? mem_nm1_waddr_ovrd : mem_nm1_waddr;
  assign mem_nm1_wdata_muxed = mem_nm1_we_ovrd ? mem_nm1_wdata_ovrd : u_to_mem_nm1;

  // Memory u_{n-1}
  M10KMem #(NUM_ROWS) mem_nm1 (
    .clk            (clk),
    .we             (mem_nm1_we_muxed),
    .write_address  (mem_nm1_waddr_muxed),
    .read_address   (mem_nm1_raddr),
    .d              (mem_nm1_wdata_muxed),
    .q              (u_prev)
  );

endmodule

`endif /* COLUMN_WRAPPER_V */