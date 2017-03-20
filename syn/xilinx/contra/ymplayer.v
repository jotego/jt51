`timescale 1ns / 1ps
`timescale 1ns / 1ps

module ymplayer(
	input	clk50,
	input	rst,
	// sound
	output	speaker_left,
	output	speaker_right,
	// switches
	input [1:0]	sw_sel,	// 
	input		send_data, // 1=Send music over UART, 0=stop
	// UART wires
	input		uart_rx,
	output		uart_tx,
	output [7:0] led
);

wire clk_dac, clk, locked, rst_clk, rst_clk50;

// Send data
reg	[9:0] send_data_shr;
wire send_data_s = &send_data_shr;

always @(posedge clk50)
	send_data_shr <= { send_data_shr[8:0], send_data };

clocks u_clocks(
    .rst	( rst		),
    .clk50	( clk50		),
	.locked	( locked	),
	.divide_more( send_data_s ),
    .clk_cpu( clk		),
	.clk_dac( clk_dac	)
);

// wire clk_per = clk; // cpu09l.vhd
wire clk_per = ~clk; // cpu09new.vhd
wire cpu_rst_req;

rst_sync u_rst1(
	.rst_in	( rst|(~locked)|cpu_rst_req	),
	.clk	( clk			),
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

wire cpu_rw;

wire signed	[15:0] jt_left, jt_right;

wire jt_sample;

// JT51
`ifndef NOJT
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
	.xleft	( jt_left		),
	.xright	( jt_right		)
);
`endif

speaker u_speaker(
	.clk100		( clk50		), // the faster the clock the better !
	.left_in	( jt_left	),
	.right_in	( jt_right	),
	.left_out	( speaker_left	),
	.right_out	( speaker_right	)
);

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
	.ena	( 1'b1			),
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
	.clk_cpu	( clk		),
	.rst		( rst_clk50	),
	// Sound
	.sound_latch(sound_latch),
	.jt_left	( jt_left	),
	.jt_right	( jt_right	),     
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


bus_manager #(RAM_MSB) bus_mng(
	.rst50			( rst_clk50		),
	.clk50			( clk50			),
	.clk_per		( clk_per		),
	.sw_sel			( sw_sel		),
	.ROM_data_out	( ROM_data_out	),
	.RAM_data		( RAM_data		),
	.sound_latch	( sound_latch	),
	.clear_irq		( clear_irq		),
	.cpu_data_out	( cpu_data_out	),
	.jt_data_out	( jt_data_out	),
	//
	.cpu_data_in	( cpu_data_in	),	
	.cpu_rw			( cpu_rw		),
	.addr			( cpu_addr		),
	.cpu_vma		( cpu_vma		),
	.RAM_cs			( RAM_cs		),
	.opm_cs_n		( jt_cs_n		)
	);

wire [15:0] dummy_pcout; // cpu09 debug

wire	cpu_firq = sw_sel[0] ? ~jt_irq_n : 1'b0;

cpu09 cpu(
	.clk		( clk			),
	.rst		( rst_clk		),
	.rw			( cpu_rw		),
	.vma		( cpu_vma		),
	.address	( cpu_addr		),
	.data_in	( cpu_data_in	),
	.data_out	( cpu_data_out	),
	.halt		( 1'b0			),
	.hold		( 1'b0			),
	.irq		( irq			),
	.firq		( cpu_firq		), // for Contra	
	.nmi		( 1'b0			),
	.pc_out		( dummy_pcout	)
	);

/*
// cpu09l.vhd
cpu09 cpu(
	.clk		( clk			),
	.rst		( rst_clk		),
	.rw			( cpu_rw		),
	.vma		( cpu_vma		),
	.addr		( cpu_addr		),
	.data_in	( cpu_data_in	),
	.data_out	( cpu_data_out	),
	.halt		( 1'b0			),
	.hold		( 1'b0			),
	.irq		( irq			),
	.firq		( 1'b0			), // for Contra	
	.nmi		( 1'b0			),
	.lic		(),
	.ifetch		(),
	.ba			(),
	.bs			()
	//.pc_out		( pc_out		)
	);
*/
endmodule 

