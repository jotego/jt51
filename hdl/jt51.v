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
	output	reg	 		p1,
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

wire	[1:0]	rl_I;
wire	[2:0]	fb_II;
wire	[2:0]	con_I;
wire	[6:0]	kc_I;
wire	[5:0]	kf_I;
wire	[2:0]	pms_I;
wire	[1:0]	ams_VII;
wire	[2:0]	dt1_II;
wire	[3:0]	mul_VI;
wire	[6:0]	tl_VII;
wire	[1:0]	ks_III;
wire	[4:0]	arate_II;
wire			amsen_VII;
wire	[4:0]	rate1_II;
wire	[1:0]	dt2_I;
wire	[4:0]	rate2_II;
wire	[3:0]	d1l_I;
wire	[3:0]	rrate_II;

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

jt51_pg u_pg(
	.clk		( p1		),				// P1
	.zero		( zero		),
	// Channel frequency
	.kc_I		( kc_I		),
	.kf_I		( kf_I		),
	// Operator multiplying
	.mul_VI		( mul_VI	),
	// Operator detuning
	.dt1_II		( dt1_II	),
	.dt2_I		( dt2_I		),
	// phase modulation from LFO
	.pms_I		( pms_I		),
	.pm			( pm		),
	// phase operation
	.pg_rst_III	( pg_rst_III 	),
	.keycode_III( keycode_III	),
	.pg_phase_X	( ph_X			)
);

`ifdef TEST_SUPPORT
wire		test_eg, test_op0;
`endif
wire [9:0]	eg_XI;

jt51_eg	u_eg(
	`ifdef TEST_SUPPORT
	.test_eg	( test_eg	),
	`endif	
	.rst		( rst_p1	),
	.clk		( p1		),
	.zero		( zero		),
	// envelope configuration
	.keycode_III(keycode_III),	// used in stage III
	.arate_II	( arate_II	),
	.rate1_II	( rate1_II	),
	.rate2_II	( rate2_II	),
	.rrate_II	( rrate_II	),
	.d1l_I		( d1l_I		),
	.ks_III		( ks_III	),
	// envelope operation
	.keyon_II	( keyon_II	),
	.pg_rst_III	( pg_rst_III),
	// envelope number
	.tl_VII		( tl_VII	),
	.am			( am 		),
	.ams_VII	( ams_VII	),
	.amsen_VII	( amsen_VII	),
	.eg_XI		( eg_XI	)
);

wire signed [13:0] op_out;

jt51_op u_op(
	`ifdef TEST_SUPPORT
	.test_eg 		( test_eg			),
	.test_op0		( test_op0			),	
	`endif	
	.clk			( p1				),
	.pg_phase_X		( ph_X				),
	.con_I			( con_I				),
	.fb_II			( fb_II				),
	// volume
	.eg_atten_XI	( eg_XI				),
	// modulation
	.m1_enters		( m1_enters			),
	.c1_enters		( c1_enters			),
	// Operator
	.use_prevprev1	( use_prevprev1		),
	.use_internal_x	( use_internal_x	),
	.use_internal_y	( use_internal_y	),
	.use_prev2		( use_prev2			),
	.use_prev1		( use_prev1			),	
	.test_214		( 1'b0				),
	`ifdef SIMULATION
	.zero			( zero				),
	`endif
	// output data
	.op_XVII		( op_out			)
);

wire	[4:0] nfrq;
wire	[10:0] noise_out;
wire		  ne, op31_acc, op31_no;

jt51_noise u_noise(
	.rst	( rst_p1	),
	.clk	( p1		),
	.nfrq	( nfrq		),	
	.eg		( eg_XI		),
	.out	( noise_out	),
	.op31_no( op31_no	)
);

jt51_acc u_acc(
	.rst		( rst_p1		),
	.clk		( p1			),
	.m1_enters	( m1_enters		),
	.m2_enters	( m2_enters		),
	.c1_enters	( c1_enters		),
	.c2_enters	( c2_enters		),
	.op31_acc	( op31_acc		),
	.rl_I		( rl_I			),
	.con_I		( con_I			),
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
/*verilator tracing_on*/

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
	.test_eg	( test_eg	),
	.test_op0	( test_op0	),
	`endif
	// REG
	.rl_I		( rl_I 		),
	.fb_II		( fb_II 	),
	.con_I		( con_I 	),
	.kc_I		( kc_I 		),
	.kf_I		( kf_I 		),
	.pms_I		( pms_I 	),
	.ams_VII	( ams_VII 	),
	.dt1_II		( dt1_II 	),
	.mul_VI	( mul_VI 	),
	.tl_VII		( tl_VII 	),
	.ks_III		( ks_III 	),
	.arate_II	( arate_II 	),
	.amsen_VII	( amsen_VII ),
	.rate1_II	( rate1_II 	),
	.dt2_I		( dt2_I 	),
	.rate2_II	( rate2_II 	),
	.d1l_I		( d1l_I 	),
	.rrate_II	( rrate_II 	),
	.keyon_II	( keyon_II	),

	.cur_op		( cur_op		),
	.op31_no	( op31_no		),
	.op31_acc	( op31_acc		),
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

