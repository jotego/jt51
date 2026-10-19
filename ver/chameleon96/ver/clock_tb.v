`timescale 1ns/1ps
module clock_tb;
reg clk=0,enable=1;
wire dut_clk;
integer edges=0,checks=0;
reg [63:0] previous_rise=0;
always #10 clk=~clk;
always @(posedge dut_clk) begin
    if (!clk) $fatal(1,"DUT rise outside parent rise");
    if (previous_rise!=0 && $time-previous_rise<20) $fatal(1,"short/extra DUT pulse");
    previous_rise=$time;
    edges=edges+1;
end
jt51_replay_clock dut(.*);
task check;
input good;
input [255:0] label_text;
begin checks=checks+1;if(!good)$fatal(1,"clock check failed: %s",label_text);end
endtask
initial begin
    #11;check(dut_clk,"initial pass-through rise");
    #2;enable=0;
    #1;check(dut_clk,"disable cannot shorten high pulse");
    #7;check(!dut_clk,"falling edge disables gate");
    #51;check(edges==1&&!dut_clk,"host clocks cannot advance stopped DUT");
    enable=1;
    #1;check(!dut_clk,"enable cannot add midcycle rise");
    #18;check(dut_clk&&edges==2,"enable resumes at next whole parent rise");
    #1;enable=0;
    #9;check(!dut_clk,"second clean disable");
    #9;check(!dut_clk&&edges==2,"second host rise suppressed");
    #3;enable=1;
    #19;check(dut_clk&&edges==3,"second clean resume");
    $display("PASS %0d glitch-free clock checks",checks);$finish;
end
endmodule
