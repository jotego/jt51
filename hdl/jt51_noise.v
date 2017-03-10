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

	tab size 4
	
	See xapp052.pdf from Xilinx
	
	The NFRQ formula in the App. Note does not make sense:
	Output rate is 55kHz but for NFRQ=1 the formula states that
	the noise is 111kHz, twice the output rate per channel.
	
	That would suggest that the noise for LEFT and RIGHT are
	different but the rest of the system suggest that LEFT and
	RIGHT outputs are calculated at the same time, based on the
	same OP output.
	
	Also, the block diagram states a 1 bit serial input from
	EG to NOISE and that seems unnecessary too.
	
	I have not been able to measure noise in actual chip because
	operator 31 does not produce any output on my two chips.

*/

module jt51_noise(
	input	rst,
	input	clk,
	input	zero,
	input	ne,
	input	[4:0] nfrq,
	input	[9:0]	eg,
	output	[9:0]	out,
	output			op31_acc
);

reg [9:0] limit;
wire zero_out;

always @(posedge clk) 
	if( ne ) begin
		if( zero_out )
			casex ( ~eg )
				10'b1xxxxxxxxx:	limit <= 10'h3FF;
				10'b01xxxxxxxx:	limit <= 10'h1FF;
				10'b001xxxxxxx:	limit <= 10'h0FF;
				10'b0001xxxxxx:	limit <= 10'h07F;
				10'b00001xxxxx:	limit <= 10'h03F;
				10'b000001xxxx:	limit <= 10'h01F;
				10'b0000001xxx:	limit <= 10'h00F;
				10'b00000001xx:	limit <= 10'h007;
				10'b000000001x:	limit <= 10'h003;
				10'b0000000001:	limit <= 10'h001;		
			endcase
	end
	else limit <= 10'd0;


reg 		base;
reg [4:0]	cnt;

always @(posedge clk)
	if( rst ) begin
		base <= 1'b0;
		cnt  <= 5'b1;
	end
	else begin
		if( zero_out ) begin
			if ( cnt==nfrq && nfrq!=5'd0 ) begin
				base <= ~base;
				cnt  <= 5'b1;
			end
			else cnt <= cnt + 1'b1;
		end
	end

wire [9:0] pre;

assign	out = pre & limit;

genvar aux;
generate
for( aux=0; aux<10; aux=aux+1) begin : noise_lfsr
	jt51_noise_lfsr #(.init(aux*29+97*aux*aux*aux)) u_lfsr (
		.rst	( rst ),
		.clk	( clk ),
		.base	( base ),
		.out	( pre[aux] )
	);
end
endgenerate

// shift ZERO to make it match the output of OP31 from EG
jt51_sh #( .width(1), .stages(5) ) u_zerosh(
	.clk	( clk		),
	.din	( zero		),
    .drop	( zero_out	)
);

jt51_sh #( .width(1), .stages(7) ) u_op31sh(
	.clk	( clk		),
	.din	( zero_out	),
    .drop	( op31_acc	)
);

endmodule
