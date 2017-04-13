`timescale 1ns / 1ps

module clocks(
    input	rst,
    input	clk50,
	input	divide_more,
	input	clk_dac_sel,
    output	clk_cpu,
	output	locked,
    output	clk_dac,
	output	reg rst_clk4,
	output	E,
	output	Q
    );
	
	wire clk_base;	// 4*3.58MHz
	reg [1:0] clk4_cnt;
	reg	Qr, Er;

	reg clk_dac_sel2, clk_dac_aux;

	always @(negedge clk_base)
		clk_dac_sel2 <= clk_dac_sel;

	reg dac_cnt;
	
	always @(posedge clk_base or posedge rst)
		if( rst )
			dac_cnt <= 1'b0;
		else begin
			if( clk_dac_sel2 )
				clk_dac_aux <= ~clk_dac_aux;
			else	
				{ clk_dac_aux, dac_cnt } <= { clk_dac_aux, dac_cnt } + 1'b1;			
		end


	// BUFG dacbuf( .I(clk_dac_aux), .O(clk_dac) );
	// assign clk_dac = clk_base;
	assign clk_dac = clk4_cnt[0];

	always @(posedge clk_base or posedge rst) 
		if (rst) begin
			clk4_cnt <= 0;
			{ Er, Qr } <= 2'b0;
		end
		else begin
			clk4_cnt <= clk4_cnt+2'b01;
			case( clk4_cnt )
				2'b00: Qr <= 1;  // RISING EDGE OF E
				2'b01: Er <= 1;  // RISING EDGE OF Q
				2'b10: Qr <= 0;  // FALLING EDGE OF E
				2'b11: Er <= 0;  // FALLING EDGE OF Q
			endcase
		end
	
	BUFG ebuf( .I(Er), .O(E) );
	BUFG qbuf( .I(Qr), .O(Q) );
	
	reg rst_clk4_aux;
	
	always @(negedge Q)
		{ rst_clk4, rst_clk4_aux } <= { rst_clk4_aux, rst };
	
	reg		[1:0] clk_cpu_cnt;
	
	always @( posedge clk_base or posedge rst) 
		if( rst )
			clk_cpu_cnt <= 2'b0;
		else
			clk_cpu_cnt <= clk_cpu_cnt + 1'b1;
	
	reg clk_sel;
	always @( negedge clk_base )
		clk_sel <= divide_more;

	BUFG  CLKDV_BUFG_INST(
		.I( clk_sel ? clk_cpu_cnt[0] : clk_base ), 
		.O(clk_cpu)
	);
	// clk_base = clk50*2/7 = 14.28MHz						 
	clk50div u_clk50div (
		.CLKIN_IN(clk50), 
		.RST_IN(rst), 
		.CLKFX_OUT(clk_base), 
		//.CLKIN_IBUFG_OUT(CLKIN_IBUFG_OUT), 
		//.CLK0_OUT(CLK0_OUT), 
		.LOCKED_OUT(locked)
		);
endmodule
