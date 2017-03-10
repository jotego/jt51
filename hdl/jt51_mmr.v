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

module jt51_mmr(
	input		  	rst,
	input		  	clk,		// P1
	input	[7:0]	d_in,
	input			write,
	input			a0,
	output	reg		busy,
	
	// CT
	output	reg		ct1,
	output	reg		ct2,
	
	// Noise
	output	reg			ne,
	output	reg [4:0]	nfrq,

	// LFO
	output	reg	[7:0]	lfo_freq,
	output	reg	[1:0]	lfo_w,
	output	reg [6:0]	lfo_amd,	
	output	reg [6:0]	lfo_pmd,
	output	reg			lfo_rst,
	// Timers
	output	reg	[9:0]	value_A,
	output	reg	[7:0]	value_B,
	output	reg			load_A,
	output	reg			load_B,
	output	reg	 		enable_irq_A,
	output	reg	 		enable_irq_B,
	output	reg			clr_flag_A,
	output	reg			clr_flag_B,
	output	reg			clr_run_A,
	output	reg			clr_run_B,
	output	reg			set_run_A,
	output	reg			set_run_B,
	input				flag_A,

	`ifdef TEST_SUPPORT		
	// Test
	output	reg		test_eg,
	output	reg		test_op0,
	`endif
	// REG
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
	output			kon_out,
	output			koff_out,

	output	[1:0]	cur_op,

	output			zero	
);

reg [7:0] selected_register;

reg		up_clr;
reg		up_rl,	up_kc,	up_kf,	up_pms,
		up_dt1,	up_tl,	up_ks,	up_dt2,
		up_d1l,	up_kon,	up_amsen;

wire			busy_reg;		

parameter 	REG_TEST	=	8'h01,
			REG_TEST2	=	8'h02,
			REG_KON		=	8'h08,
			REG_NOISE	=	8'h0f,
			REG_CLKA1	=	8'h10,
			REG_CLKA2	=	8'h11,
			REG_CLKB	=	8'h12,
			REG_TIMER	=	8'h14,
			REG_LFRQ	=	8'h18,
			REG_PMDAMD	=	8'h19,
			REG_CTW		=	8'h1b;

reg	csm;

