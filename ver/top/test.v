`timescale 1ns / 1ps

module test;

reg       rst, clk, cen_p1=1'b0;
reg       wr_n;
wire      a0;
reg [7:0] din;
wire [7:0] dout;

initial begin
    rst = 1'b0;
    #300 rst=1'b1;
    #3000 rst=1'b0;
end

initial begin
    clk = 1'b0;
    forever #140 clk=~clk;
end

always @(negedge clk)
    cen_p1 <= ~cen_p1;

wire signed  [15:0] left;
wire signed  [15:0] right;
wire                sample, ct1, ct2, irq_n;

 jt51 uut(
    .rst        (  rst      ),    // reset
    .clk        (  clk      ),    // main clock
    .cen        (  1'b1     ),    // clock enable
    .cen_p1     (  cen_p1   ), // clock enable at half the speed
    .cs_n       (  1'b0     ),   // chip select
    .wr_n       (  wr_n     ),   // write
    .a0         (  a0       ),
    .din        (  din      ), // data in
    .dout       (  dout     ), // data out
    // peripheral control
    .ct1        ( ct1       ),
    .ct2        ( ct2       ),
    .irq_n      ( irq_n     ),  // I do not synchronize this signal
    // Low resolution output (same as real chip)
    .sample     ( sample    ), // marks new output sample
    .left       ( left      ),
    .right      ( right     ),
    // Full resolution output
    .xleft      (           ),
    .xright     (           ),
    // unsigned outputs for sigma delta converters, full resolution
    .dacleft    (           ),
    .dacright   (           )
);

// simulation control
reg [7:0] cmd[0:4096];
integer cnt, waitcnt;

assign a0 = ~cnt[0];
wire   busy = dout[7];

initial begin
    $readmemh( "cmd.hex", cmd);
end

initial begin
    `ifdef NCVERILOG
    $shm_open("test.shm");
    $shm_probe(test,"AS");
    `else
    $dumpfile("test.lxt");
    $dumpvars(0,test);
    $dumpon;
    `endif
end

always @(posedge clk, posedge rst) begin
    if( rst ) begin
        din  <= 8'd0;
        wr_n <= 1'b1;
        cnt  <= 0;
        waitcnt <= 0;
    end else begin
        wr_n <= 1'b1;
        if(!busy ) begin
            waitcnt <= waitcnt-1;
            if(waitcnt==0) begin
                $display("%d, %h, %d", a0, cmd[cnt], waitcnt);
                if( cnt[0]==1'b0 && (cmd[cnt]==0 || cmd[cnt]==1) ) begin
                    if( cmd[cnt]==0) begin
                        $finish;
                    end else begin
                        cnt<=cnt+4; // wait
                        waitcnt<={cmd[cnt+1],~11'h0};                    
                    end
                end
                else begin
                    din <= cmd[cnt];
                    cnt <= cnt+1;
                    wr_n <= 1'b0;
                    waitcnt <= 1;
                end                
            end
        end
    end
end

endmodule