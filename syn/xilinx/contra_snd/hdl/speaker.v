`timescale 1ns / 1ps

module speaker(
	input			clk100,
	input	[15:0]	left_in,
	input	[15:0]	right_in,
	output			left_out,
	output			right_out
);

sd2_dac sd_jt( 
	.clk		( clk100	),
	.ldatasum	( left_in 	), 
	.rdatasum	( right_in	), 
	.left		( left_out	),
	.right		( right_out	)
);		

endmodule
