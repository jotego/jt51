`timescale 1ns / 1ps
`timescale 1ns / 1ps

module ymplayer(
	input	clk50,
	input	rst,
	// sound
	output	reg speaker_left,
	output	reg speaker_right,
	// switches
	input [3:0]	sw_sel,	// 
	input		send_data, // 1=Send music over UART, 0=stop
	// UART wires
	input		uart_rx,
	output		uart_tx,
	output [7:0] led
);

wire locked, rst_clk, rst_clk50;

// Send data
reg	[9:0] send_data_shr;
wire send_data_s = &send_data_shr;

always @(posedge clk50)
	send_data_shr <= { send_data_shr[8:0], send_data };
	
wire E, Q, clk_dac;

clocks u_clocks(
    .rst	( rst		),
    .clk50	( clk50		),
	.locked	( locked	),
	.divide_more( send_data_s ),
    //.clk_cpu( clk		),
	.clk_dac( clk_dac	),
	.clk_dac_sel( sw_sel[1] ),
	.E		( E 	),
	.Q		( Q		)
);

wire cpu_rw;
wire AVMA;
//wire BS, BA, BUSY, LIC;

wire clk_per = E; // mc6809i 
wire cpu_rst_req;

rst_sync u_rst1(
	.rst_in	( rst|(~locked)|cpu_rst_req	),
	.clk	( Q				),
	.rst_out( rst_clk		)
);
/*
rst_sync u_rst2(
	.rst_in	( rst_clk	),
	.clk	( clk_dac	),
	.rst_out( rst_fast	)
);
*/
rst_sync u_rst50(
	.rst_in	( rst|(~locked)	),
	.clk	( clk50			),
	.rst_out( rst_clk50		)
);

wire [7:0] cpu_data_in, cpu_data_out;
wire [15:0] cpu_addr;
wire [7:0]jt_data_out;


wire jt_sample;

