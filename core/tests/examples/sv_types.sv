// SystemVerilog data types, declarations and attributes (TODO.md group 2)
package sv_types_pkg;
  typedef logic [7:0] byte_t;
  localparam int N = 2;
endpackage

module sv_types (
  input  var logic        clk,
  (* mark_debug *) input logic [3:0][7:0] packed2d,   // 32 bits
  input  sv_types_pkg::byte_t pk_in,
  input  bit              b_in,
  output logic [7:0]      y,
  output wire             w_out
);
  typedef logic [3:0] nib_t;
  typedef enum logic [1:0] {IDLE, RUN, DONE} state_t;
  typedef struct packed { logic valid; logic [7:0] data; } pkt_t;

  int       count;
  byte      small;
  shortint  s16;
  longint   l64;
  integer   n32;
  bit [3:0] nib;
  nib_t     nib2, nib_arr [2];
  state_t   state;
  pkt_t     pkt;
  enum logic {A, B} e;
  struct packed { logic q; } sq;
  trireg    tr;
  uwire     uw;
  logic     init_var = 1'b0;          // variable initializer: reported, not a driver
  wire      w_init = b_in;            // net initializer: an assign
  (* keep = "true" *) logic kept;
  logic [sv_types_pkg::N-1:0] scoped;

  (* dont_touch *) assign kept = packed2d[0][1];
  assign y = pkt.data ^ pk_in;
  assign w_out = w_init | kept;
  assign tr = b_in;
  assign uw = b_in;
  assign scoped = {sv_types_pkg::N{b_in}};

  always_ff @(posedge clk) begin : upd
    count <= count + 1;
    small <= 8'd1; s16 <= 0; l64 <= 0; n32 <= 0;
    nib <= nib2; nib2 <= nib_arr[0]; nib_arr[1] <= nib;
    state <= RUN; pkt.valid <= b_in; pkt.data <= packed2d[1];
    e <= A; sq.q <= init_var;
  end : upd
endmodule
