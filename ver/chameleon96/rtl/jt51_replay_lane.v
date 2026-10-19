/* SPDX-License-Identifier: GPL-3.0-or-later */
module jt51_replay_lane #(
    parameter AW=12, CAP_AW=14
)(
    input                   clk, rst, start, abort_run,
    input                   prog_we,
    input      [AW-1:0]     prog_addr,
    input      [63:0]       prog_data,
    input      [AW:0]       length, event_first,
    input      [47:0]       idle_clocks,
    input      [ 5:0]       event_shift,
    input      [CAP_AW:0]   capture_limit,
    input      [23:0]       stride,
    input                   output_mode,
    input      [CAP_AW-1:0] read_addr,
    output     [63:0]       prog_q,
    output reg [39:0]       read_data,
    output reg              busy, done, failed,
    output reg [ 7:0]       error_code,
    output reg [CAP_AW:0]   count,
    output reg [AW:0]       executed,
    output reg [47:0]       logical_clock, first_clock, last_clock, failure_clock,
    output reg [ 7:0]       last_status,
    output reg              debug_cs_n, debug_wr_n, debug_a0,
    output reg [ 7:0]       debug_din,
    output                  debug_dut_rst, debug_sample
);

localparam [AW:0] TAPE_CAPACITY={1'b1,{AW{1'b0}}};
localparam [CAP_AW:0] PCM_CAPACITY={1'b1,{CAP_AW{1'b0}}};
localparam [7:0] E_CONFIG=1, E_ABORT=2, E_FORMAT=3, E_ORDER=4,
    E_MISSED=5, E_BUSY=6, E_STATUS=7, E_CAPTURE=8, E_CADENCE=9,
    E_OVERFLOW=10, E_MUTATION=11, E_CLOCK=12;

wire [63:0] tape_q;
wire [AW-1:0] tape_addr;
wire tape_we, unused_zero;
wire [15:0] left, right, xleft, xright;
wire [ 7:0] status;
wire sample, cen, cen_p1;
wire [49:0] target;
wire [47:0] next_clock, stored_gap;
wire [ 7:0] pending_mask;
wire due, prepare, emit_sample, native_sample, capture_we;
wire [CAP_AW-1:0] capture_addr;
wire [39:0] capture_data;
reg [AW:0] run_length, run_event_first, record_index, fetch_index;
reg [CAP_AW:0] run_limit;
reg [47:0] init_clock, previous_timestamp, pending_clock, capture_arm_clock;
reg [47:0] last_native_clock;
reg [63:0] record, following;
reg [23:0] run_stride, native_phase;
reg [ 5:0] run_shift;
reg [ 3:0] pending_opcode;
reg [ 7:0] pending_data;
reg [ 2:0] pending_bits;
reg [ 1:0] fill;
reg record_valid, pending, recording, ended, draining, dut_rst;
reg capture_pending, had_native, run_mode;
reg [ 7:0] fault;
reg [39:0] capture_ram [0:(1<<CAP_AW)-1];

