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

module jt51(
	input				clk,	// main clock
	input				rst,	// reset
	input				cs_n,	// chip select
	input				wr_n,	// write
	input				a0,
	input		[7:0]	d_in, // data in
	output 		[7:0]	d_out, // data out
	output 				ct1,
	output 				ct2,
	output 				irq_n,	// I do not synchronize this signal
	output	reg 		p1,
	// Low resolution output (same as real chip)
	output				sample,	// marks new output sample
	output	signed	[15:0] left,
	output	signed	[15:0] right,
	// Full resolution output
	output	signed	[15:0] xleft,
	output	signed	[15:0] xright,
	// unsigned outputs for sigma delta converters, full resolution
	output	[15:0] dacleft,
	output	[15:0] dacright
);

reg rst_p1, rst_p1_aux;

assign dacleft  = { ~xleft [15],  xleft[14:0] };
assign dacright = { ~xright[15], xright[14:0] };


// Generate internal clock and synchronous reset for it.
always @(posedge clk or posedge rst) 
	if( rst ) 
		p1 		<= 1'b0;
	else 
		p1		<= ~p1;

always @(posedge p1 or posedge rst) 
	if( rst ) begin
		rst_p1_aux	<= 1'b1;
		rst_p1		<= 1'b1;
	end
	else begin
		rst_p1_aux	<= 1'b0;
		rst_p1		<= rst_p1_aux;
	end

// Timers
wire [9:0] 	value_A;
wire [7:0] 	value_B;
wire		load_A, load_B;
wire 		enable_irq_A, enable_irq_B;
wire		clr_flag_A, clr_flag_B;
wire		flag_A, flag_B, overflow_A;
wire		set_run_A, set_run_B;
wire		clr_run_A, clr_run_B;

jt51_timers timers( 
	.clk		( clk			),
	.rst   		( rst_p1		),
	.value_A	( value_A		),
	.value_B	( value_B		),
	.load_A		( load_A		),
	.load_B		( load_B		),
	.enable_irq_A( enable_irq_A ),
	.enable_irq_B( enable_irq_B ),
	.clr_flag_A	( clr_flag_A	),
	.clr_flag_B	( clr_flag_B	),
	.set_run_A	( set_run_A		),
	.set_run_B	( set_run_B		),
	.clr_run_A	( clr_run_A		),
	.clr_run_B	( clr_run_B		),	
	.flag_A		( flag_A		),
	.flag_B		( flag_B		),
	.overflow_A	( overflow_A	),
	.irq_n		( irq_n			)
);

