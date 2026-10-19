/* SPDX-License-Identifier: GPL-3.0-or-later */
// Stop the complete DUT, including registers outside cen/cen_p1. The enable
// changes only on the falling input edge, so no pulse is shortened or added.
module jt51_replay_clock(
    input clk, enable,
    output dut_clk
);

`ifdef VERILATOR
reg enable_l = 1'b1;

assign dut_clk = clk & enable_l;

always @(negedge clk) begin
    enable_l <= enable;
end
`else
wire [3:0] clock_inputs;
wire [1:0] clock_select;

assign clock_inputs = {3'd0,clk};
assign clock_select = 2'd0;

altclkctrl #(
    .clock_type        ( "Global Clock" ),
    .ena_register_mode ( "falling edge" ),
    .implement_in_les  ( "OFF"          )
) u_gate (
    .inclk     ( clock_inputs ),
    .clkselect ( clock_select ),
    .ena       ( enable       ),
    .outclk    ( dut_clk      )
);
`endif
endmodule
