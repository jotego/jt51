`include "build.vh"

module c96_jt51_control #(parameter AW=12, CAP_AW=14)(
    input clk,
    input rst,
    input [31:0] gp_out,
    output reg [31:0] gp_in
);

localparam [47:0] CLOCK_HZ=48'd50000000;
localparam [2:0] IDLE=0, SETTLE=1, EXEC=2, REPLY=3, ACK=4;
localparam [23:0] CAPABILITIES=(CAP_AW<<16)|(AW<<8)|4;

(* altera_attribute = "-name SYNCHRONIZER_IDENTIFICATION FORCED_IF_ASYNCHRONOUS" *)
reg [31:0] request_meta, request_sync;
reg [31:0] command;
reg [2:0] state;
reg [23:0] result;
reg error;
reg [1:0] selected, stage, read_kind;
reg [3:0] mask, ready, start_mask, abort_mask, continue_mask, prog_we;
reg [AW:0] upload_addr;
reg [AW-1:0] write_addr;
reg [CAP_AW:0] sample_addr;
reg [47:0] stage_data;
reg [63:0] prog_data, read_latch;
reg read_valid;
reg [3:0] cfg_index;
reg [AW:0] cfg_length[0:3], cfg_event_first[0:3];
reg [47:0] cfg_idle[0:3];
reg [5:0] cfg_shift[0:3];
reg [CAP_AW:0] cfg_limit[0:3];
reg cfg_mode[0:3], cfg_stream[0:3];
reg [31:0] cfg_stream_first[0:3];
reg [255:0] next_page_hash, previous_page_hash, active_page_hash[0:3];
reg [15:0] next_hash_valid, previous_hash_valid;
reg [23:0] expected_page;
reg [23:0] cfg_stride[0:3], cfg_run_id[0:3], active_run_id[0:3], generation[0:3];
wire [3:0] busy, done, failed, cfg_valid, paused, chain_valid, stream_mask;
wire [63:0] prog_q[0:3];
wire [39:0] read_data[0:3];
wire [7:0] error_code[0:3], last_status[0:3];
wire [CAP_AW:0] count[0:3];
wire [AW:0] executed[0:3];
wire [31:0] total_executed[0:3], page_index[0:3];
wire [47:0] logical_clock[0:3], first_clock[0:3], last_clock[0:3], failure_clock[0:3];
wire [AW-1:0] prog_addr;
wire [255:0] source_hash;
wire [15:0] source_word;
wire any_busy, upload_locked;
integer k;
genvar l;

assign any_busy = |busy;
assign upload_locked = any_busy && (paused & busy)!=busy;
assign prog_addr = |prog_we ? write_addr : upload_addr[AW-1:0];
assign source_hash = `JT51_FPGA_SOURCE_HASH256;
assign source_word = source_hash[{command[3:0],4'b0}+:16];

always @(posedge clk) begin
    request_meta <= gp_out;
    request_sync <= request_meta;
end

always @(posedge clk) begin
    if (rst) begin
        command <= 0;
        state <= IDLE;
        gp_in <= 0;
        result <= 0;
        error <= 0;
        selected <= 0;
        mask <= 4'hf;
        ready <= 0;
        start_mask <= 0;
        abort_mask <= 0;
        continue_mask <= 0;
        prog_we <= 0;
        upload_addr <= 0;
        write_addr <= 0;
        sample_addr <= 0;
        stage_data <= 0;
        stage <= 0;
        prog_data <= 0;
        read_latch <= 0;
        read_valid <= 0;
        read_kind <= 0;
        cfg_index <= 0;
        next_page_hash<=0; previous_page_hash<=0;
        next_hash_valid<=0; previous_hash_valid<=0; expected_page<=0;
        for (k=0;k<4;k=k+1) begin
            cfg_length[k] <= 0;
            cfg_event_first[k] <= 0;
            cfg_idle[k] <= 0;
            cfg_shift[k] <= 0;
            cfg_limit[k] <= 1<<CAP_AW;
            cfg_mode[k] <= 0; cfg_stream[k]<=0; cfg_stream_first[k]<=0; active_page_hash[k]<=0;
            cfg_stride[k] <= 1;
            cfg_run_id[k] <= 0;
            active_run_id[k] <= 0;
            generation[k] <= 0;
        end
    end else begin
        start_mask <= 0;
        abort_mask <= 0;
        continue_mask <= 0;
        prog_we <= 0;
        case (state)
            IDLE: if (request_sync[31]!=gp_in[31]) state <= SETTLE;
            SETTLE: begin
                command <= request_sync;
                state <= EXEC;
            end
            EXEC: begin
                result <= 0;
                error <= 0;
                state <= REPLY;
                if (command[30]) begin
                    error <= 1;
                    result <= 1;
                end else case (command[29:24])
                    0: result <= 24'h965102;
                    1: result <= CAPABILITIES;
                    2: if (command[23:2]!=0) begin
                        error <= 1; result <= 2;
                    end else begin
                        selected <= command[1:0]; stage <= 0; read_valid <= 0;
                    end
                    3: if (upload_locked || command[23:4]!=0 || command[3:0]==0) begin
                        error <= 1; result <= 2;
                    end else begin
                        mask <= command[3:0]; stage <= 0; read_valid <= 0;
                    end
                    4: if (upload_locked || command[23:0]>=(1<<AW)) begin
                        error <= 1; result <= 2;
                    end else begin
                        upload_addr <= command[AW:0]; stage <= 0; read_valid <= 0;
                    end
                    5: if (upload_locked || upload_addr>=(1<<AW)) begin
                        error <= 1; result <= 2;
                    end else begin
                        stage_data[23:0] <= command[23:0]; stage <= 1;
                    end
                    6: if (upload_locked || stage!=1) begin
                        error <= 1; result <= 2;
                    end else begin
                        stage_data[47:24] <= command[23:0]; stage <= 2;
                    end
                    7: if (upload_locked || stage!=2 || command[23:16]!=0 || upload_addr>=(1<<AW)) begin
                        error <= 1; result <= 2;
                    end else begin
                        prog_data <= {command[15:0],stage_data};
                        write_addr <= upload_addr[AW-1:0];
                        prog_we <= mask;
                        upload_addr <= upload_addr+1'b1;
                        ready <= ready & ~mask;
                        next_hash_valid<=0; previous_hash_valid<=0;
                        stage <= 0; read_valid <= 0;
                    end
                    8: if (upload_locked || upload_addr>=(1<<AW)) begin
                        error <= 1; result <= 2;
                    end else begin
                        read_latch <= prog_q[selected];
                        result <= prog_q[selected][23:0];
                        read_valid <= 1; read_kind <= 1;
                    end
                    9: if (!read_valid || read_kind!=1) begin
                        error <= 1; result <= 4;
                    end else result <= read_latch[47:24];
                   10: if (!read_valid || read_kind!=1) begin
                        error <= 1; result <= 4;
                    end else begin
                        result <= {8'd0,read_latch[63:48]};
                        upload_addr <= upload_addr+1'b1; read_valid <= 0;
                    end
                   11: if (upload_locked || command[23:0]!=0 || (cfg_valid & mask)!=mask || stage!=0 ||
                               ((stream_mask & mask)!=0 &&
                                (next_hash_valid!=16'hffff || previous_hash_valid!=16'hffff ||
                                 (!any_busy && (expected_page!=0 || previous_page_hash!=0))))) begin
                        error <= 1; result <= 2;
                    end else ready <= ready | mask;
                   12: if (any_busy || command[23:4]!=0 || command[3:0]==0 ||
                               (ready & command[3:0])!=command[3:0] ||
                               ((stream_mask & command[3:0])!=0 &&
                                (expected_page!=0 || previous_page_hash!=0 ||
                                 next_hash_valid!=16'hffff || previous_hash_valid!=16'hffff))) begin
                        error <= 1; result <= 2;
                    end else begin
                        start_mask <= command[3:0];
                        ready <= ready & ~command[3:0];
                        for (k=0;k<4;k=k+1) if (command[k]) begin
                            active_run_id[k] <= cfg_run_id[k];
                            generation[k] <= generation[k]+1'b1;
                            active_page_hash[k] <= cfg_stream[k] ? next_page_hash : 256'd0;
                        end
                    end
                   13: result <= {8'd0,error_code[selected],4'd0,failed[selected],done[selected],busy[selected],ready[selected]};
                   14: result <= {{(23-CAP_AW){1'b0}},count[selected]};
                   15: if (command[23:4]!=0 || command[3:0]==0) begin
                        error <= 1; result <= 2;
                    end else abort_mask <= command[3:0];
                   16: if (upload_locked || command[23:0]>11) begin
                        error <= 1; result <= 2;
                    end else cfg_index <= command[3:0];
                   17: if (upload_locked || (any_busy && cfg_index>1) ||
                              ((cfg_index==0 || cfg_index==1) && command[23:0]>(1<<AW)) ||
                              (cfg_index==4 && command[23:0]>63) ||
                              (cfg_index==5 && (command[23:0]==0 || command[23:0]>(1<<CAP_AW))) ||
                              (cfg_index==6 && command[23:0]>1) ||
                              (cfg_index==8 && command[23:0]==0) ||
                              (cfg_index==9 && command[23:0]>1) ||
                              (cfg_index==11 && command[23:8]!=0)) begin
                        error <= 1; result <= 2;
                    end else begin
                        ready <= ready & ~mask;
                        for (k=0;k<4;k=k+1) if (mask[k]) case (cfg_index)
                            0: cfg_length[k] <= command[AW:0];
                            1: cfg_event_first[k] <= command[AW:0];
                            2: cfg_idle[k][23:0] <= command[23:0];
                            3: cfg_idle[k][47:24] <= command[23:0];
                            4: cfg_shift[k] <= command[5:0];
                            5: cfg_limit[k] <= command[CAP_AW:0];
                            6: cfg_mode[k] <= command[0];
                            7: cfg_run_id[k] <= command[23:0];
                            8: cfg_stride[k] <= command[23:0];
                            9: cfg_stream[k] <= command[0];
                           10: cfg_stream_first[k][23:0] <= command[23:0];
                           11: cfg_stream_first[k][31:24] <= command[7:0];
                        endcase
                    end
                   18: case (command[23:0])
                        0: result <= {{(23-AW){1'b0}},cfg_length[selected]};
                        1: result <= {{(23-AW){1'b0}},cfg_event_first[selected]};
                        2: result <= cfg_idle[selected][23:0];
                        3: result <= cfg_idle[selected][47:24];
                        4: result <= {18'd0,cfg_shift[selected]};
                        5: result <= {{(23-CAP_AW){1'b0}},cfg_limit[selected]};
                        6: result <= {23'd0,cfg_mode[selected]};
                        7: result <= cfg_run_id[selected];
                        8: result <= cfg_stride[selected];
                        9: result <= {23'd0,cfg_stream[selected]};
                       10: result <= cfg_stream_first[selected][23:0];
                       11: result <= {16'd0,cfg_stream_first[selected][31:24]};
                        default: begin error <= 1; result <= 2; end
                    endcase
                   19: if (busy[selected] || command[23:0]>={{(23-CAP_AW){1'b0}},count[selected]}) begin
                        error <= 1; result <= 2;
                    end else begin
                        sample_addr <= command[CAP_AW:0]; read_valid <= 0;
                    end
                   20: if (busy[selected] || sample_addr>=count[selected]) begin
                        error <= 1; result <= 2;
                    end else begin
                        read_latch <= {24'd0,read_data[selected]};
                        result <= read_data[selected][23:0];
                        read_valid <= 1; read_kind <= 2;
                    end
                   21: if (!read_valid || read_kind!=2) begin
                        error <= 1; result <= 4;
                    end else begin
                        result <= {8'd0,read_latch[39:24]};
                        sample_addr <= sample_addr+1'b1; read_valid <= 0;
                    end
                   22: case (command[23:0])
                        0: result <= {{(23-AW){1'b0}},executed[selected]};
                        1: result <= {16'd0,error_code[selected]};
                        2: result <= logical_clock[selected][23:0];
                        3: result <= logical_clock[selected][47:24];
                        4: result <= first_clock[selected][23:0];
                        5: result <= first_clock[selected][47:24];
                        6: result <= last_clock[selected][23:0];
                        7: result <= last_clock[selected][47:24];
                        8: result <= failure_clock[selected][23:0];
                        9: result <= failure_clock[selected][47:24];
                       10: result <= {16'd0,last_status[selected]};
                       11: result <= CLOCK_HZ[23:0];
                       12: result <= CLOCK_HZ[47:24];
                       13: result <= 1<<CAP_AW;
                       14: result <= 1<<AW;
                       15: result <= {23'd0,cfg_mode[selected]};
                       16: result <= active_run_id[selected];
                       17: result <= generation[selected];
                       18: result <= total_executed[selected][23:0];
                       19: result <= {16'd0,total_executed[selected][31:24]};
                       20: result <= page_index[selected][23:0];
                       21: result <= {16'd0,page_index[selected][31:24]};
                       22: result <= {23'd0,paused[selected]};
                       23: result <= {23'd0,cfg_stream[selected]};
                        default: begin error <= 1; result <= 2; end
                    endcase
                   23: if (command[23:4]!=0) begin
                        error <= 1; result <= 2;
                    end else result <= {8'd0,source_word};
                   24: if (upload_locked) begin
                        error<=1; result<=2;
                    end else begin expected_page<=command[23:0]; ready<=0; end
                   25: if (upload_locked || command[19:16]!=0) begin
                        error<=1; result<=2;
                    end else begin
                        next_page_hash[{command[23:20],4'b0}+:16]<=command[15:0];
                        next_hash_valid[command[23:20]]<=1;
                        ready<=0;
                    end
                   26: if (upload_locked || command[19:16]!=0) begin
                        error<=1; result<=2;
                    end else begin
                        previous_page_hash[{command[23:20],4'b0}+:16]<=command[15:0];
                        previous_hash_valid[command[23:20]]<=1;
                        ready<=0;
                    end
                   27: if (command[23:4]!=0 || command[3:0]==0 || stage!=0 ||
                               (busy & command[3:0])!=command[3:0] ||
                               (paused & command[3:0])!=command[3:0] ||
                               (ready & command[3:0])!=command[3:0] ||
                               (chain_valid & command[3:0])!=command[3:0] ||
                               next_hash_valid!=16'hffff || previous_hash_valid!=16'hffff) begin
                        error<=1; result<=2;
                    end else begin
                        continue_mask<=command[3:0]; ready<=ready & ~command[3:0];
                        for(k=0;k<4;k=k+1) if(command[k]) active_page_hash[k]<=next_page_hash;
                        next_hash_valid<=0; previous_hash_valid<=0;
                    end
                   28: if (command[23:4]!=0) begin error<=1; result<=2; end
                       else result<={8'd0,active_page_hash[selected][{command[3:0],4'b0}+:16]};
                    default: begin error <= 1; result <= 1; end
                endcase
            end
            REPLY: begin
                gp_in[30:0] <= {error,command[29:24],result};
                state <= ACK;
            end
            ACK: begin
                gp_in[31] <= command[31];
                state <= IDLE;
            end
            default: state <= IDLE;
        endcase
    end
end

generate for (l=0;l<4;l=l+1) begin: lanes
    assign cfg_valid[l] = cfg_length[l]!=0 && cfg_length[l]<=(1<<AW) &&
                          cfg_event_first[l]<=cfg_length[l] && cfg_limit[l]!=0 &&
                          cfg_limit[l]<=(1<<CAP_AW) && cfg_stride[l]!=0;
    assign stream_mask[l]=cfg_stream[l];
    assign chain_valid[l]=cfg_stream[l] && page_index[l]+1'b1=={8'd0,expected_page} &&
                          active_page_hash[l]==previous_page_hash;
    jt51_replay_lane #(.AW(AW),.CAP_AW(CAP_AW)) u_lane(
        .clk           ( clk                     ),
        .rst           ( rst                     ),
        .start         ( start_mask[l]           ),
        .abort_run     ( abort_mask[l]           ),
        .stream_enable ( cfg_stream[l]           ),
        .continue_page ( continue_mask[l]        ),
        .stream_event_first ( cfg_stream_first[l] ),
        .paused        ( paused[l]               ),
        .total_executed ( total_executed[l]       ),
        .page_index    ( page_index[l]           ),
        .prog_we       ( prog_we[l]              ),
        .prog_addr     ( prog_addr               ),
        .prog_data     ( prog_data               ),
        .prog_q        ( prog_q[l]               ),
        .length        ( cfg_length[l]           ),
        .event_first   ( cfg_event_first[l]      ),
        .idle_clocks   ( cfg_idle[l]             ),
        .event_shift   ( cfg_shift[l]            ),
        .capture_limit ( cfg_limit[l]            ),
        .output_mode   ( cfg_mode[l]             ),
        .stride        ( cfg_stride[l]           ),
        .read_addr     ( sample_addr[CAP_AW-1:0] ),
        .read_data     ( read_data[l]            ),
        .busy          ( busy[l]                 ),
        .done          ( done[l]                 ),
        .failed        ( failed[l]               ),
        .error_code    ( error_code[l]           ),
        .count         ( count[l]                ),
        .executed      ( executed[l]             ),
        .logical_clock ( logical_clock[l]        ),
        .first_clock   ( first_clock[l]          ),
        .last_clock    ( last_clock[l]           ),
        .failure_clock ( failure_clock[l]        ),
        .last_status   ( last_status[l]          ),
        .debug_cs_n    (                         ),
        .debug_wr_n    (                         ),
        .debug_a0      (                         ),
        .debug_din     (                         ),
        .debug_dut_rst (                         ),
        .debug_sample  (                         )
    );
end endgenerate

endmodule
