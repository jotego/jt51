`timescale 1ns / 1ps

module rst_sync(
	input 		rst_in,
	input		clk,
	output reg	rst_out
);

reg aux;

always @(posedge clk or posedge rst_in ) begin
	if( rst_in ) begin
		aux <= 1'b1;
		rst_out <= 1'b1;
	end
	else begin
		rst_out <= aux;
		aux		<= 1'b0;
	end
end

endmodule