always @(posedge clk) begin : memory_mapped_registers
	if( rst ) begin
		selected_register 	<= 8'h0;
		busy				<= 1'b0;
		{ up_rl, up_kc, up_kf, up_pms, up_dt1, up_tl,
				up_ks, up_amsen, up_dt2, up_d1l, up_kon } <= 11'd0;		
		`ifdef TEST_SUPPORT
		{ test_eg, test_op0 } <= 2'd0;
		`endif
		// timers
		{ value_A, value_B } <= 18'd0;
		{ clr_flag_B, clr_flag_A, 
		enable_irq_B, enable_irq_A, load_B, load_A } <= 6'd0;
		{ clr_run_A, clr_run_B, set_run_A, set_run_B } <= 4'b1100;
		up_clr <= 1'b0;
		// LFO
		{ lfo_amd, lfo_pmd }	<= 14'h0;
		lfo_freq		<= 8'd0;
		lfo_w			<= 2'd0;
		lfo_rst			<= 1'b0;
		{ ct2, ct1 }	<= 2'd0;
		csm				<= 1'b0;
	end else begin
		// WRITE IN REGISTERS
		if( write && !busy ) begin
			busy <= 1'b1;
			if( !a0 )
				selected_register <= d_in;
			else begin
				// Global registers
				if( selected_register < 8'h20 ) begin
					case( selected_register)
					// registros especiales
					REG_TEST:	lfo_rst <= 1'b1; // regardless of d_in
					`ifdef TEST_SUPPORT
					REG_TEST2:	{ test_op0, test_eg } <= d_in[1:0];
					`endif
					REG_KON: 	up_kon 		<= 1'b1;
					REG_NOISE:	{ ne, nfrq } <= { d_in[7], d_in[4:0] };
					REG_CLKA1:	value_A[9:2]<= d_in;
					REG_CLKA2:	value_A[1:0]<= d_in[1:0];
					REG_CLKB:	value_B		<= d_in;
					REG_TIMER: begin
						csm	<= d_in[7];
						{ clr_flag_B, clr_flag_A, 
						  enable_irq_B, enable_irq_A,
						  load_B, load_A } <= d_in[5:0];
						  clr_run_A <= ~d_in[0];
						  set_run_A <=  d_in[0];
						  clr_run_B <= ~d_in[1];
						  set_run_B <=  d_in[1];
						end
					REG_LFRQ:	lfo_freq <= d_in;
					REG_PMDAMD: begin
						if( !d_in[7] ) 
							lfo_amd <= d_in[6:0];
						else
							lfo_pmd <= d_in[6:0];						
						end
					REG_CTW: begin
						{ ct2, ct1 } <= d_in[7:6];
						lfo_w 		 <= d_in[1:0];
						end
					endcase
				end else
				// channel registers
				if( selected_register < 8'h40 ) begin
					case( selected_register[4:3] )
						2'h0: up_rl	<= 1'b1;
						2'h1: up_kc	<= 1'b1;
						2'h2: up_kf	<= 1'b1;
						2'h3: up_pms<= 1'b1;
					endcase
				end
				else 
				// operator registers
				begin
					case( selected_register[7:5] )
						3'h2: up_dt1 	<= 1'b1;
						3'h3: up_tl		<= 1'b1;
						3'h4: up_ks		<= 1'b1;
						3'h5: up_amsen	<= 1'b1;
						3'h6: up_dt2 	<= 1'b1;
						3'h7: up_d1l 	<= 1'b1;
					endcase
				end
			end
		end
		else begin /* clear once-only bits */
			csm 	<= 1'b0;
			lfo_rst <= 1'b0;
			{ clr_flag_B, clr_flag_A, load_B, load_A } <= 4'd0;
			{ clr_run_A, clr_run_B, set_run_A, set_run_B } <= 4'd0;
			
			if( |{ up_rl, up_kc, up_kf, up_pms, up_dt1, up_tl,
				up_ks, up_amsen, up_dt2, up_d1l, up_kon } == 1'b0 )
				busy	<= busy_reg;
			else
				busy	<= 1'b1;
				
			if( busy_reg ) begin
				up_clr <= 1'b1;
			end
			else begin
				up_clr <= 1'b0;
				if( up_clr	)
				{ up_rl, up_kc, up_kf, up_pms, up_dt1, up_tl,
					up_ks, up_amsen, up_dt2, up_d1l, up_kon } <= 11'd0;
			end
		end
	end
end

jt51_reg u_reg(
	.rst	( rst		),
	.clk	( clk		),				// P1
	.d_in	( d_in		),

	.up_rl( up_rl ),
	.up_kc( up_kc ),
	.up_kf( up_kf ),
	.up_pms( up_pms ),
	.up_dt1( up_dt1 ),
	.up_tl( up_tl ),
	.up_ks( up_ks ),
	.up_amsen( up_amsen ),
	.up_dt2( up_dt2 ),
	.up_d1l( up_d1l ),
	.up_kon( up_kon ),
	.op( selected_register[4:3] ),		// operator to update
	.ch( selected_register[2:0] ),		// channel to update
	
	.csm	( csm		),
	.flag_A	( flag_A	),

	.busy( 	busy_reg ),
	.rl_out( rl_out ),
	.fb_out( fb_out ),
	.con_out( con_out ),
	.kc_out( kc_out ),
	.kf_out( kf_out ),
	.pms_out( pms_out ),
	.ams_out( ams_out ),
	.dt1_out( dt1_out ),
	.mul_out( mul_out ),
	.tl_out( tl_out ),
	.ks_out( ks_out ),
	.ar_out( ar_out ),
	.amsen_out( amsen_out ),
	.d1r_out( d1r_out ),
	.dt2_out( dt2_out ),
	.d2r_out( d2r_out ),
	.d1l_out( d1l_out ),
	.rr_out( rr_out ),
	.kon_out(kon_out),
	.koff_out(koff_out),

	.cur_op(cur_op),
	.zero(zero)
);

endmodule
