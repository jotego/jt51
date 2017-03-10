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

/*

	Pipeline operator

*/

module jt51_op(
	`ifdef TEST_SUPPORT
	input				test_eg,
	input				test_op0,
	`endif
	input             	clk,          	// P1
	input		[19:0] 	phase_cnt,
	input		[2:0]	con,
	input		[1:0]	cur_op,
	input		[2:0]	fb,
	// volume
	input		[9:0]	eg,
	// output data
	output reg	signed	[13:0]	out
);


reg	[ 9:0]	phase;
reg [ 4:0]	log_msb, log_msb2;
reg [12:0]	pre; // preattenuation value
reg [3:0]	sign;
reg	[12:0]	out_abs;

reg 	[7:0]	phase_addr;
wire 	[11:0]	log_val;	// sine mantisa, in 2's complement

jt51_sintable u_sintable(
	.phase   ( phase_addr	),
	.log_val ( log_val   	)
);


reg 	[7:0]	pow_addr;
wire 	[12:0]	pow_val;

jt51_exptable u_exptable(
	.pow_addr( pow_addr	),
	.pow_val ( pow_val 	)
);

reg	[9:0]	eg_II, eg_III;
`ifdef TEST_SUPPORT
reg	[9:0]	eg_IV, eg_V, eg_VI;
`endif
reg	signed	[19:0]	modulation;
wire	[2:0]	con_I, con_VII;
wire	[1:0]	cur_op_VII, cur_op_I;
wire	[2:0]	fb_I;

parameter mod_lat = 5; /* latency */
parameter mod_stg = 5*8-mod_lat; /* stages */
reg	[14*mod_stg-1:0] mod;

wire signed [13:0] mod1 = mod[ (16-mod_lat)*14-1: (15-mod_lat)*14 ];
wire signed [13:0] mod2 = mod[ (24-mod_lat)*14-1: (23-mod_lat)*14 ];
wire signed [13:0] mod3 = mod[ (32-mod_lat)*14-1: (31-mod_lat)*14 ];
wire signed [13:0] mod4 = mod[ (40-mod_lat)*14-1: (39-mod_lat)*14 ];
wire signed [13:0] mod7;

wire mod7_en = cur_op_I==2'd0;

jt51_sh2 #( .width(14), .stages(8) ) u_mod7sh(
	.clk	( clk	),
	.en		( mod7_en ),
	.ld		( 1'b1	),
	.din	( mod3	),
    .drop	( mod7	)
);


parameter M1=2'd0, M2=2'd1, C1=02'd2, C2=2'd3;

always @(*) begin
	case( cur_op_I )
		default: // M1, FL
			case( fb_I )
				3'd0: modulation <= 20'd0;
				3'd1: modulation <= (mod3+mod7)<<1;
				3'd2: modulation <= (mod3+mod7)<<2;
				3'd3: modulation <= (mod3+mod7)<<3;
				3'd4: modulation <= (mod3+mod7)<<4;
				3'd5: modulation <= (mod3+mod7)<<5;
				3'd6: modulation <= (mod3+mod7)<<6;
				3'd7: modulation <= (mod3+mod7)<<7;
			endcase
		C1: case(con_I)
				3'd7, 3'd2, 3'd1:
					modulation <= 20'd0;
				default:
					modulation <= mod1<<9; // M1
			endcase
		C2: case(con_I)
				default: // 3'd4, 3'd1, 3'd0:
					modulation <= mod1<<9; // M2
				3'd2:
					modulation <= (mod1+mod2)<<9; // M2+M1
				3'd3:
					modulation <= (mod1+mod4)<<9; // M2+C1
				3'd5:
					modulation <= mod2<<9; // M1
				3'd7, 3'd6:
					modulation <= 20'd0;
			endcase
		M2: case(con_I)
				default: // 3'd2, 3'd0:
					modulation <= mod2<<9; // C1
				3'd1:
					modulation <= (mod2+mod4)<<9; // C1+M1
				3'd5:
					modulation <= mod4<<9; // M1
				3'd7, 3'd6, 3'd4, 3'd3:
					modulation <= 20'd0;
			endcase
	endcase
end


always @(posedge clk) begin
	// I
	phase <= (phase_cnt + modulation)>>10;
	eg_II <= eg;
	// II
	phase_addr	<= phase[8]? ~phase[7:0]:phase[7:0];
	sign[0]		<= phase[9];
	eg_III <= eg_II;
	// III
	{ log_msb, pow_addr } <= log_val[11:0] + { eg_III, 2'b0};
	sign[1]	<= sign[0];
	`ifdef TEST_SUPPORT
	eg_IV <= eg_III;
	`endif
	// IV
	pre		<= pow_val;
	log_msb2<= log_msb;
	sign[2]	<= sign[1];
	`ifdef TEST_SUPPORT
	eg_V <= eg_IV;
	`endif
	// V
	case( log_msb2 )
		5'h0: out_abs <= pre;
		5'h1: out_abs <= pre >> 1;
		5'h2: out_abs <= pre >> 2;
		5'h3: out_abs <= pre >> 3;
		5'h4: out_abs <= pre >> 4;
		5'h5: out_abs <= pre >> 5;
		5'h6: out_abs <= pre >> 6;
		5'h7: out_abs <= pre >> 7;
		5'h8: out_abs <= pre >> 8;
		5'h9: out_abs <= pre >> 9;
		5'hA: out_abs <= pre >> 10;
		5'hB: out_abs <= pre >> 11;
		5'hC: out_abs <= pre >> 12;
		default: out_abs <= 13'd0;
	endcase
	sign[3]	<= sign[2];
	`ifdef TEST_SUPPORT
	eg_VI <= eg_V;
	`endif
	// VI
    mod[14*mod_stg-1:14] <= mod[14*(mod_stg-1)-1:0];
	`ifdef TEST_SUPPORT
	if( test_eg)
		mod[14-1:0]	<= eg_VI;
	else
	`endif
		mod[14-1:0]	<= sign[3] ? ~{1'b0,out_abs}+1'b1 : {1'b0,out_abs} ;
	// VII
	`ifdef TEST_SUPPORT
	if( test_op0 ) begin
		if( cur_op_VII==3'd0)
			out <= mod[14-1:0];
		else
			out <= 14'd0;
	end
	else			
	`endif
	case( con_VII )
		3'd0, 3'd1, 3'd2, 3'd3:
			if( cur_op_VII!=2'd3 )
				out <= 14'd0;
			else
				out <= mod[14-1:0];
		3'd4:
			if( cur_op_VII==2'd0 || cur_op_VII==2'd1 )
				out <= 14'd0;
			else
				out <= mod[14-1:0];
		3'd5, 3'd6:
			if( cur_op_VII==2'd0 )
				out <= 14'd0;
			else
				out <= mod[14-1:0];
		3'd7:	out <= mod[14-1:0];
	endcase
end

jt51_sh #( .width(8), .stages(7) ) u_con1sh(
	.clk	( clk	),
	.din	( { con, cur_op, fb } 	),
    .drop	( { con_I, cur_op_I, fb_I } )
);


jt51_sh #( .width(5), .stages(6) ) u_con7sh(
	.clk	( clk	),
	.din	( { con_I, cur_op_I }	),
    .drop	( { con_VII, cur_op_VII } )
);


endmodule
