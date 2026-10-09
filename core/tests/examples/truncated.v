module cut (input a, output b);
  assign b = a;
  always @(posedge a) begin
    b <= 