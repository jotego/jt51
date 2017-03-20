`timescale 1ns / 1ps

module sigma_delta1(
	input			rst,
	input			clk,
	input [15:0]	data,
	output			sound
);
	parameter width=16;
	reg [width:0] cnt;
	
	reg [width-1:0] predata, good_data;

	// synchronize with the sigma delta clock to avoid meta-states
	// this may not be much of a practical issue, but it is simple to do it right
	// so I do it.
	always @( posedge clk  ) begin
		if( rst ) begin
			predata   <= {width{1'd0}};
			good_data <= {width{1'd0}};			
		end
		else begin
			predata   <= data;
			good_data <= predata;
		end
	end	
	
	assign sound=cnt[width];
	always @( posedge clk ) begin
		if( rst ) begin
			cnt <= 0;
		end
		else begin
			cnt <= cnt[width-1:0] + good_data[width-1:0];
			// $display("%d", sound );
		end
	end
endmodule

