module system_bus(
	input  wire clk,
	input  wire rst,
	// MEMORY
	output reg  [ 7:0] cpu_data_i,
	input  wire [ 7:0] cpu_data_o,
	input  wire        cpu_rw,
	input  wire        cpu_vma,
	//	input  wire        cpu_oe_o,
	input  wire [ 7:0] memory_data_o,
	input  wire [15:0] address,
	output wire ram_cs,
	// UART
	input  wire [ 7:0] uart_rx_byte,
	output reg         uart_transmit,
	output reg  [ 7:0] uart_tx_byte,
	input			   uart_speed,
	// IRQ handling
	output reg         rx_status_clear,
	output reg         tx_status_clear,
	input  wire        rx_status,
	input  wire        tx_status,
	// YM2151 pins
	inout  wire [ 7:0] ym_d,
	output reg         ym_a0,
	output reg         ym_wr_n,
	output reg         ym_rd_n,
	output reg         ym_cs_n,
	output reg         ym_ic_n,
	input  wire        ym_irq_n,
	input  wire        ym_ct1,
	input  wire        ym_ct2,
	input  wire        ym_so,
	input  wire        ym_sh1,
	input  wire        ym_sh2,
	output wire        ym_pm, // system clock
	input  wire        ym_p1, // DAC clock
	// JT51 pins
	output				jt51_cs_n,  // chip select
	input 		[7:0]	jt51_do, // data out
	input 				jt51_ct1,
	input wire			jt51_ct2,
	input wire			jt51_irq_n,
	input wire			jt51_sh1,
	input wire			jt51_sh2,
	input wire signed	[15:0] jt51_left,
	input wire signed	[15:0] jt51_right,	
	// level shifters
	output reg         dir, // 0 means FPGA writes on YM, 1 means FPGA reads from YM
	// LED
	output reg  [ 7:0] led,
	output reg  [ 7:0] led_alt
);

parameter version_number = 8'hff;

parameter UART_DATA    = 16'h800;
parameter UART_RXSTATUS= 16'h801;
parameter UART_TXSTATUS= 16'h802;
parameter LED          = 16'h810;
parameter LED_ALT      = 16'h811;
parameter VERSION      = 16'h820;
parameter YMCTRL       = 16'hA00;
parameter YMSIGNALS    = 16'hA01;
parameter YMDATA       = 16'hA02;
parameter YMPM         = 16'hA03;
parameter YMICN        = 16'hA04;
parameter YMA0         = 16'hA05;
parameter YMSPEED      = 16'hA06;
parameter YMDATA_SYNC  = 16'hA08;
parameter YMLEFT       = 16'hA0A;
parameter YMRIGHT      = 16'hA0C;
parameter YMCNT        = 16'hA10;     // 4 bytes
parameter YMLEFT_EXP   = 16'hA1A;			// 2 bytes
parameter YMRIGHT_EXP  = 16'hA1C;
parameter YMCNT_CLR    = 16'hA20;     // cualquier escritura borra los cuatro byes de YMCNT
// JT51
parameter JTSIGNALS    = 16'hB01;
parameter JTDATA0      = 16'hB02;
parameter JTDATA1      = 16'hB03;
parameter JTLEFT       = 16'hB0A;
parameter JTRIGHT      = 16'hB0C;

parameter WRITE=1'b0, READ=1'b1;

