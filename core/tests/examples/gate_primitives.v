// Gate primitive instantiations — exercises the parser's gate_primitive branch.
module gate_demo(input a, input b, output y1, output y2, output y3);
  wire t1, t2;
  and  g1 (t1, a, b);
  or   g2 (t2, a, b);
  nand g3 (y1, t1, t2), g4 (y2, t1, t2);
  not      (y3, a);
endmodule
