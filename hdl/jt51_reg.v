/*  This file is part of JT51.

    JT51 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    JT51 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with JT51.  If not, see <http://www.gnu.org/licenses/>.
	
	Author: Jose Tejada Gomez. Twitter: @topapate
	Version: 1.0
	Date: 27-10-2016
	*/

`timescale 1ns / 1ps

module jt51_reg(
	input		  	rst,
	input		  	clk,		// P1
	input	[7:0]	d_in,

	input			up_rl,
	input			up_kc,
	input			up_kf,
	input			up_pms,
	input			up_dt1,
	input			up_tl,
	input			up_ks,
	input			up_amsen,
	input			up_dt2,
	input			up_d1l,
	input			up_keyon,
	input	[1:0]	op,		// operator to update
	input	[2:0]	ch,		// channel to update
	
	input			csm,
	input			overflow_A,

	output			busy,
	output	[1:0]	rl_out,
	output	[2:0]	fb_out,
	output	[2:0]	con_out,
	output	[6:0]	kc_out,
	output	[5:0]	kf_out,
	output	[2:0]	pms_out,
	output	[1:0]	ams_out,
	output	[2:0]	dt1_out,
	output	[3:0]	mul_out,
	output	[6:0]	tl_out,
	output	[1:0]	ks_out,
	output	[4:0]	ar_out,
	output			amsen_out,
	output	[4:0]	d1r_out,
	output	[1:0]	dt2_out,
	output	[4:0]	d2r_out,
	output	[3:0]	d1l_out,
	output	[3:0]	rr_out,
	output			keyon_II,

	output	[1:0]	cur_op,

	output	reg		zero
);

reg		kon, koff;
reg [1:0] csm_state;
reg	[4:0] csm_cnt;

wire csm_kon  = csm_state[0];
wire csm_koff = csm_state[1];

wire	[1:0]	rl_in	= d_in[7:6];
wire	[2:0]	fb_in	= d_in[5:3];
wire	[2:0]	con_in	= d_in[2:0];
wire	[6:0]	kc_in	= d_in[6:0];
wire	[5:0]	kf_in	= d_in[7:2];
wire	[2:0]	pms_in	= d_in[6:4];
wire	[1:0]	ams_in	= d_in[1:0];
wire	[2:0]	dt1_in	= d_in[6:4];
wire	[3:0]	mul_in	= d_in[3:0];
wire	[6:0]	tl_in	= d_in[6:0];
wire	[1:0]	ks_in	= d_in[7:6];
wire	[4:0]	ar_in	= d_in[4:0];
wire			amsen_in= d_in[7];
wire	[4:0]	d1r_in	= d_in[4:0];
wire	[1:0]	dt2_in	= d_in[7:6];
wire	[4:0]	d2r_in	= d_in[4:0];
wire	[3:0]	d1l_in	= d_in[7:4];
wire	[3:0]	rr_in	= d_in[3:0];

wire up = 	up_rl | up_kc | up_kf | up_pms | up_dt1 | up_tl |
			up_ks | up_amsen | up_dt2 | up_d1l | up_keyon;

reg	[4:0]	cnt, next, cur;
reg			last, last_kon;
reg	[1:0]	cnt_kon;
reg			busy_op;

assign busy = busy_op;

assign cur_op = cur[4:3];

always @(*) begin
	next = cur +1'b1;
end

wire	[4:0] abs	= { op, ch };
wire	update_op	= abs == cur;
wire	update_ch	= ch  == cur[2:0];

wire up_rl_ch	= up_rl		& update_ch;
wire up_kc_ch	= up_kc		& update_ch;
wire up_kf_ch	= up_kf		& update_ch;
wire up_pms_ch	= up_pms	& update_ch;
wire up_dt1_op	= up_dt1	& update_op;
wire up_tl_op	= up_tl		& update_op;
wire up_ks_op	= up_ks		& update_op;
wire up_amsen_op= up_amsen	& update_op;
wire up_dt2_op	= up_dt2	& update_op;
wire up_d1l_op	= up_d1l	& update_op;

reg  up_keyon_long;

always @(posedge clk) begin : up_counter
	if( rst ) begin
		cnt		<= 5'h0;
		cur		<= 5'h0;
		last	<= 1'b0;
		zero	<= 1'b0;
        busy_op	<= 1'b0;
        up_keyon_long <= 1'b0;
	end
	else begin
		cur		<= next;
		zero 	<= next== 5'd0;
		last	<= up;
		if( up && !last ) begin
			cnt		<= cur;
			busy_op	<= 1'b1;
			up_keyon_long <= up_keyon;
		end
		else if( cnt == cur ) begin
				busy_op <= 1'b0;
				up_keyon_long <= 1'b0;
			end
	end
end

wire [2:0]  cur_ch =  cur[2:0];
wire [3:0] keyon_op = d_in[6:3];
wire [2:0] keyon_ch = d_in[2:0];

jt51_kon i_jt51_kon (
	.rst       (rst       ),
	.clk       (clk       ),
	.keyon_op  (keyon_op  ),
	.keyon_ch  (keyon_ch  ),
	.cur_op    (cur_op    ),
	.cur_ch    (cur_ch    ),
	.up_keyon  (up_keyon_long	  ),
	.csm       (csm       ),
	.overflow_A(overflow_A),
	.keyon_II  (keyon_II  )
);


// memory for OP registers

reg  [41:0] reg_op[31:0];
reg  [41:0] reg_out;

assign { dt1_out, mul_out, tl_out, ks_out, ar_out, amsen_out, d1r_out, 
	dt2_out, d2r_out, d1l_out, rr_out } = reg_out;

wire [41:0] reg_in = { 	
					up_dt1_op	? { dt1_in, mul_in}		: { dt1_out, mul_out },
					up_tl_op	? tl_in					: tl_out,
                    up_ks_op	? { ks_in, ar_in }		: { ks_out, ar_out },
                    up_amsen_op	? { amsen_in, d1r_in }	: { amsen_out, d1r_out },
                    up_dt2_op	? { dt2_in, d2r_in }	: { dt2_out, d2r_out },
                    up_d1l_op	? { d1l_in, rr_in }		: { d1l_out, rr_out } };

wire opdata_wr = |{ up_dt1_op, up_tl_op, up_ks_op, up_amsen_op, up_dt2_op, up_d1l_op };

always @(posedge clk) begin
	reg_out		<= reg_op[next];
    if( opdata_wr )
    	reg_op[cur]	<= reg_in;
end

// memory for CH registers

reg [25:0] reg_ch[7:0];
reg [25:0] reg_ch_out;
wire [25:0] reg_ch_in = {
		up_rl_ch	? { rl_in, fb_in, con_in }	: { rl_out, fb_out, con_out },
        up_kc_ch	? kc_in						: kc_out,
        up_kf_ch	? kf_in						: kf_out,
        up_pms_ch	? { pms_in, ams_in }		: { pms_out, ams_out } };
        
assign { rl_out, fb_out, con_out, kc_out, kf_out, pms_out, ams_out } = reg_ch_out;

wire [2:0] next_ch = next[2:0];
wire chdata_wr = |{up_rl_ch, up_kc_ch, up_kf_ch, up_pms_ch };

always @(posedge clk) begin
	reg_ch_out		<= reg_ch[next_ch];
    if( chdata_wr )
    	reg_ch[cur_ch]	<= reg_ch_in;
end

endmodule