assign ram_cs = cpu_vma && (address >= 16'h8000 ); // last 32 kbytes
assign jt51_cs_n = cpu_vma && (address == 16'hB02 || address == 16'hB03 );

wire cpu_rd, cpu_wr;
reg   [7:0] ym_din;
reg         rst_counter;
wire [31:0] pm_counter;
reg         ym_real_speed;

assign cpu_rd = cpu_rw & cpu_vma;
assign cpu_wr = ~cpu_rw & cpu_vma;
assign ym_d = dir==WRITE ? ym_din : 8'bZZZZZZZZ;

wire ym_p1_sync, ym_so_sync, ym_sh1_sync, ym_sh2_sync, ym_irq_n_sync;
wire [7:0] ym_data_sync;

pm_clk_real u_pm(
	.clk        ( clk           ),
	.rst        ( rst           ),
	.real_speed ( ym_real_speed ),
	.irq_n      ( ym_irq_n_sync ),
	.rst_counter( rst_counter   ),
	.ym_pm      ( ym_pm         ),
	.pm_counter ( pm_counter    ),
	.uart_speed ( uart_speed 	)
);

ym_sync u_synchronizer(
	.clk          ( clk           ),
	.rst          ( rst           ),
	// YM2151 pins
	.ym_p1        ( ym_p1         ),
	.ym_so        ( ym_so         ),
	.ym_sh1       ( ym_sh1        ),
	.ym_sh2       ( ym_sh2        ),
	.ym_irq_n     ( ym_irq_n      ),
	.ym_data      ( ym_d          ),
	//
	.ym_p1_sync   ( ym_p1_sync    ),
	.ym_so_sync   ( ym_so_sync    ),
	.ym_sh1_sync  ( ym_sh1_sync   ),
	.ym_sh2_sync  ( ym_sh2_sync   ),
	.ym_irq_n_sync( ym_irq_n_sync ),
	.ym_data_sync ( ym_data_sync  )
);

wire [15:0] left, right, left_exp, right_exp;
wire so_update_left, so_update_right;

so2par u_so2par(
	.clk      ( clk       ),
	.ym_so    ( ym_so     ),
	.ym_sh1   ( ym_sh1    ),
	.ym_sh2   ( ym_sh2    ),
	.ym_p1    ( ym_p1     ),
	.left     ( left      ),
	.right    ( right     ),
	.left_exp ( left_exp  ),
	.right_exp( right_exp ),
	.update_left ( so_update_left  ),
	.update_right( so_update_right )
);

// DATA WRITE
always @(posedge clk or posedge rst) begin : ym_control
	if( rst ) begin
    // YM signals
		ym_a0      <= 1'b0;
		ym_wr_n    <= 1'b0;
		ym_rd_n    <= 1'b0;
		ym_cs_n    <= 1'b0;
		ym_ic_n    <= 1'b0;
		dir        <= 1'b0;
		ym_real_speed <= 1'b1;
    // UART
		uart_transmit   <= 1'b0;
		uart_tx_byte    <= 8'h0;
		rx_status_clear <= 1'b0;
		tx_status_clear <= 1'b0;
    // other
		led           <= 8'h0;
		led_alt       <= 8'h0;
	end
	else begin
		if( cpu_wr )
			case( address )
				YMCTRL: begin
						ym_wr_n <= cpu_data_o[0];
						ym_rd_n <= cpu_data_o[1];
						dir     <= cpu_data_o[2];
						ym_cs_n <= cpu_data_o[7];
					end
				UART_DATA: begin
						  uart_tx_byte  <= cpu_data_o;
						  uart_transmit <= 1'b1;
					  end
        // write to single registers with no other effect:
				YMSPEED:       ym_real_speed     <= cpu_data_o[0];
				YMICN:         ym_ic_n           <= cpu_data_o[0];
				YMDATA:        ym_din            <= cpu_data_o;
				YMA0:          ym_a0             <= cpu_data_o[0];
				YMCNT_CLR:     rst_counter       <= 1'b1;
				LED:           led               <= cpu_data_o;
				LED_ALT:       led_alt           <= cpu_data_o;
				UART_RXSTATUS: rx_status_clear   <= 1'b1;
				UART_TXSTATUS: tx_status_clear   <= 1'b1;
			endcase
  else begin
			// these signals are only allowed to be 1 for one clock cycle
			uart_transmit   <= 1'b0;
			rx_status_clear <= 1'b0;
			tx_status_clear <= 1'b0;
			rst_counter     <= 1'b0;
		end
	end
end

// DATA READ
always @(*) begin : data_read
	if( cpu_rd )
    case( address )
		UART_RXSTATUS:cpu_data_i <= { 7'b0, rx_status };
		UART_TXSTATUS:cpu_data_i <= { 7'b0, tx_status };
		UART_DATA:    cpu_data_i <= uart_rx_byte;
		YMCTRL:    	  cpu_data_i <= { ym_cs_n, 5'b0, ym_rd_n, ym_wr_n };
		YMSIGNALS:    cpu_data_i <= { ym_irq_n_sync, ym_ct2, ym_ct1, ym_pm,
										ym_p1_sync, ym_sh2_sync, ym_sh1_sync, ym_so_sync };
		YMICN:     	  cpu_data_i <= { 7'h0, ym_ic_n };
		YMPM:         cpu_data_i <= { 7'h0, ym_pm };
		YMDATA:   	  cpu_data_i <= ym_d;
		YMDATA_SYNC:  cpu_data_i <= ym_data_sync;
		YMA0:         cpu_data_i <= ym_a0;
		LED:          cpu_data_i <= led;
		LED_ALT:		cpu_data_i <= led_alt;
		YMSPEED:		cpu_data_i <= ym_real_speed;
		// JT51
		JTSIGNALS:		cpu_data_i <= { jt51_irq_n, jt51_ct2, jt51_ct1, 2'b0,
										jt51_sh2, jt51_sh1, 1'b0 };
		JTDATA0:		cpu_data_i <= jt51_do;
		// audio data
		JTLEFT:			cpu_data_i <= jt51_left[15:8];
		JTLEFT+16'h1:	cpu_data_i <= jt51_left[ 7:0];
		JTRIGHT:		cpu_data_i <= jt51_right[15:8];
		JTRIGHT+16'h1:	cpu_data_i <= jt51_right[ 7:0];
		YMLEFT:       cpu_data_i <= left [15:8];
		YMLEFT+16'h1: cpu_data_i <= left [ 7:0];
		YMRIGHT:      cpu_data_i <= right[15:8];
		YMRIGHT+16'h1:cpu_data_i <= right[ 7:0];
		YMLEFT_EXP:       cpu_data_i <= left_exp [15:8];
		YMLEFT_EXP+16'h1: cpu_data_i <= left_exp [ 7:0];
		YMRIGHT_EXP:      cpu_data_i <= right_exp[15:8];
		YMRIGHT_EXP+16'h1:cpu_data_i <= right_exp[ 7:0];

		// counter
		YMCNT:        cpu_data_i <= pm_counter[31:24];
		YMCNT+16'h1:  cpu_data_i <= pm_counter[23:16];
		YMCNT+16'h2:  cpu_data_i <= pm_counter[15:08];
		YMCNT+16'h3:  cpu_data_i <= pm_counter[ 7:0 ];
		VERSION:      cpu_data_i <= version_number;
      default:		  cpu_data_i <= ram_cs ? memory_data_o : 8'h0;
    endcase
	else
		cpu_data_i <= 8'h0;
end


endmodule