// JT51
`ifndef NOJT
wire signed	[15:0] direct_l, direct_r;

jt51 u_jt51(
	.clk	( clk_per		),
	.rst	( rst_clk		),
	.cs_n	( jt_cs_n		),	// chip select
	.wr_n	( cpu_rw		),	// write
	.a0		( cpu_addr[0]	),
	.d_in	( cpu_data_out	), // data in
	.d_out	( jt_data_out	), // data out
	.irq_n	( jt_irq_n		),
	// uso salidas exactas para el DAC
	.sample	( jt_sample		),
	.xleft	( direct_l		),
	.xright	( direct_r		)
);

wire [15:0] inter_l, inter_r;

jt51_interpol i_jt51_interpol (
	.clk        (clk50	    ),
	.rst        (rst        ),
	.sample_in  (jt_sample  ),
	.left_in    (direct_l	),
	.right_in   (direct_r	),
	.left_other (16'd0 		),
	.right_other(16'd0		),
	.out_l		(inter_l	),
	.out_r		(inter_r	),
	.sample_out (fir_sample )
);

reg [15:0] dacin_l, dacin_r;

always @(posedge clk_dac)
	if( sw_sel[2] ) begin
		dacin_l <= inter_l;
		dacin_r <= inter_r;
	end
	else begin 
		dacin_l <= direct_l;
		dacin_r <= direct_r;
	end

wire dac2_l, dac2_r;
wire dacmist_l, dacmist_r; 

speaker u_speaker(
	.clk100		( clk_dac	), 
	.left_in	( dacin_l	),
	.right_in	( dacin_r	),
	.left_out	( dacmist_l	),
	.right_out	( dacmist_r	)
);

always @(posedge clk_per)
	if( sw_sel[3] ) begin 
		speaker_left <= dac2_l;
		speaker_right<= dac2_r;
	end
	else begin 
		speaker_left<= dacmist_l;
		speaker_right<=dacmist_r;
	end


jt51_dac2 i_jt51_dac2_l (.clk(clk_dac), .rst(rst), .din(dacin_l), .dout(dac2_l));
jt51_dac2 i_jt51_dac2_r (.clk(clk_dac), .rst(rst), .din(dacin_r), .dout(dac2_r));


`else
wire jt_irq_n = 1'b1;
wire [15:0] dacin_l = 16'd0, dacin_r=16'd0;
`endif

parameter RAM_MSB = 10; // 10 for Contra;

wire [7:0] ROM_data_out, RAM_data;

wire		fsm_wr;
wire [ 7:0]	fsm_data;
wire [14:0]	fsm_addr;
wire		rom_prog;

//synthesis attribute box_type ram32 "black_box"
ram32 ROM( // 32kb
	.clka	( clk_per		),
	.dina	( fsm_data		),
	.wea	( fsm_wr		),
	.douta	( ROM_data_out	),
	.addra	( rom_prog ? fsm_addr : cpu_addr[14:0])
);

//synthesis attribute box_type ram2 "black_box"
ram2 RAM( // 2kb
	.clka	( clk_per		),
	.dina	( cpu_data_out	),
	.douta	( RAM_data		),
	.addra	( cpu_addr[RAM_MSB:0] ),
	.ena	( RAM_cs		),
	.wea	( ~cpu_rw		)
);
		
wire [7:0] sound_latch;
wire clear_irq;

assign led = rom_prog ? fsm_addr[14:7] : sound_latch;


fsm_control fsm_ctrl(
	.clk		( clk50		),
	.clk_cpu	( E			),
	.rst		( rst_clk50	),
	// Sound
	.sound_latch(sound_latch),
	.jt_left	( dacin_l	),
	.jt_right	( dacin_r	),     
	.jt_sample	( jt_sample	),
	.irq		( irq		),
	.clear_irq	( clear_irq	),
	// Programming
	.cpu_rst	( cpu_rst_req), 
	.rom_prog	( rom_prog	),
    .rom_wr		( fsm_wr	), 
    .rom_addr	( fsm_addr	), 
    .rom_data	( fsm_data	), 
	// UART wires
	.uart_rx	( uart_rx	),
	.uart_tx	( uart_tx	)
);	

reg cpu_vma;

always @(negedge E)
	cpu_vma <= AVMA;


bus_manager #(RAM_MSB) bus_mng(
//	.rst50			( rst_clk50		),
//	.clk50			( clk50			),
//	.clk_per		( clk_per		),
	.game_sel		( sw_sel[0]		),
	.ROM_data_out	( ROM_data_out	),
	.RAM_data		( RAM_data		),
	.sound_latch	( sound_latch	),
	.clear_irq		( clear_irq		),
//	.cpu_data_out	( cpu_data_out	),
	.jt_data_out	( jt_data_out	),
	//
	.cpu_data_in	( cpu_data_in	),	
	.cpu_rw			( cpu_rw		),
	.addr			( cpu_addr		),
	.cpu_vma		( cpu_vma		),
	.RAM_cs			( RAM_cs		),
	.opm_cs_n		( jt_cs_n		)
	);

`ifndef NOCPU
wire cpu_firq_n = sw_sel[0] ? jt_irq_n : 1'b1;


mc6809i cpu_good(
    .D		( cpu_data_in	),
    .DOut	( cpu_data_out	),
    .ADDR	( cpu_addr		),
    .RnW	( cpu_rw		),
//    .BS		( BS			),
//    .BA		( BA			),
    .nIRQ	( ~irq			),
    .nFIRQ	( cpu_firq_n	),
    .nNMI	( 1'b1			),
    .AVMA	( AVMA			),
//    .BUSY	( BUSY			),
//    .LIC	( LIC			),
    .nRESET	( ~rst_clk		),
    .nHALT	( 1'b1			),
    .nDMABREQ( 1'b1			),
    .E		( E 			),
    .Q		( Q				)
);
`endif

endmodule 

