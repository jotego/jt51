`timescale 1ns / 1ps

module clocks(
    input	rst,
    input	clk50,
	input	divide_more,
    output	clk_cpu,
    output	clk_dac,
	output	reg locked
    );
	
	wire 	GND_BIT = 1'b0;
	wire	clk_base; // 4*3.58MHz
	wire	CLKDV_BUF;
	
	wire	locked0, locked1;
	
	always @(posedge clk_cpu) locked <= locked0 & locked1;
	
	reg		[1:0] clk_cpu_cnt;
	
	always @( posedge CLKDV_BUF or posedge rst) 
		if( rst )
			clk_cpu_cnt <= 2'b0;
		else
			clk_cpu_cnt <= clk_cpu_cnt + 1'b1;
	
	reg clk_sel;
	always @( negedge CLKDV_BUF )
		clk_sel <= divide_more;

	BUFG  CLKDV_BUFG_INST(
		.I( clk_sel ? clk_cpu_cnt[0] : CLKDV_BUF ), 
		.O(clk_cpu)
	);

	BUFG  CLK2X_BUFG_INST (.I(clkbase_2x), 
                         .O( clkbase_fbin ));
						 
	DCM_SP #( .CLK_FEEDBACK("2X"), .CLKDV_DIVIDE(2.0), .CLKFX_DIVIDE(7), 
         .CLKFX_MULTIPLY(2), .CLKIN_DIVIDE_BY_2("FALSE"), 
         .CLKIN_PERIOD(20.000), .CLKOUT_PHASE_SHIFT("NONE"), 
         .DESKEW_ADJUST("SYSTEM_SYNCHRONOUS"), .DFS_FREQUENCY_MODE("LOW"), 
         .DLL_FREQUENCY_MODE("LOW"), .DUTY_CYCLE_CORRECTION("TRUE"), 
         .FACTORY_JF(16'hC080), .PHASE_SHIFT(0), .STARTUP_WAIT("FALSE") )  
         u_clkbase ( .CLKFB(clkbase_fbin), 
                       .CLKIN(clk50), 	//	*
                       .DSSEN(GND_BIT), 
                       .PSCLK(GND_BIT), 
                       .PSEN(GND_BIT), 
                       .PSINCDEC(GND_BIT), 
                       .RST(GND_BIT), 
                       .CLKDV(), 
                       .CLKFX(clk_base), 	// *
                       .CLKFX180(), 
                       .CLK0(), 
                       .CLK2X(clkbase_2x), 
                       .CLK2X180(), 
                       .CLK90(), 
                       .CLK180(), 
                       .CLK270(), 
                       .LOCKED(locked0), 
                       .PSDONE(), 
                       .STATUS());   

	wire clkdiv0, clkdiv0_buf;
	BUFG  u_clkdiv_buf(
		.I(clkdiv0), 
		.O(clkdiv0_buf)
	);
					   
   DCM_SP #( .CLK_FEEDBACK("1X"), .CLKDV_DIVIDE(4.0), .CLKFX_DIVIDE(1), 
         .CLKFX_MULTIPLY(4), .CLKIN_DIVIDE_BY_2("FALSE"), 
         .CLKIN_PERIOD(69.832), .CLKOUT_PHASE_SHIFT("NONE"), 
         .DESKEW_ADJUST("SYSTEM_SYNCHRONOUS"), .DFS_FREQUENCY_MODE("LOW"), 
         .DLL_FREQUENCY_MODE("LOW"), .DUTY_CYCLE_CORRECTION("TRUE"), 
         .FACTORY_JF(16'hC080), .PHASE_SHIFT(0), .STARTUP_WAIT("FALSE") ) 
         u_clkdiv ( .CLKFB(clkdiv0_buf), //*
                       .CLKIN(clk_base), //*
                       .DSSEN(GND_BIT), 
                       .PSCLK(GND_BIT), 
                       .PSEN(GND_BIT), 
                       .PSINCDEC(GND_BIT), 
                       .RST(GND_BIT), 
                       .CLKDV(CLKDV_BUF), //*
                       .CLKFX(), 
                       .CLKFX180(), 
                       .CLK0(clkdiv0), 
                       .CLK2X(), 
                       .CLK2X180(), 
                       .CLK90(), 
                       .CLK180(), 
                       .CLK270(), 
                       .LOCKED(locked1), 
                       .PSDONE(), 
                       .STATUS());					   

	wire clk_dac_pre;
	BUFG  CLKFX_BUFG_INST (.I(clk_dac_pre), 
                         .O(clk_dac));

						   
	DCM_SP #( .CLK_FEEDBACK("NONE"), .CLKDV_DIVIDE(2.0), .CLKFX_DIVIDE(1), 
         .CLKFX_MULTIPLY(2), .CLKIN_DIVIDE_BY_2("FALSE"), 
         .CLKIN_PERIOD(20.000), .CLKOUT_PHASE_SHIFT("NONE"), 
         .DESKEW_ADJUST("SYSTEM_SYNCHRONOUS"), .DFS_FREQUENCY_MODE("LOW"), 
         .DLL_FREQUENCY_MODE("LOW"), .DUTY_CYCLE_CORRECTION("TRUE"), 
         .FACTORY_JF(16'hC080), .PHASE_SHIFT(0), .STARTUP_WAIT("FALSE") ) 
         u_clkdac ( .CLKFB(GND_BIT), 
                       .CLKIN(clk50), 
                       .DSSEN(GND_BIT), 
                       .PSCLK(GND_BIT), 
                       .PSEN(GND_BIT), 
                       .PSINCDEC(GND_BIT), 
                       .RST(GND_BIT), 
                       .CLKDV(), 
                       .CLKFX(clk_dac_pre), 
                       .CLKFX180(), 
                       .CLK0(), 
                       .CLK2X(), 
                       .CLK2X180(), 
                       .CLK90(), 
                       .CLK180(), 
                       .CLK270(), 
                       .LOCKED(), 
                       .PSDONE(), 
                       .STATUS());					   
endmodule
