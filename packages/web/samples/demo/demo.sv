// VeriLens demo design: a small streaming accelerator.
// The `verilens: top` pragma marks the module the viewer opens first.

// verilens: top
module top (
  input  logic        clk,
  input  logic        rst_n,
  input  logic [31:0] in_data,
  input  logic        in_valid,
  output logic        in_ready,
  output logic [31:0] out_data,
  output logic        out_valid,
  input  logic        out_ready,
  output logic        busy
);
  logic [31:0] fifo_data;
  logic        fifo_valid, fifo_ready;
  logic [31:0] result;
  logic        result_valid, result_ready;
  logic        start, done, mode;
  logic [3:0]  coeff_sel;

  input_fifo #(.DEPTH(4)) input_fifo (
    .clk(clk), .rst_n(rst_n),
    .wr_data(in_data), .wr_valid(in_valid), .wr_ready(in_ready),
    .rd_data(fifo_data), .rd_valid(fifo_valid), .rd_ready(fifo_ready)
  );

  controller controller (
    .clk(clk), .rst_n(rst_n),
    .data_valid(fifo_valid), .done(done), .out_ready(result_ready),
    .start(start), .mode(mode), .coeff_sel(coeff_sel), .busy(busy)
  );

  compute compute (
    .clk(clk), .rst_n(rst_n),
    .data_in(fifo_data), .valid_in(fifo_valid), .ready_out(fifo_ready),
    .start(start), .mode(mode), .coeff_sel(coeff_sel),
    .data_out(result), .valid_out(result_valid), .ready_in(result_ready), .done(done)
  );

  output_fifo #(.DEPTH(2)) output_fifo (
    .clk(clk), .rst_n(rst_n),
    .wr_data(result), .wr_valid(result_valid), .wr_ready(result_ready),
    .rd_data(out_data), .rd_valid(out_valid), .rd_ready(out_ready)
  );
endmodule

module input_fifo #(parameter DEPTH = 4) (
  input  logic        clk,
  input  logic        rst_n,
  input  logic [31:0] wr_data,
  input  logic        wr_valid,
  output logic        wr_ready,
  output logic [31:0] rd_data,
  output logic        rd_valid,
  input  logic        rd_ready
);
  logic [31:0] mem [0:DEPTH-1];
  logic [2:0]  wr_ptr, rd_ptr, count;
  logic        push, pop;

  assign push     = wr_valid & wr_ready;
  assign pop      = rd_valid & rd_ready;
  assign wr_ready = count != DEPTH;
  assign rd_valid = count != 0;
  assign rd_data  = mem[rd_ptr];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      wr_ptr <= 0; rd_ptr <= 0; count <= 0;
    end else begin
      if (push) begin mem[wr_ptr] <= wr_data; wr_ptr <= wr_ptr + 1; end
      if (pop) rd_ptr <= rd_ptr + 1;
      count <= count + push - pop;
    end
  end
endmodule

module output_fifo #(parameter DEPTH = 2) (
  input  logic        clk,
  input  logic        rst_n,
  input  logic [31:0] wr_data,
  input  logic        wr_valid,
  output logic        wr_ready,
  output logic [31:0] rd_data,
  output logic        rd_valid,
  input  logic        rd_ready
);
  logic [31:0] buffer;
  logic        full;

  assign wr_ready = !full | rd_ready;
  assign rd_valid = full;
  assign rd_data  = buffer;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      full <= 1'b0;
    end else begin
      if (wr_valid & wr_ready) begin buffer <= wr_data; full <= 1'b1; end
      else if (rd_ready) full <= 1'b0;
    end
  end
endmodule

module controller (
  input  logic       clk,
  input  logic       rst_n,
  input  logic       data_valid,
  input  logic       done,
  input  logic       out_ready,
  output logic       start,
  output logic       mode,
  output logic [3:0] coeff_sel,
  output logic       busy
);
  localparam IDLE = 2'd0, RUN = 2'd1, DRAIN = 2'd2;
  logic [1:0] state, next_state;

  assign start = (state == IDLE) & data_valid;
  assign busy  = state != IDLE;
  assign mode  = coeff_sel[3];

  always_comb begin
    next_state = state;
    case (state)
      IDLE:  if (data_valid) next_state = RUN;
      RUN:   if (done) next_state = DRAIN;
      DRAIN: if (out_ready) next_state = IDLE;
      default: next_state = IDLE;
    endcase
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state     <= IDLE;
      coeff_sel <= 4'd0;
    end else begin
      state <= next_state;
      if (done) coeff_sel <= coeff_sel + 4'd1;
    end
  end
endmodule

module compute (
  input  logic        clk,
  input  logic        rst_n,
  input  logic [31:0] data_in,
  input  logic        valid_in,
  output logic        ready_out,
  input  logic        start,
  input  logic        mode,
  input  logic [3:0]  coeff_sel,
  output logic [31:0] data_out,
  output logic        valid_out,
  input  logic        ready_in,
  output logic        done
);
  logic [31:0] pe0_out, pe1_out, sum, acc, mux_out;
  logic [15:0] coeff;
  logic        pe0_valid, pe1_valid;

  assign coeff     = {12'd0, coeff_sel};
  assign ready_out = ready_in | ~valid_out;

  pe #(.WIDTH(32)) pe0 (
    .clk(clk), .rst_n(rst_n),
    .a(data_in), .b(coeff), .valid_in(valid_in & start),
    .y(pe0_out), .valid_out(pe0_valid)
  );

  pe #(.WIDTH(32)) pe1 (
    .clk(clk), .rst_n(rst_n),
    .a(pe0_out), .b(acc[15:0]), .valid_in(pe0_valid),
    .y(pe1_out), .valid_out(pe1_valid)
  );

  assign sum     = pe1_out + acc;
  assign mux_out = mode ? sum : pe1_out;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      acc       <= 32'd0;
      data_out  <= 32'd0;
      valid_out <= 1'b0;
      done      <= 1'b0;
    end else begin
      if (pe1_valid) begin
        acc      <= mux_out;
        data_out <= mux_out;
      end
      valid_out <= pe1_valid;
      done      <= pe1_valid & ready_in;
    end
  end
endmodule

module pe #(parameter WIDTH = 32) (
  input  logic             clk,
  input  logic             rst_n,
  input  logic [WIDTH-1:0] a,
  input  logic [15:0]      b,
  input  logic             valid_in,
  output logic [WIDTH-1:0] y,
  output logic             valid_out
);
  logic [WIDTH-1:0] product, shifted;

  assign product = a * b;
  assign shifted = product >> 4;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      y         <= '0;
      valid_out <= 1'b0;
    end else begin
      y         <= shifted;
      valid_out <= valid_in;
    end
  end
endmodule
