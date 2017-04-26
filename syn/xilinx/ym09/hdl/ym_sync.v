`timescale 1ns / 1ps

module ym_sync(
  input rst,  
  input clk,
  // YM2151 pins
	input ym_p1,
  input ym_so,
  input ym_sh1,
  input ym_sh2,
	input ym_irq_n,
	input [7:0] ym_data,
	//
	output reg ym_p1_sync,
	output reg ym_so_sync,
	output reg ym_sh1_sync,
	output reg ym_sh2_sync,
	output reg ym_irq_n_sync,
	output reg [7:0] ym_data_sync
);

reg p1_0, so_0, sh1_0, sh2_0, irq_0;
reg [7:0] data0;

always @(posedge ym_p1 or posedge rst ) begin : first_sync
	if( rst ) begin
		p1_0  <= 1'b0;
		so_0  <= 1'b0;
		sh1_0 <= 1'b0;
		sh2_0 <= 1'b0;
		irq_0 <= 1'b1;
		data0 <= 8'h0;
	end
	else begin
		p1_0  <= ym_p1;
		so_0  <= ym_so;
		sh1_0 <= ym_sh1;
		sh2_0 <= ym_sh2;
		irq_0 <= ym_irq_n;
		data0 <= ym_data;
	end
end

always @(posedge clk or posedge rst ) begin : second_sync
	if( rst ) begin
		ym_p1_sync    <= 1'b0;
		ym_so_sync    <= 1'b0;
		ym_sh1_sync   <= 1'b0;
		ym_sh2_sync   <= 1'b0;
		ym_irq_n_sync <= 1'b1;
		ym_data_sync  <= 8'h0;
	end
	else begin
		ym_p1_sync    <= p1_0;
		ym_so_sync    <= so_0;
		ym_sh1_sync   <= sh1_0;
		ym_sh2_sync   <= sh2_0;
		ym_irq_n_sync <= irq_0;
		ym_data_sync  <= data0;
	end
end

endmodule
