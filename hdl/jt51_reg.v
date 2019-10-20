/*  This file is part of JT51.

    JT51 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    JT51 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with JT51.  If not, see <http://www.gnu.org/licenses/>.
    
    Author: Jose Tejada Gomez. Twitter: @topapate
    Version: 1.0
    Date: 27-10-2016
    */

`timescale 1ns / 1ps

module jt51_reg(
    input           rst,
    input           clk,
    input           cen,        // P1
    input   [7:0]   d_in,

    input           up_rl,
    input           up_kc,
    input           up_kf,
    input           up_pms,
    input           up_dt1,
    input           up_tl,
    input           up_ks,
    input           up_amsen,
    input           up_dt2,
    input           up_d1l,
    input           up_keyon,
    input   [1:0]   op,     // operator to update
    input   [2:0]   ch,     // channel to update
    
    input           csm,
    input           overflow_A,

    output  reg     busy,
    output  [1:0]   rl_I,
    output  [2:0]   fb_II,
    output  [2:0]   con_I,
    output  [6:0]   kc_I,
    output  [5:0]   kf_I,
    output  [2:0]   pms_I,
    output  [1:0]   ams_VII,
    output  [2:0]   dt1_II,
    output  [3:0]   mul_VI,
    output  [6:0]   tl_VII,
    output  [1:0]   ks_III,
    output          amsen_VII,

    output  [4:0]   arate_II,
    output  [4:0]   rate1_II,
    output  [4:0]   rate2_II,
    output  [3:0]   rrate_II,

    output  [1:0]   dt2_I,
    output  [3:0]   d1l_I,
    output          keyon_II,

    // Pipeline order
    output  reg     zero,
    output  reg     m1_enters,
    output  reg     m2_enters,
    output  reg     c1_enters,
    output  reg     c2_enters,
    // Operator
    output          use_prevprev1,
    output          use_internal_x,
    output          use_internal_y, 
    output          use_prev2,
    output          use_prev1,

    output  [1:0]   cur_op,
    output  reg     op31_no,
    output  reg     op31_acc
);

reg     kon, koff;
reg [1:0] csm_state;
reg [4:0] csm_cnt;

wire csm_kon  = csm_state[0];
wire csm_koff = csm_state[1];

always @(*) begin
    m1_enters = cur_op == 2'b00;
    m2_enters = cur_op == 2'b01;
    c1_enters = cur_op == 2'b10;
    c2_enters = cur_op == 2'b11;
end

wire    [1:0]   rl_in   = d_in[7:6];
wire    [2:0]   fb_in   = d_in[5:3];
wire    [2:0]   con_in  = d_in[2:0];
wire    [6:0]   kc_in   = d_in[6:0];
wire    [5:0]   kf_in   = d_in[7:2];
wire    [2:0]   pms_in  = d_in[6:4];
wire    [1:0]   ams_in  = d_in[1:0];
wire    [2:0]   dt1_in  = d_in[6:4];
wire    [3:0]   mul_in  = d_in[3:0];
wire    [6:0]   tl_in   = d_in[6:0];
wire    [1:0]   ks_in   = d_in[7:6];
wire    [4:0]   ar_in   = d_in[4:0];
wire            amsen_in= d_in[7];
wire    [4:0]   d1r_in  = d_in[4:0];
wire    [1:0]   dt2_in  = d_in[7:6];
wire    [4:0]   d2r_in  = d_in[4:0];
wire    [3:0]   d1l_in  = d_in[7:4];
wire    [3:0]   rr_in   = d_in[3:0];

wire up =   up_rl | up_kc | up_kf | up_pms | up_dt1 | up_tl |
            up_ks | up_amsen | up_dt2 | up_d1l | up_keyon;

reg [4:0]   cur;

always @(posedge clk) if(cen) begin
    op31_no  <= cur == 5'o10;
    op31_acc <= cur == 5'o16;
end

assign cur_op = cur[4:3];

wire [4:0] req_I   = { op, ch };
wire [4:0] req_II  = req_I   + 5'd1;
wire [4:0] req_III = req_II  + 5'd1;
wire [4:0] req_IV  = req_III + 5'd1;
wire [4:0] req_V   = req_IV  + 5'd1;
wire [4:0] req_VI  = req_V   + 5'd1;
wire [4:0] req_VII = req_VI  + 5'd1;


wire    update_op_I     = cur == req_I;
wire    update_op_II    = cur == req_II;
wire    update_op_III   = cur == req_III;
wire    update_op_IV    = cur == req_IV;
wire    update_op_V     = cur == req_V;
wire    update_op_VI    = cur == req_VI;
wire    update_op_VII   = cur == req_VII;

wire up_rl_ch   = up_rl     & update_op_I;
wire up_fb_ch   = up_rl     & update_op_II;
wire up_con_ch  = up_rl     & update_op_I;

wire up_kc_ch   = up_kc     & update_op_I;
wire up_kf_ch   = up_kf     & update_op_I;
wire up_pms_ch  = up_pms    & update_op_I;
wire up_ams_ch  = up_pms    & update_op_VII;

wire up_dt1_op  = up_dt1    & update_op_II; // DT1, MUL
wire up_mul_op  = up_dt1    & update_op_VI; // DT1, MUL
wire up_tl_op   = up_tl     & update_op_VII;
wire up_ks_op   = up_ks     & update_op_III; // KS, AR
wire up_amsen_op= up_amsen  & update_op_VII; // AMS-EN, D1R
wire up_dt2_op  = up_dt2    & update_op_I; // DT2, D2R
wire up_d1l_op  = up_d1l    & update_op_I; // D1L, RR

wire up_ar_op   = up_ks     & update_op_II; // KS, AR
wire up_d1r_op  = up_amsen  & update_op_II; // AMS-EN, D1R
wire up_d2r_op  = up_dt2    & update_op_II; // DT2, D2R
wire up_rr_op   = up_d1l    & update_op_II; // D1L, RR

wire [4:0] next = cur+5'd1;

always @(posedge clk, posedge rst) begin : up_counter
    if( rst ) begin
        cur     <= 5'h0;
        zero    <= 1'b0;
        busy    <= 1'b0;
    end
    else if(cen) begin
        cur     <= next;
        zero    <= next== 5'd0;
        if( &cur ) busy <= up && !busy;
    end
end

wire [2:0] cur_ch =  cur[2:0];
wire [3:0] keyon_op = d_in[6:3];
wire [2:0] keyon_ch = d_in[2:0];

jt51_kon u_kon (
    .rst       (rst       ),
    .clk       (clk       ),
    .cen       (cen       ),
    .keyon_op  (keyon_op  ),
    .keyon_ch  (keyon_ch  ),
    .cur_op    (cur_op    ),
    .cur_ch    (cur_ch    ),
    .up_keyon  (up_keyon && busy ),
    .csm       (csm       ),
    .overflow_A(overflow_A),
    .keyon_II  (keyon_II  )
);


jt51_mod u_mod(
    .alg_I      ( con_I ),
    .m1_enters  ( m1_enters ),
    .m2_enters  ( m2_enters ),
    .c1_enters  ( c1_enters ),
    .c2_enters  ( c2_enters ),
    
    .use_prevprev1 ( use_prevprev1  ),
    .use_internal_x( use_internal_x ),
    .use_internal_y( use_internal_y ),  
    .use_prev2   ( use_prev2      ),
    .use_prev1   ( use_prev1      )
);

// memory for OP registers
localparam opreg_w = 42;
reg  [opreg_w-1:0] reg_op[31:0];
reg  [opreg_w-1:0] reg_out;

assign { dt1_II, mul_VI, tl_VII, ks_III, amsen_VII, 
    dt2_I, d1l_I, arate_II, rate1_II, rate2_II, rrate_II  } = reg_out;

wire [opreg_w-1:0] reg_in = {   
                    up_dt1_op   ? dt1_in    : dt1_II,       // 3
                    up_mul_op   ? mul_in    : mul_VI,       // 4
                    up_tl_op    ? tl_in     : tl_VII,       // 7
                    up_ks_op    ? ks_in     : ks_III,       // 2
                    up_amsen_op ? amsen_in  : amsen_VII,    // 1
                    up_dt2_op   ? dt2_in    : dt2_I,        // 2
                    up_d1l_op   ? d1l_in    : d1l_I,        // 4

                    up_ar_op    ? ar_in     : arate_II,     // 5
                    up_d1r_op   ? d1r_in    : rate1_II,     // 5
                    up_d2r_op   ? d2r_in    : rate2_II,     // 5
                    up_rr_op    ? rr_in     : rrate_II };   // 4

// wire opdata_wr = |{ up_dt1_op, up_mul_op, up_tl_op, up_ks_op, up_amsen_op, 
//  up_dt2_op, up_d1l_op, up_ar_op  , up_d1r_op, up_d2r_op, up_rr_op     };

always @(posedge clk) if(cen) begin
    reg_out     <= reg_op[next];
    if( busy )
        reg_op[cur] <= reg_in;
end

// jt51_sh #( .width(opreg_w), .stages(8)) u_regop(
//     .rst    ( rst     ),
//     .clk    ( clk     ),
//     .cen    ( cen     ),
//     .din    ( reg_in  ),
//     .drop   ( reg_out )
// );
// 

// memory for CH registers
localparam chreg_w = 26;
reg  [chreg_w-1:0] reg_ch[7:0];
reg  [chreg_w-1:0] reg_ch_out;
wire [chreg_w-1:0] reg_ch_in = {
        up_rl_ch    ? rl_in     : rl_I,
        up_fb_ch    ? fb_in     : fb_II,
        up_con_ch   ? con_in    : con_I,
        up_kc_ch    ? kc_in     : kc_I,
        up_kf_ch    ? kf_in     : kf_I,
        up_ams_ch   ? ams_in    : ams_VII,
        up_pms_ch   ? pms_in    : pms_I     };
        
assign { rl_I, fb_II, con_I, kc_I, kf_I, ams_VII, pms_I  } = reg_ch_out;

wire [2:0] next_ch = next[2:0];
// wire chdata_wr = |{ up_rl_ch, up_fb_ch, up_con_ch, up_kc_ch, up_kf_ch, up_ams_ch, up_pms_ch };

always @(posedge clk) if(cen) begin
    reg_ch_out      <= reg_ch[next_ch];
    if( busy )
        reg_ch[cur_ch]  <= reg_ch_in;
end

//////////////////// Debug
`ifndef JT51_NODEBUG
`ifdef SIMULATION
/* verilator lint_off PINMISSING */
wire [4:0] cnt_aux;

sep32_cnt u_sep32_cnt (.clk(clk), .zero(zero), .cnt(cnt_aux));

sep32 #(.width(7),.stg(1)) sep_tl(
    .clk    ( clk           ),
    .mixed  ( tl_VII        ),
    .cnt    ( cnt_aux       )
    );

sep32 #(.width(5),.stg(1)) sep_ar(
    .clk    ( clk           ),
    .mixed  ( arate_II      ),
    .cnt    ( cnt_aux       )
    );


sep32 #(.width(4),.stg(1)) sep_d1l(
    .clk    ( clk           ),
    .mixed  ( d1l_I         ),
    .cnt    ( cnt_aux       )
    );


sep32 #(.width(4),.stg(1)) sep_rr(
    .clk    ( clk           ),
    .mixed  ( rrate_II      ),
    .cnt    ( cnt_aux       )
    );

sep32 #(.width(1),.stg(1)) sep_amsen(
    .clk    ( clk           ),
    .mixed  ( amsen_VII     ),
    .cnt    ( cnt_aux       )
    );

/* verilator lint_on PINMISSING */
`endif
`endif

endmodule
