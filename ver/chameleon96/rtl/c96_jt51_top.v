module c96_jt51_top(input [3:0] pcm_pins);

wire clk, hps_clk, hps_reset_n, rst;
wire [31:0] gp_out, gp_in;
reg [3:0] reset_pipe = 4'hf;
reg replay_div = 1'b0;

assign rst = reset_pipe[3];
assign clk = replay_div;

always @(posedge hps_clk) begin
    replay_div <= !replay_div;
end

always @(posedge clk) begin
    reset_pipe <= {reset_pipe[2:0],!hps_reset_n};
end

cyclonev_hps_interface_clocks_resets u_clocks(
    .h2f_user0_clk       ( hps_clk     ),
    .h2f_rst_n           ( hps_reset_n ),
    .f2h_warm_rst_req_n  ( 1'b1        ),
    .f2h_cold_rst_req_n  ( 1'b1        ),
    .f2h_dbg_rst_req_n   ( 1'b1        ),
    .f2h_pending_rst_ack ( 1'b1        )
);

cyclonev_hps_interface_mpu_general_purpose u_gp(
    .gp_out ( gp_out ),
    .gp_in  ( gp_in  )
);

c96_jt51_control u_control(
    .clk    ( clk    ),
    .rst    ( rst    ),
    .gp_out ( gp_out ),
    .gp_in  ( gp_in  )
);

endmodule
