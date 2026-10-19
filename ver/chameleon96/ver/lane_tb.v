`timescale 1ns/1ps
module lane_tb;
reg stream_enable=0,continue_page=0;
reg [31:0] stream_event_first=0;
wire paused;
wire [31:0] total_executed,page_index;
reg clk=0;
always #5 clk=~clk;
reg rst=1,start=0,abort_run=0,prog_we=0,output_mode=0;
reg[3:0]prog_addr=0,read_addr=0;
reg[63:0]prog_data=0;
reg[4:0]length=5,event_first=1,capture_limit=3;
reg[47:0]idle_clocks=0;
reg[5:0]event_shift=0;
reg[23:0]stride=2;
wire[63:0]prog_q;
wire[39:0]read_data;
wire busy,done,failed;
wire[7:0]error_code,last_status;
wire[4:0]count,executed;
wire[47:0]logical_clock,first_clock,last_clock,failure_clock;
wire debug_cs_n,debug_wr_n,debug_a0,debug_dut_rst,debug_sample;
wire[7:0]debug_din;
integer checks=0,i;
reg[47:0]saved_first,saved_last;
reg[4:0]saved_count;
reg[39:0]saved_data;
jt51_replay_lane #(.AW(4),.CAP_AW(4))dut(.*);
task tick;begin @(posedge clk);#1;end endtask
task check;
 input condition;
 input[255:0]label;
 begin checks=checks+1;if(!condition)begin $display("FAIL %0s",label);$fatal;end end
endtask
function[63:0]rec;
 input[47:0]ts;
 input[3:0]op;
 input[7:0]data;
 input[2:0]pins;
 begin rec={ts,op,data,pins,1'b0};end
endfunction
task put;
 input[3:0]addr;
 input[63:0]data;
 begin @(negedge clk);prog_we=1;prog_addr=addr;prog_data=data;tick;@(negedge clk);prog_we=0;tick;check(prog_q==data,"synchronous tape readback");end
endtask
task launch;
 begin @(negedge clk);start=1;tick;@(negedge clk);start=0;end
endtask
task finish_run;
 input[7:0]checked_error;
 integer deadline;
 begin deadline=0;while(!done&&deadline<100000)begin tick;deadline=deadline+1;end
 $display("terminal expected=%0d actual=%0d count=%0d executed=%0d",checked_error,error_code,count,executed);
 check(done,"bounded completion");check(error_code==checked_error,"checked terminal error");check(failed==(checked_error!=0),"failure status");check(!busy,"terminal not busy");end
endtask
task fetch;
 input[3:0]addr;
 begin @(negedge clk);read_addr=addr;tick;end
endtask
initial begin
 repeat(3)tick;@(negedge clk);rst=0;
 // Adjacent timestamps0/1 exercise synchronous prefetch at full tape rate.
 put(0,rec(0,3,0,0));put(1,rec(1,0,8'h80,3'b000|1));
 put(2,rec(3,0,0,3'b011));put(3,rec(5,2,0,3'b011));put(4,rec(6,4,0,0));
 launch;finish_run(0);
 check(count==3&&executed==5,"limits and executed tape count");
 check(first_clock==513&&last_clock==769,"stride2 clock labels");
 for(i=0;i<3;i=i+1)begin fetch(i);check($signed(read_data[31:16])==257+i*128,"post-edge left capture");check($signed(read_data[15:0])==-(257+i*128),"post-edge right capture");check(read_data[39:32]==(i==0?0:128),"post-edge status capture");end
 saved_first=first_clock;saved_last=last_clock;saved_data=read_data;
 repeat(20)tick;check(first_clock==saved_first&&last_clock==saved_last&&read_data==saved_data,"terminal evidence preserved");
 // Idle shifts absolute schedule; event shift does not move CAPTURE.
 idle_clocks=13;event_shift=19;output_mode=1;stride=1;capture_limit=2;
 launch;finish_run(0);check(first_clock==577&&last_clock==641,"idle and shifted event capture");fetch(0);check($signed(read_data[31:16])==1321,"mode1 post-edge xleft");
 idle_clocks=0;event_shift=0;output_mode=0;stride=2;capture_limit=3;
 put(3,rec(5,1,0,3'b011));launch;finish_run(6);check(count==1,"busy fail preserves prefix");
 put(3,rec(5,2,1,3'b011));launch;finish_run(7);
 put(3,rec(5,2,0,3'b011));put(2,rec(1,0,0,3'b011));launch;finish_run(4);
 put(2,rec(3,0,0,3'b011));put(4,rec(6,4,0,0)|1);launch;finish_run(3);
 put(4,rec(6,4,0,0));stride=0;launch;finish_run(1);stride=2;
 launch;repeat(540)tick;@(negedge clk);abort_run=1;tick;@(negedge clk);abort_run=0;finish_run(2);
 // All remaining malformed-config dimensions preserve explicit failures.
 capture_limit=0;launch;finish_run(1);capture_limit=17;launch;finish_run(1);capture_limit=3;
 length=0;launch;finish_run(1);length=17;launch;finish_run(1);length=5;
 event_first=6;launch;finish_run(1);event_first=1;
 idle_clocks=48'hffffffffffff;launch;finish_run(1);idle_clocks=0;
 put(4,rec(6,0,0,3'b011));launch;finish_run(3);
 put(4,rec(6,4,0,0));put(3,rec(5,3,0,0));launch;finish_run(8);
 put(3,rec(5,2,0,3'b011));put(0,rec(0,0,0,3'b011));launch;finish_run(8);
 put(0,rec(0,3,0,0));put(3,rec(5,0,0,3'b011));put(1,rec(1,0,8'h55,3'b001));launch;finish_run(9);
 put(1,rec(1,0,8'h56,3'b001));launch;finish_run(9);
 put(1,rec(1,0,8'h80,3'b001));launch;repeat(540)tick;@(negedge clk);prog_we=1;tick;@(negedge clk);prog_we=0;finish_run(11);
 // Three consecutive timestamp intervals qualify the lookahead pipeline.
 put(1,rec(1,0,8'h11,3'b011));put(2,rec(2,0,8'h22,3'b011));put(3,rec(3,0,8'h33,3'b011));put(4,rec(4,4,0,0));
 capture_limit=1;launch;
 while(!done)begin tick;if(logical_clock==513)check(debug_din==8'h11,"consecutive first BUS");if(logical_clock==514)check(debug_din==8'h22,"consecutive second BUS");if(logical_clock==515)check(debug_din==8'h33,"consecutive third BUS");end
 finish_run(0);
 // Long decimation performs logical clocks continuously, never cen pauses.
 capture_limit=2;stride=1024;launch;finish_run(0);check(first_clock==513&&last_clock==66049,"stride1024 exact clock labels");
 // Stream pages stop the whole DUT and preserve pending capture/history.
 stride=1;capture_limit=2;length=5;event_first=1;stream_event_first=1;
 stream_enable=1;
 put(0,rec(0,3,0,0));put(1,rec(1,0,8'h80,3'b001));
 put(2,rec(3,0,0,3'b011));put(3,rec(5,2,0,3'b011));put(4,rec(6,5,0,0));
 launch;while(!paused&&!done)tick;
 check(paused&&busy&&!done,"PAGE pauses active run");
 check(executed==5&&total_executed==5&&page_index==0,"first page accounting");
 saved_first=logical_clock;saved_count=count;saved_data={32'd0,last_status};
 repeat(33)tick;
 check(logical_clock==saved_first&&count==saved_count&&last_status==saved_data[7:0],"frozen public time/status");
 put(0,rec(8,2,0,3'b011));put(1,rec(9,4,0,0));length=2;
 check(paused&&logical_clock==saved_first,"RAM upload keeps DUT frozen");
 @(negedge clk);continue_page=1;tick;@(negedge clk);continue_page=0;
 finish_run(0);check(count==2&&executed==2&&total_executed==7&&page_index==1,"continued accounting");
 check(first_clock==513&&last_clock==577,"continued capture cadence");
 // PAGE is rejected by revision1 and must terminate a stream page.
 stream_enable=0;length=2;put(0,rec(0,3,0,0));put(1,rec(1,5,0,0));launch;finish_run(3);
 stream_enable=1;length=3;put(1,rec(1,5,0,0));put(2,rec(3,4,0,0));launch;finish_run(3);
 length=2;put(1,rec(1,5,0,0)|1);launch;finish_run(3);
 put(1,rec(1,5,0,0));launch;while(!paused&&!done)tick;
 length=0;@(negedge clk);continue_page=1;tick;@(negedge clk);continue_page=0;finish_run(1);
 length=2;launch;while(!paused&&!done)tick;
 @(negedge clk);abort_run=1;tick;@(negedge clk);abort_run=0;finish_run(2);
 // A resumed page cannot replay a passed timestamp or reorder history.
 length=2;put(0,rec(0,3,0,0));put(1,rec(1,5,0,0));launch;while(!paused&&!done)tick;
 put(0,rec(2,4,0,0));length=1;
 @(negedge clk);continue_page=1;tick;@(negedge clk);continue_page=0;finish_run(5);
 length=2;put(0,rec(0,3,0,0));put(1,rec(1,5,0,0));launch;while(!paused&&!done)tick;
 put(0,rec(0,4,0,0));length=1;
 @(negedge clk);continue_page=1;tick;@(negedge clk);continue_page=0;finish_run(4);
 // An upload during continuation prefetch is a mutation and cannot qualify.
 length=2;put(0,rec(0,3,0,0));put(1,rec(1,5,0,0));launch;while(!paused&&!done)tick;
 put(0,rec(8,4,0,0));length=1;
 @(negedge clk);continue_page=1;tick;@(negedge clk);continue_page=0;prog_we=1;
 tick;@(negedge clk);prog_we=0;finish_run(11);
 length=2;put(0,rec(0,3,0,0));
 // Continuation while executing is a mutation, never an implicit restart.
 put(1,rec(2,4,0,0));launch;repeat(10)tick;
 @(negedge clk);continue_page=1;tick;@(negedge clk);continue_page=0;finish_run(11);
 $display("PASS %0d directed checks",checks);$finish;
end
endmodule
