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

module jt51_acc(
	input 					clk,
	input					zero,
	input					op31_acc,
	input			[1:0]	rl,
	input	signed	[13:0]	op_out,
	input					ne,
	input	signed	[ 9:0]	noise,
	output  signed	[15:0]	left,
    output  signed	[15:0]	right,
	output  signed	[15:0]	xleft,	// exact outputs
    output  signed	[15:0]	xright    
);

wire [1:0]	rl_out;
wire		zero_out;
reg  [13:0] op_value;

always @(*) begin
	if( ne && op31_acc ) // cambiar a OP 31
		op_value = { noise, 4'd0 };
	else
		op_value = op_out;
end

jt51_sum_op u_left(
	.clk(clk),
	.zero(zero_out),
	.en_ch(rl_out[0]),
	.op_out(op_value),
	.out(xleft)
);

jt51_sum_op u_right(
	.clk(clk),
	.zero(zero_out),
	.en_ch(rl_out[1]),
	.op_out(op_value),
	.out(xright)
);

jt51_sh #( .width(2), .stages(14) ) u_rlsh(
	.clk	( clk		),
	.din	( rl		),
    .drop	( rl_out	)
);

jt51_sh #( .width(1), .stages(22) ) u_zerosh(
	.clk	( clk		),
	.din	( zero		),
    .drop	( zero_out	)
);

wire signed [9:0] left_man, right_man;
wire [2:0] left_exp, right_exp;

jt51_exp2lin left_reconstruct(
	.lin( left	),
	.man( left_man		),
	.exp( left_exp		)
);

jt51_exp2lin right_reconstruct(
	.lin( right	),
	.man( right_man		),
	.exp( right_exp		)
);

jt51_lin2exp left2exp(
  .lin( xleft     ),
  .man( left_man ),
  .exp( left_exp ) );

jt51_lin2exp right2exp(
  .lin( xright     ),
  .man( right_man ),
  .exp( right_exp ) );

`ifdef DUMPLEFT

reg skip;

wire signed [15:0] dump = left;

initial skip=1;

always @(posedge clk)
	if( zero_out && (!skip || dump) ) begin
		$display("%d", dump );
		skip <= 0;
	end

`endif

endmodule
