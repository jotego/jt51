`timescale 1ns/1ps
module control_tb;
reg clk=0;
always #5 clk=~clk;
reg rst=1;
reg [31:0] gp_out=0;
wire [31:0] gp_in;
reg toggle=0;
reg [23:0] answer;
reg [63:0] tape[0:5];
reg [63:0] word;
reg [39:0] expected[0:3];
integer checks=0, lane, i, cycles;
c96_jt51_control #(.AW(4),.CAP_AW(3)) dut(.clk(clk),.rst(rst),.gp_out(gp_out),.gp_in(gp_in));

task check;
input condition;
input [255:0] label_text;
begin
    if (!condition) $fatal(1,"control check failed: %s",label_text);
    checks=checks+1;
end
endtask

task cmd;
input [5:0] op;
input [23:0] arg;
input expect_error;
integer n;
begin
    @(negedge clk);
    toggle=~toggle;
    gp_out={toggle,1'b0,op,arg};
    n=0;
    while (gp_in[31]!=toggle && n<100) begin @(negedge clk); n=n+1; end
    check(n<100,"ack timeout");
    check(gp_in[29:24]==op,"reply opcode");
    check(gp_in[30]==expect_error,"reply error");
    answer=gp_in[23:0];
end
endtask

task config_field;
input [23:0] field_index,value;
begin cmd(16,field_index,0);cmd(17,value,0);cmd(18,field_index,0);check(answer==value,"config readback");end
endtask

task page_hashes;
input [15:0] next_low,previous_low;
integer h;
begin
    for(h=0;h<16;h=h+1)begin
        cmd(25,{h[3:0],4'd0,(h==0?next_low:16'd0)},0);
        cmd(26,{h[3:0],4'd0,(h==0?previous_low:16'd0)},0);
    end
end
endtask

initial begin
    repeat(5) @(negedge clk);
    rst=0;
    cmd(0,0,0);check(answer==24'h965102,"protocol");
    cmd(1,0,0);check(answer==24'h030404,"capacities");
    cmd(7,0,1);
    cmd(2,4,1);
    cmd(3,15,0);cmd(4,0,0);
    tape[0]=(64'd0<<16)|(64'd3<<12);
    tape[1]=(64'd2<<16)|(64'd3<<4)|(64'd1<<1);
    tape[2]=(64'd4<<16)|(64'd3<<4)|(64'd3<<1);
    tape[3]=(64'd8<<16)|(64'd1<<12)|(64'd3<<4)|(64'd3<<1);
    tape[4]=(64'd10<<16)|(64'd2<<12)|(64'd3<<4)|(64'd3<<1);
    tape[5]=(64'd12<<16)|(64'd4<<12);
    for(i=0;i<6;i=i+1) begin
        cmd(5,tape[i][23:0],0);cmd(6,tape[i][47:24],0);cmd(7,{8'd0,tape[i][63:48]},0);
    end
    for(lane=0;lane<4;lane=lane+1) begin
        cmd(2,lane[23:0],0);cmd(4,0,0);
        for(i=0;i<6;i=i+1) begin
            cmd(8,0,0);word[23:0]=answer;
            cmd(9,0,0);word[47:24]=answer;
            cmd(10,0,0);word[63:48]=answer[15:0];check(word==tape[i],"four tape replicas");
        end
    end
    cmd(3,15,0);
    config_field(0,6);config_field(1,1);config_field(2,0);config_field(3,0);
    config_field(4,0);config_field(5,4);config_field(6,0);config_field(7,77);config_field(8,1);
    cmd(11,0,0);cmd(12,15,0);cmd(5,0,1);
    cycles=0;
    cmd(13,0,0);
    while(answer[1] && cycles<2048) begin cmd(13,0,0);cycles=cycles+1;end
    check(cycles<2048,"bounded completion");
    for(lane=0;lane<4;lane=lane+1) begin
        cmd(2,lane[23:0],0);cmd(13,0,0);check(answer[3:0]==4,"terminal DONE");
        cmd(14,0,0);check(answer==4,"sample count");
        cmd(22,0,0);check(answer==6,"all tape actions");
        cmd(22,16,0);check(answer==77,"latched run ID");
        cmd(22,17,0);check(answer==1,"generation");
        cmd(19,0,0);
        for(i=0;i<4;i=i+1) begin
            cmd(20,0,0);word[23:0]=answer;
            cmd(21,0,0);word[39:24]=answer[15:0];
            if(lane==0) expected[i]=word[39:0];
            else check(word[39:0]==expected[i],"four lane PCM equality");
            check(word[31:16]!=0 && word[15:0]!=0,"nonzero signed PCM");
        end
        cmd(20,0,1);
    end
    cmd(3,15,0);config_field(7,78);cmd(11,0,0);cmd(12,15,0);cmd(15,15,0);
    repeat(4) @(negedge clk);
    for(lane=0;lane<4;lane=lane+1) begin
        cmd(2,lane[23:0],0);cmd(13,0,0);check(answer[3] && !answer[1],"abort retained failure");
        cmd(22,1,0);check(answer==2,"abort code");
        cmd(22,16,0);check(answer==78,"abort actual ID");
    end
    cmd(21,0,1);
    cmd(16,8,0);cmd(17,0,1);
    // Strict chained stream: all four DUT clocks stop while RAM is replaced.
    cmd(3,15,0);cmd(4,0,0);
    tape[0]=(64'd0<<16)|(64'd3<<12);
    tape[1]=(64'd2<<16)|(64'd3<<1);
    tape[2]=(64'd20<<16)|(64'd5<<12);
    for(i=0;i<3;i=i+1)begin
        cmd(5,tape[i][23:0],0);cmd(6,tape[i][47:24],0);cmd(7,{8'd0,tape[i][63:48]},0);
    end
    config_field(0,3);config_field(1,3);config_field(7,99);
    config_field(9,1);config_field(10,4);config_field(11,0);
    cmd(11,0,1); // missing first page identity cannot START
    cmd(24,0,0);page_hashes(16'h11,0);cmd(11,0,0);cmd(12,15,0);
    cycles=0;cmd(22,22,0);
    while(answer!=1&&cycles<2048)begin cmd(22,22,0);cycles=cycles+1;end
    check(cycles<2048,"bounded page pause");
    for(lane=0;lane<4;lane=lane+1)begin
        cmd(2,lane[23:0],0);cmd(22,22,0);check(answer==1,"all lanes paused");
        cmd(22,2,0);check(answer==533,"pause logical clock");
        cmd(22,18,0);check(answer==3,"first page total execution");
        cmd(28,0,0);check(answer==17,"latched initial page digest");
    end
    // Config controls affecting DUT/epoch are immutable during a pause.
    cmd(16,2,0);cmd(17,7,1);
    cmd(3,15,0);cmd(4,0,0);
    tape[0]=(64'd22<<16)|(64'd2<<12)|(64'd3<<1);
    tape[1]=(64'd24<<16)|(64'd4<<12);
    for(i=0;i<2;i=i+1)begin
        cmd(5,tape[i][23:0],0);cmd(6,tape[i][47:24],0);cmd(7,{8'd0,tape[i][63:48]},0);
    end
    config_field(0,2);config_field(1,1);
    cmd(24,2,0);page_hashes(16'h22,16'h11);cmd(11,0,0);cmd(27,15,1);
    cmd(24,1,0);page_hashes(16'h22,0);cmd(11,0,0);cmd(27,15,1);
    page_hashes(16'h22,16'h11);cmd(11,0,0);
    for(lane=0;lane<4;lane=lane+1)begin
        cmd(2,lane[23:0],0);cmd(22,2,0);check(answer==533,"RAM and hash upload freeze logical clock");
        cmd(22,16,0);check(answer==99,"page loading preserves run ID");
        cmd(22,18,0);check(answer==3,"page loading preserves execution count");
    end
    // Global identity staging invalidates readiness on EVERY lane, even
    // when the selected upload/config mask contains only another lane.
    cmd(3,1,0);cmd(25,24'h000033,0);cmd(27,14,1);
    cmd(3,15,0);page_hashes(16'h22,16'h11);cmd(11,0,0);
    cmd(27,15,0);cmd(27,15,1);
    cycles=0;cmd(13,0,0);
    while(answer[1]&&cycles<2048)begin cmd(13,0,0);cycles=cycles+1;end
    check(cycles<2048,"bounded continued completion");
    for(lane=0;lane<4;lane=lane+1)begin
        cmd(2,lane[23:0],0);cmd(13,0,0);check(answer[3:0]==4,"continued DONE");
        cmd(22,0,0);check(answer==2,"final page execution count");
        cmd(22,18,0);check(answer==5,"complete stream execution count");
        cmd(22,20,0);check(answer==1,"final page index");
        cmd(22,16,0);check(answer==99,"continued run ID");
        cmd(28,0,0);check(answer==34,"latched final page digest");
    end
    $display("PASS controller %0d checks",checks);
    $finish;
end
initial begin #1000000; $fatal(1,"control timeout");end
endmodule
