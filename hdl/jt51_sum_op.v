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

module jt51_sum_op(
	input 		clk,
	input		zero,
	input		en_ch,
	input	signed	[13:0] op_out,
	output reg signed	[15:0] out
);

reg	signed [18:0]	sum;

wire signed [18:0] op_signed = { {5{op_out[13]}}, op_out};

always @(posedge clk) 
	if( zero ) begin
		sum <= en_ch ? op_signed : 19'd0;
		if( sum[18:16]==3'd0 || sum[18:16]==3'b111 )
			out <= sum[15:0];
		else
			out<={ sum[18], {15{~sum[18]}}};
	end
	else
		if( en_ch ) sum <= sum + op_signed;


endmodule
