`timescale 1ns / 1ps

module memory #(parameter msb=9 )(
    input 	[7:0] 	datain,
    output reg [7:0] dataout,
    input 			clk,
    input [msb:0] 	addr,
    input 			en,
    input 			we		// high for write, low for red
    );

reg [7:0] mem[(2**(msb+1))-1:0];

always @(posedge clk) begin
	if( en ) begin
		if(we)
			mem[addr] <= datain;
		dataout <= mem[addr];
	end
end

endmodule
