`timescale 1ns / 1ps

module fake_tone(
	input rst,
	input clk,
	input ym_p1,
	output onebit
);
	wire [15:0] tone, linear;
	wire sh, so;
		
	ramp_a_tone tonegen( .rst(rst), .clk(clk), .tone(tone) );
	sh_encode encoder(   .rst(rst), .ym_p1(ym_p1), .data(tone), .sh(sh), .so(so) );
	ym_linearize linearizer( .rst(rst), .sh(sh), .ym_so(so), .ym_p1(ym_p1), .linear(linear) );
	sigma_delta1 sd1(    .rst(rst), .clk(clk), .data(linear), .sound(onebit) );	
endmodule

module sh_encode(
	input rst,
	input ym_p1,
	input [15:0] data,
	output reg sh,
	output so
);
	reg [12:0] serial_data;
	reg [3:0] cnt;
	
	assign so = serial_data[0];	
	always @(posedge rst or posedge ym_p1) begin
		if( rst ) begin
			sh <= 1'b0;			
			cnt <= 0;
		end
		else begin			
			cnt <= cnt + 1'b1;
			if( cnt==4'd2 ) begin
				casex( data[15:10] )
					6'b1XXXXX: serial_data <= { 3'd7, data[15:6]}; 
					6'b01XXXX: serial_data <= { 3'd6, data[14:5]}; 
					6'b001XXX: serial_data <= { 3'd5, data[13:4]}; 
					6'b0001XX: serial_data <= { 3'd4, data[12:3]}; 
					6'b00001X: serial_data <= { 3'd3, data[11:2]}; 
					6'b000001: serial_data <= { 3'd2, data[10:1]}; 
					default:   serial_data <= { 3'd1, data[ 9:0]}; 
				endcase
			end
			else serial_data <= serial_data>>1;
			if( cnt==4'd10 ) sh<=1'b1;
			if( cnt==4'd15 ) sh<=1'b0;
		end
	end
endmodule

// it produces a ~440Hz triangular signal at full scale for a 50MHz clock
module ramp_a_tone ( input rst, input clk, output reg [15:0] tone );
	reg up;
	always @(posedge rst or posedge clk) begin
		if( rst ) begin
			up   <= 0;
			tone <= 0;
		end
		else begin
			if( tone == 16'hFFFE ) begin
				up <= 1'b0;
			end
			else if( tone == 16'h1 ) begin
				up <= 1'b1;
			end
			tone <= up ? (tone+1'b1) : (tone-1'b1);
		end
	end
endmodule
