// Procedural statement forms: kept where they carry structure (assignments),
// skipped where they do not (loops, timing controls, initial blocks).
module sv_statements (
  input  logic       clk, rst,
  input  logic [3:0] a,
  output logic [3:0] q, y,
  output logic [7:0] acc
);
  logic [3:0] i;

  always_ff @(posedge clk) begin
    lbl: begin q <= a; end                       // statement label
    do begin acc <= acc + 8'd1; end while (0);   // do ... while
    for (i = 0; i < 4; i = i + 1) y[i] <= a[i];  // loop body kept, loop skipped
    while (0) acc <= 8'd0;
    repeat (2) acc <= acc + 8'd1;
    acc >>= 1; acc <<<= 1; acc >>>= 1;           // compound shifts
  end

  always begin                                   // no sensitivity list (reported)
    @(posedge clk) q <= a;                       // event control as a statement
    #1 y <= ~a;                                  // delay control
  end

  always @(posedge clk) forever acc <= 8'd0;

  initial begin                                  // skipped as a whole
    if (rst) acc = 8'd0; else acc = 8'd1;
    case (a) 4'd0: acc = 8'd1; default: acc = 8'd2; endcase
    for (i = 0; i < 4; i = i + 1) acc = {4'd0, i};
    while (0) acc = 8'd0;
    repeat (2) acc = acc + 8'd1;
    forever #5 acc = ~acc;
  end

  // bare initial statements (skipped one statement at a time)
  initial if (rst) acc = 8'd0; else acc = 8'd1;
  initial case (a) 4'd0: acc = 8'd1; default: acc = 8'd2; endcase
  initial for (i = 0; i < 4; i = i + 1) acc = {4'd0, i};
  initial repeat (2) acc = acc + 8'd1;
  initial forever #5 acc = ~acc;
endmodule
