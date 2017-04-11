`timescale 1ns / 1ps

/*
 * Milkymist VJ SoC
 * Copyright (C) 2007, 2008, 2009, 2010 Sebastien Bourdeauducq
 * Copyright (C) 2007 Das Labor
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 
 Jose Tejada Dec 2014: 
 	Added parity and error signal
 	Modified to work at 230400 bps with 50MHz clock
 	230400 does not work on the implemented device. Both RX and TX seem to be failing
 	The reason could be electrical: signal loss through the cable. But I have not verified it
	
 Jose Tejada June 2016
  Parity removed.
 */

module uart_transceiver(
	input sys_rst,
	input sys_clk,

	input uart_rx,
	output reg uart_tx, // serial signal to transmit. High when idle

	input [4:0] uart_divider, /* number of divisions of the UART bit period */
	input [4:0] clk_divider, /* Division of the system clock
		For a 50MHz system clock use:
			clk_divider = 28, uart_divider = 30 ->  57kbps, 0.01% timing error		
			clk_divider = 14, uart_divider = 30 -> 115kbps, 0.01% timing error
			clk_divider =  7, uart_divider = 30 -> 230kbps, 0.01% timing error
	*/

	output reg [7:0] rx_data,
	output reg rx_done,
	output reg rx_error,
	output reg [7:0] rx_error_count,

	input [7:0] tx_data,
	input tx_wr,
	output reg tx_done
);

//-----------------------------------------------------------------
// slow_clk generator... this is actually a 32-module counter
//-----------------------------------------------------------------
reg [4:0] clk_counter;

wire slow_clk;
assign slow_clk = !clk_counter;

always @(posedge sys_clk or posedge sys_rst) begin : clock_divider
	if(sys_rst)
		clk_counter <= clk_divider - 4'b1;
	else begin
		clk_counter <= clk_counter - 4'd1;
		if(slow_clk)
			clk_counter <= clk_divider - 4'b1;
	end
end

//-----------------------------------------------------------------
// Synchronize uart_rx
//-----------------------------------------------------------------
reg uart_rx1;
reg uart_rx2;

always @(posedge sys_clk) begin : synchronizer
	uart_rx1 <= uart_rx;
	uart_rx2 <= uart_rx1;
end

//-----------------------------------------------------------------
// UART RX Logic
//-----------------------------------------------------------------
reg rx_busy;
reg [4:0] rx_divcount;
reg [3:0] rx_bitcount;
reg [7:0] rx_reg;

always @(posedge sys_clk or posedge sys_rst) begin : error_count
	if( sys_rst ) begin
		rx_error_count <= 8'd0;
	end
	else begin
		if( rx_done && rx_error ) rx_error_count <= rx_error_count + 1'd1;
	end
end

always @(posedge sys_clk or posedge sys_rst) begin : rx_logic
	if(sys_rst) begin
		rx_done <= 1'b0;
		rx_busy <= 1'b0;
		rx_divcount  <= 5'd0;
		rx_bitcount <= 4'd0;
		rx_data		<= 8'd0;
		rx_reg    <= 8'd0;
		rx_error <= 1'b0;
	end else begin
		rx_done <= 1'b0;
		
		if(slow_clk) begin
			if(~rx_busy) begin // look for start bit
				if(~uart_rx2) begin // start bit found
					rx_busy <= 1'b1;
					rx_divcount <= { 1'b0, uart_divider[4:1] }; // middle bit period
					rx_bitcount  <= 4'd0;
					rx_reg       <= 8'h0;
				end
			end else begin
				if( !rx_divcount ) begin // sample
					rx_bitcount  <= rx_bitcount + 4'd1;
					rx_divcount <= uart_divider;	// start to count down from top again
					rx_error     <= 1'b0;
					case( rx_bitcount )
						4'd0: // verify startbit
							if(uart_rx2)
								rx_busy <= 1'b0;
						4'd9: begin // stop bit
							rx_busy <= 1'b0;
							if(uart_rx2) begin // stop bit ok
								rx_data <= rx_reg;
								rx_done <= 1'b1;	
							end else begin // RX error
								rx_done  <= 1'b1;
								rx_error <= 1'b1;
								end
							end
							default: // shift data in
								rx_reg <= {uart_rx2, rx_reg[7:1]};
					endcase
				end
				else rx_divcount <= rx_divcount - 1'b1;
			end
		end
	end
end

//-----------------------------------------------------------------
// UART TX Logic
//-----------------------------------------------------------------
reg tx_busy;
reg [3:0] tx_bitcount;
reg [4:0] tx_divcount;
reg [7:0] tx_reg;

always @(posedge sys_clk or posedge sys_rst) begin :tx_logic
	if(sys_rst) begin
		tx_done <= 1'b0;
		tx_busy <= 1'b0;
		uart_tx <= 1'b1;
	end else begin
		tx_done <= 1'b0;
		if(tx_wr) begin
			tx_reg <= tx_data;
			tx_bitcount <= 4'd0;
			tx_divcount <= uart_divider;
			tx_busy <= 1'b1;
			uart_tx <= 1'b0;
`ifdef SIMULATION
//			$display("UART: send%c", tx_data);
`endif
		end else if(slow_clk && tx_busy) begin

			if( !tx_divcount ) begin
				tx_bitcount <= tx_bitcount + 4'd1;
				tx_divcount <= uart_divider;	// start to count down from top again
				if( tx_bitcount < 4'd8 ) begin
						uart_tx <= tx_reg[0];
						tx_reg <= {1'b0, tx_reg[7:1]};
						end
				else begin
					uart_tx <= 1'b1; // 8 bits sent, now 1 or more stop bits
					if( tx_bitcount==4'd10 ) begin
						tx_busy <= 1'b0;
						tx_done <= 1'b1;
					end
				end
			end
			else tx_divcount  <= tx_divcount - 1'b1;
		end
	end
end

endmodule
