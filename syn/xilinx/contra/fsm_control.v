`timescale 1ns / 1ps

module fsm_control(
	input 		clk,
	input		clk_cpu,
	input 		rst,
	input		jt_sample,
	// Sound
	output reg	[ 7:0] sound_latch,
	input		[15:0] jt_left,
	input		[15:0] jt_right,         
	output reg 	irq,
	input 		clear_irq,
	// Program
	output reg	cpu_rst,
	output		rom_prog,
	output reg	rom_wr,
	output reg [14:0] rom_addr,
	output reg [7:0]  rom_data,
	// UART wires
	input		uart_rx,
	output		uart_tx
);	


// UART control
wire  	[7:0] 	uart_rx_data; 
reg		[7:0]	uart_tx_data;
wire 			uart_received, uart_error;
reg				uart_tx_wr;
wire			uart_tx_done;
reg		[1:0] 	send_st;

parameter TX_IDLE=0, TX_WAIT_LSB=1, TX_WAIT_MSB=2;

reg	[15:0]	left, right, mono;
reg			sample;

always @(*) { left, right, sample } <= { jt_left, jt_right, jt_sample };    
always @(*) mono <= (left+right)>>1;

// Send music to computer
reg	last_sample;

always @(posedge clk or posedge rst) begin
	if( rst ) begin
		uart_tx_wr	<= 1'b0;
		send_st		<= TX_IDLE;
	end
	else begin
		last_sample <= sample;
		case( send_st )
			TX_IDLE:
				if( sample && !last_sample ) begin
					send_st		<= TX_WAIT_LSB;
					uart_tx_data<= mono[7:0];
					uart_tx_wr	<= 1'b1;
				end
			TX_WAIT_LSB: begin
				uart_tx_wr	<= 1'b0;
				if( uart_tx_done ) begin
					send_st		<= TX_WAIT_MSB;
					uart_tx_data<= mono[15:8];
					uart_tx_wr	<= 1'b1;
				end
			end
			TX_WAIT_MSB: begin
				uart_tx_wr	<= 1'b0;
				if( uart_tx_done ) 
					send_st		<= TX_IDLE;
			end
		endcase
	end
end

reg	set_irq;

always @(posedge clk_cpu or posedge rst ) begin
	if( rst )
		irq <= 1'b0;
	else begin
		if(set_irq) irq<=1'b1;
		else if(clear_irq ) irq<=1'b0;
	end
end

reg [1:0] irq_s;
reg rom_wr_s, prog_done_s, adv_s;
reg advance, prog_done;

assign rom_prog = !prog_done;

always @(posedge clk or posedge rst ) begin
	if( rst ) begin
		// Sound
		sound_latch <= 8'h0;
		set_irq     <= 1'b0;	
		cpu_rst		<= 1'b1;
	end
	else begin		
		irq_s <= { irq_s[0], irq };
		rom_wr_s <= rom_wr;
		prog_done_s <= prog_done;
		if( uart_received && !uart_error ) begin
			if( prog_done ) begin					
					sound_latch <= uart_rx_data;
					set_irq     <= 1'b1;
				end
			else begin
					advance		<= 1'b1;					
				end
    	end
		else begin
			cpu_rst <= !prog_done;
			if(irq_s[1]) set_irq <= 1'b0;
			if(rom_wr_s ) advance <= 1'b0;
		end
	end	
end

reg	prog_st;

parameter WAIT=1, NEXT=0;

always @(posedge clk_cpu or posedge rst ) 
	if( rst ) begin
		prog_st <= WAIT;
		rom_wr	<= 1'b0;
		prog_done<= 1'b0;
		`ifdef FASTSIM
		rom_addr<= 15'h7FF0;
		`else
		rom_addr<= 15'h0;
		`endif
	end
	else begin
		adv_s <= advance;
		case( prog_st )
			NEXT: begin
				{ prog_done,rom_addr } <= rom_addr + 1'b1;
				rom_wr	<= 1'b0;				
				prog_st	<= WAIT;
				end
			WAIT: if( adv_s ) begin
					rom_wr		<= 1'b1;
					rom_data	<= uart_rx_data;
					prog_st		<= NEXT;					
				end
		endcase	
	end


uart_transceiver u_uart(
	.sys_rst( rst ),
	.sys_clk( clk ),

	.uart_rx(uart_rx),
	.uart_tx(uart_tx),

	// 461 kbps @ 50MHz
	// .clk_divider(  5'd6 ), 
	// 921.6 kbps @ 50MHz
	.clk_divider(  5'd3 ), 
	.uart_divider( 5'd17 ), 

	.rx_data( uart_rx_data ),
	.rx_done( uart_received ),
	.rx_error( uart_error ),
  
	.tx_data	( uart_tx_data	),
	.tx_wr		( uart_tx_wr	),
	.tx_done	( uart_tx_done	)
);


endmodule
