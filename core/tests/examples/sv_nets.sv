// Net and variable declaration forms, non-ANSI ports, unknown widths.
module sv_nets (a, b, c, cc);
  input a;
  input b;
  output c;
  output cc;
  input d;                          // declared in the body only: still a port
  input nets_pkg::word_t pw;        // package-scoped port type
  input word_t tw;                  // typedef'd port type

  tri     t0;                       // every net type keyword
  tri0    t1;
  tri1    t2;
  wand    wa;
  wor     wo;
  supply0 gnd;
  supply1 vcc;

  nets_pkg::word_t w1;              // package-scoped declaration
  word_t signed    w2;              // user type followed by a qualifier
  int cnt = 0;                      // variable initializer (reported, not a driver)

  parameter N = 4;
  parameter nets_pkg::word_t PW = 8'd1;   // typed parameters
  parameter word_t PT = 8'd2;
  wire [N-1:0][7:0] md;             // multi-dimensional with a parametric dimension
  wire [a ? 3 : 1 : 0] cw;          // ranges that do not fold keep their text
  wire [md[0] : 0]     bw;
  wire [md[1:0] : 0]   pw2;
  wire [-(1) + 2 : 0]  neg;

  assign c  = a & b;
  assign w1 = md[0].x;              // member of an array element
  assign cw = f2(a, b);             // user function with several arguments
  assign bw = $clog2(8, 2);         // system function with several arguments
  wire [7:0] pat  = '{default: 0};  // assignment patterns with keys
  wire [7:0] pat2 = '{hi: 4'd1, lo: 4'd2};

  unknown_mod u_arr [1:0] (.p(a));  // array of an undefined module
  known_sub #(.W(-1), .X(a ? 1 : 2)) u1 (.*);   // wildcard connection
endmodule

module known_sub #(parameter W = 1, X = 2) (input a, input b, output cc);
  assign cc = a | b;
endmodule
