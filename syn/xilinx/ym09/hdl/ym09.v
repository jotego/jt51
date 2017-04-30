`timescale 1ns / 1ps

module ym09(
	input  wire clk50,
	input  wire ext_rst,
	// UART pins
	input  wire uart_rx,
	output wire uart_tx,
	// YM2151 pins		
	inout  wire [7:0] ym_d,
	output wire ym_a0,
	output wire ym_wr_n,
	output wire ym_rd_n,
	output wire ym_cs_n,
	output wire ym_ic_n,
	input  wire ym_irq_n,
	input  wire ym_ct1,
	input  wire ym_ct2,
	input  wire ym_so,
	input  wire ym_sh1,
	input  wire ym_sh2,
	output wire ym_pm, // system clock
	input  wire ym_p1, // DAC clock	     
	// level shifters
	output wire DIR, // 0 means FPGA writes on YM, 1 means FPGA reads from YM    
	// LED control
	output reg  [7:0] led,
	input  wire [2:0] ledcfg,
	input  wire uart_speed,
	input  wire dump_memory
);


/* historial de versiones
  
 6  Separado el estado de la UART de rx y tx en dos posiciones de memoria distintos
 9  RAM ampliada a 32 kB. Esta version no sintetizaba a 50MHz.
10  Volcado de memoria por la UART. Esta version no sintetizaba a 50MHz.
11  Control de la velocidad del YM2151 para poder bajarla durante el volcado directo.
12  La velocidad lenta del YM es aun mas lenta.
13  Añade la lectura del audio en formato exponencial directamente por la CPU
14  Añade lectura de datos YM sincronizados con ym_p1
15  Añade el JT51
16	Actualiza el JT51 y trata de medir el OP31

*/

parameter version_number = 8'd16;

wire [15:0] cpu_addr;
wire [ 7:0] cpu_data_i, cpu_data_o, memory_data_o;
wire        cpu_rw, cpu_vma;
wire        ram_cs;

wire  [7:0] uart_rx_byte, uart_tx_byte, uart_error_count;
wire        uart_transmit, uart_received;
wire        uart_irq, uart_irq_clear;
wire  [2:0] uart_status;

wire [ 7:0] led09, led09_alt, fsm_led;

wire [11:0] fsm_addr;
wire        fsm_wr;
wire        cpu_rst;

wire	[15:0]	jt51_left, jt51_right;
wire	[7:0]	jt51_do;
wire	jt51_ct1, jt51_ct2, jt51_irq_n, jt51_sh1, jt51_sh2;
 
wire        rst;

always @(*) begin
	case( ledcfg )
		default: led <= cpu_rst ? fsm_led : led09;
		3'b000:  led <= { ym_ic_n, 3'd0, ym_cs_n, ym_wr_n, ym_rd_n, ym_a0 };
		3'b001:  led <= { ym_irq_n, ym_ct2, ym_ct1, ym_pm, 
											ym_p1, ym_sh2, ym_sh1, ym_so };
		3'b010:  led <= fsm_addr[ 7:0];
		3'b011:  led <= { 4'h0, fsm_addr[11:8] };
		3'b100:  led <= cpu_rst ? fsm_led : led09;
		3'b101:  led <= cpu_rst ? fsm_led : led09_alt;
		3'b110:  led <= { uart_irq, 3'b0, 1'b0, uart_status };
		3'b111:  led <= version_number;
	endcase
end

pll u_pll (
    .CLKIN_IN(clk50), 
    .CLKFX_OUT(clk)  // 16.67 MHz
    //.CLKIN_IBUFG_OUT(CLKIN_IBUFG_OUT), 
    //.CLK0_OUT(CLK0_OUT), 
    //.LOCKED_OUT(LOCKED_OUT)
    );

fpga_reset u_rst(
  .clk    (   clk   ),
  .ext_rst( ext_rst ),
  .rst    (     rst )
);

wire dump_memory_sync;

debouncer u_debouncer(
  .clk    (     clk     ),
  .rst    (     rst     ),
  .PB     ( dump_memory ),  // "PB" is the glitchy, asynchronous to clk, active low push-button signal
  .PB_up  ( dump_memory_sync ) // 1 for one clock cycle when the push-button goes up (i.e. just released)
);

system_bus #(version_number) u_bus(  
	.clk           ( clk            ),
	.rst           ( rst            ),
	.cpu_data_i    ( cpu_data_i     ),
	.cpu_data_o    ( cpu_data_o     ),  
	.cpu_rw        ( cpu_rw         ),
	.cpu_vma       ( cpu_vma        ),
	.memory_data_o ( memory_data_o  ),
	.address       ( cpu_addr       ),
	.ram_cs        ( ram_cs         ),
	// UART
	.uart_rx_byte   ( uart_rx_byte   ), 
	.uart_transmit  ( uart_transmit  ),
	.uart_tx_byte   ( uart_tx_byte   ),
	.rx_status      ( rx_status      ), 	// IRQ handling
	.tx_status      ( tx_status      ),
	.rx_status_clear( rx_status_clear),
	.tx_status_clear( tx_status_clear),
	.uart_speed		( uart_speed	 ),
	// YM2151 pins     
	.ym_d          ( ym_d           ),
	.ym_a0         ( ym_a0          ),
	.ym_wr_n       ( ym_wr_n        ),
	.ym_rd_n       ( ym_rd_n        ),
	.ym_cs_n       ( ym_cs_n        ),
	.ym_ic_n       ( ym_ic_n        ),
	.ym_irq_n      ( ym_irq_n       ),
	.ym_ct1        ( ym_ct1         ),
	.ym_ct2        ( ym_ct2         ),
	.ym_so         ( ym_so          ),
	.ym_sh1        ( ym_sh1         ),
	.ym_sh2        ( ym_sh2         ),
	.ym_pm         ( ym_pm          ),
	.ym_p1         ( ym_p1          ), 
	// JT51 pins
	.jt51_cs_n		( jt51_cs_n		),
	.jt51_left		( jt51_left		),
	.jt51_right		( jt51_right	),
	.jt51_do		( jt51_do		),
	.jt51_ct1		( jt51_ct1		),
	.jt51_ct2		( jt51_ct2		),
	.jt51_irq_n		( jt51_irq_n	),
	.jt51_sh1		( jt51_sh1		),
	.jt51_sh2		( jt51_sh2		),	
	// level shifters
	.dir           ( DIR            ), // 0 means FPGA writes on YM, 1 means FPGA reads from YM  
	// LED
	.led           ( led09          ),                                                         
	.led_alt       ( led09_alt      )
);

memory #(15)u_memory(
    .datain ( fsm_wr ? uart_rx_byte : cpu_data_o ),
    .dataout( memory_data_o    ),
    .clk    ( clk              ),
    .addr   ( cpu_rst ? {3'b111, fsm_addr} : cpu_addr[14:0] ),
    .en     ( cpu_rst | ram_cs  ),
    .we     ( cpu_rst ? fsm_wr  : ~cpu_rw )		// high for write, low for read
    );
    
uart09 #(12)u_uart(
	.clk			( clk           ),
	.rst			( rst           ),
	.uart_rx		( uart_rx       ),
	.uart_tx		( uart_tx       ),
	.uart_rx_byte	( uart_rx_byte  ), 
	.uart_transmit	( uart_transmit ),
	.uart_tx_byte	( uart_tx_byte  ),
	.mem_data_o		( memory_data_o ),
	.uart_error_count( uart_error_count ),
	.uart_received	( uart_received ),
	.uart_speed		( uart_speed    ),
	// IRQ handling
	.rx_status      ( rx_status       ),
	.tx_status      ( tx_status       ),
	.rx_status_clear( rx_status_clear ),
	.tx_status_clear( tx_status_clear ),
	// control RAM load
	.fsm_addr     ( fsm_addr      ),	
	.fsm_wr	      ( fsm_wr        ),
	.cpu_rst      ( cpu_rst       ),
	.led          ( fsm_led       ),
	.dump_memory  ( dump_memory_sync )
);

//wire [15:0] pc_out;

cpu09 cpu(
	.clk     ( clk          ),
	.rst     ( cpu_rst      ),
	.rw      ( cpu_rw       ),
	.vma     ( cpu_vma      ),
	.address ( cpu_addr     ),
	.data_in ( cpu_data_i   ),
	.data_out( cpu_data_o   ),
	.halt    ( 1'b0         ),
	.hold    ( 1'b0         ),
	.irq     ( rx_status    ),
	.firq    ( 1'b0         ), 
	.nmi     ( 1'b0         ),
	.pc_out  (			    )
  );

jt51	u_jt51(
	.clk	( ~clk		),  // main clock
	.rst	( cpu_rst	),  // reset
	.cs_n	( jt51_cs_n	),  // chip select
	.wr_n	( ~cpu_rw	),  // write
	.a0		( cpu_addr[0] ),
	.d_in	( cpu_data_o), // data in
	.d_out	( jt51_do	), // data out
	.ct1	( jt51_ct1	),
	.ct2	( jt51_ct2	),
	.irq_n	( jt51_irq_n),
	.left	( jt51_left	),
	.right	( jt51_right)
);

endmodule
