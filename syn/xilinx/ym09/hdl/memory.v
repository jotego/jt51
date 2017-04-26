`timescale 1ns / 1ps

module memory #(parameter width=10 )(
    input  wire [7:0] datain,
    output reg  [7:0] dataout,
    input  wire clk,
    input  wire [width-1:0] addr,
    input  wire en,
    input  wire we		// high for write, low for red
    );

reg [7:0] mem[(2**width)-1:0];

`ifdef SIM_SKIPPROG
	initial $readmemh("../../asm/out/ym09.hex", mem, (2**width)-4096, (2**width)-1 );
`endif

always @(negedge clk) begin
	if( en ) begin
		if( we ) 
			mem[addr] <= datain;
		else 
			dataout  <= mem[addr];
	end		
end

endmodule
