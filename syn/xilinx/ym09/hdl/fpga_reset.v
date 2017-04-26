`timescale 1ns / 1ps

module fpga_reset(
  input      clk,
  input      ext_rst,
  output reg rst
);

reg [7:0]  cnt = 8'h0; // the FPGA should initialize this with its own POR

always @(negedge clk or posedge ext_rst) begin
  if( ext_rst ) begin
    cnt  <= 8'h0;
    rst  <= 1'b1;
  end
  else if( cnt != 8'hFF ) begin
    rst  <= 1'b1;
    cnt  <= cnt + 1'b1;
  end
  else if( cnt==8'hFF ) rst <= 1'b0;
end

endmodule

