// Directed harness qualification ONLY. Never included by FPGA synthesis.
module jt51(input clk,rst,cen,cen_p1,cs_n,wr_n,a0,input[7:0]din,
 output reg[7:0]dout,output irq_n,ct1,ct2,output sample,
 output reg signed[15:0]left,right,xleft,xright);
reg[15:0]cycle;
assign sample=dout==8'h56 ? 1'b0 : dout==8'h55 ? cycle[4:0]==0 : cycle[5:0]==0;
assign irq_n=1; assign ct1=0;assign ct2=0;
always @(posedge clk) if(rst) begin
 cycle<=0;dout<=0;left<=0;right<=0;xleft<=0;xright<=0;
end else if(cen) begin
 cycle<=cycle+1'b1;left<=cycle+1'b1;right<=-cycle-1'b1;
 xleft<=cycle+16'd1001;xright<=-cycle-16'd1001;
 if(!cs_n&&!wr_n&&a0)dout<=din;
end
endmodule