`define YM_TIMER_CTRL 8'h14

wire	[1:0]	rl_out;
wire	[2:0]	fb_II;
wire	[2:0]	con_out;
wire	[6:0]	kc_out;
wire	[5:0]	kf_out;
wire	[2:0]	pms_out;
wire	[1:0]	ams_out;
wire	[2:0]	dt1_out;
wire	[3:0]	mul_out;
wire	[6:0]	tl_out;
wire	[1:0]	ks_out;
wire	[4:0]	ar_out;
wire			amsen_out;
wire	[4:0]	d1r_out;
wire	[1:0]	dt2_out;
wire	[4:0]	d2r_out;
wire	[3:0]	d1l_out;
wire	[3:0]	rr_out;

wire	[1:0]	cur_op;
wire			zero;
assign	sample =zero;
wire 			keyon_II;

wire	[7:0]	lfo_freq;
wire	[1:0]	lfo_w;
wire			lfo_rst;
wire	[6:0]	am;
wire	[7:0]	pm;
wire	[6:0]	amd, pmd;

wire m1_enters, m2_enters, c1_enters, c2_enters;
wire use_prevprev1,use_internal_x,use_internal_y, use_prev2,use_prev1;

jt51_lfo u_lfo(
	.rst		( rst_p1	),
	.clk		( clk		),
	.zero		( zero		),
	.lfo_rst	( lfo_rst 	),
	.lfo_freq	( lfo_freq	),
	.lfo_w		( lfo_w		),
	.lfo_amd	( amd		),
	.lfo_pmd	( pmd		),
	.am			( am		),
	.pm_u		( pm		)
);

wire	[ 4:0]	keycode_III;
wire	[ 9:0]	ph_X;
wire			pg_rst_III;

jt51_phasegen u_pg(
	.clk		( p1		),				// P1
	// Channel frequency
	.kc			( kc_out	),
	.kf			( kf_out	),
	// Operator multiplying
	.mul		( mul_out	),
	// Operator detuning
	.dt1		( dt1_out	),
	.dt2		( dt2_out	),
	// phase modulation from LFO
	.pms		( pms_out	),
	.pm			( pm		),
	// phase operation
	.pg_rst_III	( pg_rst_III 	),
	.keycode_III( keycode_III	),
	.ph_X		( ph_X		)
);

`ifdef TEST_SUPPORT
wire		test_eg, test_op0;
`endif
wire [9:0]	eg_XI;

jt51_envelope u_eg(
	`ifdef TEST_SUPPORT
	.test_eg	( test_eg	),
	`endif	
	.rst		( rst_p1	),
	.clk		( p1		),
	.zero		( zero		),
	// envelope configuration
	.keycode_III(keycode_III),	// used in stage III
	.arate		( ar_out	),
	.rate1		( d1r_out	),
	.rate2		( d2r_out	),
	.rrate		( rr_out	),
	.d1l		( d1l_out	),
	.ks			( ks_out	),
	// envelope operation
	.keyon_II	( keyon_II	),
	.pg_rst_III	( pg_rst_III	),
	// envelope number
	.tl			( tl_out	),
	.am			( am 		),
	.ams		( ams_out	),
	.amsen		( amsen_out	),
	.eg_XI		( eg_XI	)
);

wire signed [13:0] op_out;

jt51_op u_op(
	`ifdef TEST_SUPPORT
	.test_eg 	( test_eg	),
	.test_op0	( test_op0	),	
	`endif	
	.clk		( p1		),
	.pg_phase_X	( ph_X		),
	.con_I		( con_out	),
	.fb_II		( fb_II		),
	// volume
	.eg_atten_XI( eg_XI		),
	// modulation
	.m1_enters	( m1_enters		),
	.c1_enters	( c1_enters		),
	// Operator
	.use_prevprev1	( use_prevprev1		),
	.use_internal_x	( use_internal_x	),
	.use_internal_y	( use_internal_y	),
	.use_prev2		( use_prev2			),
	.use_prev1		( use_prev1			),	
	.test_214		( 1'b0				),
	// .zero			( zero				),
	// output data
	.op_XVII		( op_out			)
);

wire	[4:0] nfrq;
wire	[9:0] noise_out;
wire		  ne, op31_acc;

jt51_noise u_noise(
	.rst	( rst_p1	),
	.clk	( p1		),
	.zero	( zero		),
	.ne		( ne		),
	.nfrq	( nfrq		),
	.eg		( eg_XI		),
	.out	( noise_out	),
	.op31_acc(op31_acc	)
);

jt51_acc u_acc(
	.rst		( rst_p1		),
	.clk		( p1			),
	.m1_enters	( m1_enters		),
	.m2_enters	( m2_enters		),
	.c1_enters	( c1_enters		),
	.c2_enters	( c2_enters		),
	.op31_acc	( op31_acc		),
	.rl			( rl_out		),
	.con_I		( con_out		),
	.op_out 	( op_out		),
	.ne			( ne			),
	.noise		( noise_out		),
	.left		( left			),
	.right		( right			),
	.xleft		( xleft			),
	.xright		( xright		)
);

reg		busy;
wire	busy_mmr;
reg	[1:0] busy_mmr_sh;

reg		flag_B_s, flag_A_s;
assign 	d_out = { busy, 5'h0, flag_B_s, flag_A_s };

always @(posedge clk ) 
	{ flag_B_s, flag_A_s } <= { flag_B, flag_A };


wire		write = !cs_n && !wr_n;

reg	[7:0]	d_in_copy;
reg			a0_copy;
reg			write_copy;

always @(posedge clk) begin : cpu_interface
	if( rst ) begin
		busy		<= 1'b0;
		a0_copy		<= 1'b0;
		d_in_copy	<= 8'd0;
		write_copy	<= 1'b0;
	end
	else begin
		busy_mmr_sh <= { busy_mmr_sh[0], busy_mmr };
		if( write && !busy ) begin
			busy 		<= 1'b1;
			write_copy	<= 1'b1;
			a0_copy		<= a0;
			d_in_copy	<= d_in;
		end
		else begin
			if( busy_mmr ) write_copy	<= 1'b0;
			if( busy && busy_mmr_sh==2'b10 ) busy <= 1'b0;
		end
	end
end

reg			write_s, a0_s;
reg	[7:0]	d_in_s;

always @(posedge p1 )
	{ write_s, a0_s, d_in_s } <= { write_copy, a0_copy, d_in_copy };


jt51_mmr u_mmr(
	.clk		( p1			),
	.rst		( rst_p1		),
	.a0			( a0_s			),
	.write		( write_s		),
	.d_in		( d_in_s		),
	.busy		( busy_mmr		),

	// CT
	.ct1		( ct1			),
	.ct2		( ct2			),
	// LFO
	.lfo_freq	( lfo_freq		),
	.lfo_w		( lfo_w			),
	.lfo_amd	( amd			),
	.lfo_pmd	( pmd			),
	.lfo_rst	( lfo_rst 		),
	
	// Noise
	.ne			( ne			),
	.nfrq		( nfrq			),
	
	// Timers
	.value_A	( value_A		),
	.value_B	( value_B		),
	.load_A		( load_A		),
	.load_B		( load_B		),
	.enable_irq_A( enable_irq_A ),
	.enable_irq_B( enable_irq_B ),
	.clr_flag_A	( clr_flag_A	),
	.clr_flag_B	( clr_flag_B	),	
	.clr_run_A	( clr_run_A		),
	.clr_run_B	( clr_run_B		),	
	.set_run_A	( set_run_A		),
	.set_run_B	( set_run_B		),	
	.overflow_A	( overflow_A	),
	`ifdef TEST_SUPPORT	
	// Test
	.test_eg	( test_eg		),
	.test_op0	( test_op0		),
	`endif
	// REG
	.rl_out		( rl_out 		),
	.fb_II		( fb_II 		),
	.con_out	( con_out 		),
	.kc_out		( kc_out 		),
	.kf_out		( kf_out 		),
	.pms_out	( pms_out 		),
	.ams_out	( ams_out 		),
	.dt1_out	( dt1_out 		),
	.mul_out	( mul_out 		),
	.tl_out		( tl_out 		),
	.ks_out		( ks_out 		),
	.ar_out		( ar_out 		),
	.amsen_out	( amsen_out 	),
	.d1r_out	( d1r_out 		),
	.dt2_out	( dt2_out 		),
	.d2r_out	( d2r_out 		),
	.d1l_out	( d1l_out 		),
	.rr_out		( rr_out 		),
	.keyon_II	( keyon_II		),

	.cur_op		( cur_op		),
	.zero		( zero			),
	.m1_enters	( m1_enters		),
	.m2_enters	( m2_enters		),
	.c1_enters	( c1_enters		),
	.c2_enters	( c2_enters		),
	// Operator
	.use_prevprev1	( use_prevprev1		),
	.use_internal_x	( use_internal_x	),
	.use_internal_y	( use_internal_y	),
	.use_prev2		( use_prev2			),
	.use_prev1		( use_prev1			)
);

endmodule