assign unused_zero = 1'b0;
assign cen         = 1'b1;
assign cen_p1      = logical_clock[0];
assign debug_dut_rst = dut_rst;
assign debug_sample = sample;
assign next_clock  = logical_clock+48'd1;
assign target      = {2'b0,init_clock}+{2'b0,record[63:16]}+
                     (record_index>=run_event_first ? {44'd0,run_shift} : 50'd0);
assign due         = busy && record_valid && target=={2'b0,next_clock};
assign prepare     = due && fault==0;
// Look ahead on the SAME read edge as consumption. Together with two front
// registers this supports adjacent timestamps without asynchronous RAM reads.
assign tape_addr   = fill==0 ? {AW{1'b0}} :
                     fill==1 ? {{(AW-1){1'b0}},1'b1} :
                     fill==2 ? {{(AW-2){1'b0}},2'd2} :
                     prepare && fetch_index+1'b1<run_length ?
                         fetch_index[AW-1:0]+1'b1 : fetch_index[AW-1:0];
assign tape_we     = prog_we && !busy;
assign pending_mask = {pending_bits[2],5'b00000,pending_bits[1:0]};
assign native_sample = busy && recording && sample && count<run_limit;
assign emit_sample = native_sample && native_phase==0 && fault==0;
assign stored_gap  = {18'd0,run_stride,6'd0};
assign capture_we  = busy && capture_pending && count<run_limit && count<PCM_CAPACITY;
assign capture_addr = count[CAP_AW-1:0];
assign capture_data = run_mode ? {status,xleft,xright} : {status,left,right};

// A distinct synchronous read port preserves completed evidence while HPS
// accesses it; neither the large capture memory nor the tape has async reads.
always @(posedge clk) begin
    if (capture_we) capture_ram[capture_addr] <= capture_data;
    read_data <= capture_ram[read_addr];
end

always @* begin
    fault=0;
    if (busy) begin
        if (abort_run) fault=E_ABORT;
        else if (start || prog_we) fault=E_MUTATION;
        else if (&logical_clock) fault=E_CLOCK;
        else if (pending && pending_opcode==1 && status[7]) fault=E_BUSY;
        else if (pending && pending_opcode==2 &&
                 (status & pending_mask)!=(pending_data & pending_mask)) fault=E_STATUS;
        else if (record_valid && (record[0] || record[15:12]>4 ||
                 (record[15:12]>=3 && record[11:0]!=0) ||
                 (record[15:12]==4 && record_index+1'b1!=run_length) ||
                 (record_index+1'b1==run_length && record[15:12]!=4))) fault=E_FORMAT;
        else if (record_valid && (target[49:48]!=0 ||
                 (record_index!=0 && record[63:16]<=previous_timestamp))) fault=E_ORDER;
        else if (record_valid && target<={2'b0,logical_clock}) fault=E_MISSED;
        else if (due && record[15:12]==3 &&
                 (recording || record_index>=run_event_first)) fault=E_CAPTURE;
        else if (pending && pending_opcode==4 && !recording) fault=E_CAPTURE;
        else if (capture_pending && count>=run_limit) fault=E_OVERFLOW;
        else if (native_sample && had_native && next_clock-last_native_clock!=64) fault=E_CADENCE;
        else if (native_sample && native_phase==0 && count!=0 &&
                 next_clock-last_clock!=stored_gap) fault=E_CADENCE;
        else if (busy && recording && count<run_limit && !sample &&
                 (had_native ? next_clock-last_native_clock>64 :
                               next_clock-capture_arm_clock>64)) fault=E_CADENCE;
    end
end

always @(posedge clk) begin
    if (rst) begin
        busy<=0; done<=0; failed<=0; error_code<=0;
        count<=0; executed<=0; logical_clock<=0;
        first_clock<=0; last_clock<=0; failure_clock<=0; last_status<=0;
        record_valid<=0; pending<=0; recording<=0; ended<=0; draining<=0;
        capture_pending<=0; had_native<=0; dut_rst<=1;
        fill<=0; fetch_index<=0; record_index<=0;
        debug_cs_n<=0; debug_wr_n<=1; debug_a0<=0; debug_din<=0;
        run_length<=0; run_event_first<=0; run_limit<=0; init_clock<=0;
        run_stride<=1; native_phase<=0; run_shift<=0; run_mode<=0;
        previous_timestamp<=0; pending_clock<=0; capture_arm_clock<=0;
        last_native_clock<=0; pending_opcode<=0; pending_data<=0; pending_bits<=0;
        record<=0; following<=0;
    end else if (start && !busy) begin
        count<=0; executed<=0; logical_clock<=0;
        first_clock<=0; last_clock<=0; failure_clock<=0; last_status<=0;
        record_valid<=0; pending<=0; recording<=0; ended<=0; draining<=0;
        capture_pending<=0; had_native<=0; dut_rst<=1;
        fill<=0; fetch_index<=0; record_index<=0; previous_timestamp<=0;
        debug_cs_n<=0; debug_wr_n<=1; debug_a0<=0; debug_din<=0;
        run_length<=length; run_event_first<=event_first; run_limit<=capture_limit;
        run_stride<=stride; native_phase<=0; run_shift<=event_shift; run_mode<=output_mode;
        init_clock<=idle_clocks+48'd512;
        failed<=0; error_code<=0; done<=0; busy<=1;
        if (length==0 || length>TAPE_CAPACITY || event_first>length ||
            capture_limit==0 || capture_limit>PCM_CAPACITY || stride==0 ||
            idle_clocks>48'hfffffffffdff) begin
            busy<=0; done<=1; failed<=1; error_code<=E_CONFIG;
        end
    end else if (busy) begin
        logical_clock<=next_clock;
        last_status<=status;
        pending<=0;
        capture_pending<=0;
        if (logical_clock==255) dut_rst<=0;
        // Both front registers are primed during the fixed reset interval.
        if (fill==0) fill<=1;
        if (fill==1) begin record<=tape_q; fill<=2; fetch_index<=1; end
        if (fill==2) begin following<=tape_q; fill<=3; fetch_index<=2; record_valid<=1; end
        if (pending) begin
            executed<=executed+1'b1;
            if (pending_opcode==4) ended<=1;
        end
        if (capture_we) begin
            if (count==0) first_clock<=pending_clock;
            last_clock<=pending_clock;
            count<=count+1'b1;
        end
        if (native_sample) begin
            last_native_clock<=next_clock;
            had_native<=1;
            native_phase<=native_phase+1'b1==run_stride ? 24'd0 : native_phase+1'b1;
        end
        if (emit_sample) begin
            capture_pending<=1;
            pending_clock<=next_clock;
        end
        if (prepare) begin
            pending<=1; pending_opcode<=record[15:12];
            pending_data<=record[11:4]; pending_bits<=record[3:1];
            previous_timestamp<=record[63:16];
            record_index<=record_index+1'b1;
            record_valid<=record_index+1'b1<run_length;
            record<=following; following<=tape_q;
            if (fetch_index+1'b1<run_length) fetch_index<=fetch_index+1'b1;
            if (record[15:12]==0 || record[15:12]==1) begin
                debug_cs_n<=record[3]; debug_wr_n<=record[2];
                debug_a0<=record[1]; debug_din<=record[11:4];
            end
            if (record[15:12]==2) begin
                debug_cs_n<=0; debug_wr_n<=1; debug_a0<=1;
            end
            if (record[15:12]==3) begin
                recording<=1; native_phase<=0; had_native<=0;
                capture_arm_clock<=target[47:0];
            end
        end
        if (ended && count==run_limit && !capture_pending) draining<=1;
        if (draining) begin busy<=0; done<=1; dut_rst<=1; end
        if (fault!=0) begin
            busy<=0; done<=1; failed<=1; error_code<=fault;
            failure_clock<=logical_clock; dut_rst<=1;
            pending<=0; record_valid<=0; capture_pending<=0;
        end
    end
end

jtframe_dual_ram #(.DW(64),.AW(AW)) u_tape (
    .clk0  ( clk         ),
    .data0 ( prog_data   ),
    .addr0 ( prog_addr   ),
    .we0   ( tape_we     ),
    .q0    ( prog_q      ),
    .clk1  ( clk         ),
    .data1 ( 64'd0       ),
    .addr1 ( tape_addr   ),
    .we1   ( unused_zero ),
    .q1    ( tape_q      )
);

jt51 u_jt51 (
    .clk    ( clk         ),
    .rst    ( dut_rst     ),
    .cen    ( cen         ),
    .cen_p1 ( cen_p1      ),
    .cs_n   ( debug_cs_n  ),
    .wr_n   ( debug_wr_n  ),
    .a0     ( debug_a0    ),
    .din    ( debug_din   ),
    .dout   ( status      ),
    .irq_n  (             ),
    .ct1    (             ),
    .ct2    (             ),
    .sample ( sample      ),
    .left   ( left        ),
    .right  ( right       ),
    .xleft  ( xleft       ),
    .xright ( xright      )
);
endmodule
