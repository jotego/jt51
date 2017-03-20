`timescale 1ns / 1ps

module cpu2ym(
	input	rst_fast,
	input	clk_fast,	// faster than CPU clock
	input	clk_per,
	input	cpu_cs,
	input	cpu_wr,
	input	cpu_rd,
	input	cpu_a0,
	input	[7:0]	cpu_data_out,
	output	reg	ym_cs_n,
	output	reg	ym_rd_n,
	output	reg	ym_wr_n,
	output	reg	ym_a0,
	output	reg	DIR,
	output	reg	[7:0] ym_data_out
);

reg	[2:0]	st;
reg	[2:0]	next;
parameter IDLE=0, DROP_CS=1, SET_RDWR=2, WAIT_CS=3, WAIT=4, RISE_CS=5;

always @(posedge clk_per) 
	if( rst_fast ) begin
		st 		<= IDLE;
		ym_a0	<= 1'b0;
	end
	else begin
	case( st )
		IDLE:
			if( !cpu_cs && cpu_wr ) begin
				ym_a0 <= cpu_a0;
				ym_data_out <= cpu_data_out;
				ym_wr_n <= 1'b0;				
				next <= DROP_CS;
				st	 <= WAIT;
			end
			else begin
				ym_cs_n <= 1'b1;
				ym_rd_n <= 1'b1;
				ym_wr_n <= 1'b1;
				DIR		<= 1'b0;
			end
		WAIT: st <= next;
		DROP_CS: begin
			ym_cs_n <= 1'b0;
			next	<= RISE_CS;
			st		<= WAIT;
			end
		RISE_CS: begin
			ym_cs_n <= 1'b1;
			st		<= IDLE;
		end
			/*
		SET_RDWR: begin
			ym_rd_n	<= ~cpu_rd;
			ym_wr_n	<= ~cpu_wr;
			if( cpu_wr )	DIR	<= 1'b1;
			st	<= WAIT_CS;
			end
		WAIT_CS: if( cssh==2'b10 || cssh==2'b00  ) begin
			ym_cs_n <= 1'b1;
			next<= IDLE;
			st	<= WAIT;
			end*/
	endcase

end

endmodule

