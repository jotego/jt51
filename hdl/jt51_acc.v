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
    Version: 1.1 Date: 14- 4-2017
    Version: 1.0 Date: 27-10-2016
    */


module jt51_acc(
    input                   rst,
    input                   clk,
    input                   cen,
    input            [4:0]  cycles,
    input                   m1_enters,
    input                   m2_enters,
    input                   c1_enters,
    input                   c2_enters,
    input                   op31_acc,
    input           [1:0]   rl_I,
    input           [2:0]   con_I,
    input   signed  [13:0]  op_out,
    input                   ne,     // noise enable
    input   signed  [11:0]  noise_mix,
    output  signed  [15:0]  left,
    output  signed  [15:0]  right,
    output  reg signed  [15:0]  xleft,  // exact outputs
    output  reg signed  [15:0]  xright
);

reg signed [13:0] op_val;

always @(*) begin
    if( ne && op31_acc ) // cambiar a OP 31
        op_val = { {2{noise_mix[11]}}, noise_mix };
    else
        op_val = op_out;
end

reg sum_en;

always @(*) begin
    case ( con_I )
        3'd0,3'd1,3'd2,3'd3:    sum_en = m2_enters;
        3'd4:                   sum_en = m1_enters | m2_enters;
        3'd5,3'd6:              sum_en = ~c1_enters;
        3'd7:                   sum_en = 1'b1;
        default:                sum_en = 1'bx;
    endcase
end

wire ren = rl_I[1];
wire len = rl_I[0];
reg signed [17:0] pre_left, pre_right;
reg signed [15:0] left_hold, right_hold;
wire signed [17:0] add_left, add_right;
assign add_left = len && sum_en ? {{4{op_val[13]}},op_val} : 18'sd0;
assign add_right = ren && sum_en ? {{4{op_val[13]}},op_val} : 18'sd0;

function signed [15:0] lim16;
    input signed [17:0] din;
    lim16 = din[17:16]=={2{din[15]}} ? din[15:0] : { din[17], {15{~din[17]}}};
endfunction

always @(posedge clk) begin
    if(rst) begin
        pre_left <= 18'sd0;
        pre_right <= 18'sd0;
        left_hold <= 16'sd0;
        right_hold <= 16'sd0;
        xleft <= 16'sd0;
        xright <= 16'sd0;
    end else if(cen) begin
        if(cycles==5'd31) begin
            left_hold <= lim16(pre_left);
            pre_left <= add_left;
        end else pre_left <= pre_left + add_left;
        if(cycles==5'd15) begin
            right_hold <= lim16(pre_right);
            pre_right <= add_right;
        end else pre_right <= pre_right + add_right;
        if(cycles==5'd16) begin
            xleft <= left_hold;
            xright <= right_hold;
        end
    end
end

wire signed [9:0] left_man, right_man;
wire [2:0] left_exp, right_exp;

jt51_exp2lin left_reconstruct(
    .man( left_man  ),
    .exp( left_exp  ),
    .lin( left      )
);

jt51_exp2lin right_reconstruct(
    .man( right_man ),
    .exp( right_exp ),
    .lin( right     )
);

jt51_lin2exp left2exp(
  .man( left_man ),
  .exp( left_exp ),
  .lin( xleft    )
);

jt51_lin2exp right2exp(
  .man( right_man ),
  .exp( right_exp ),
  .lin( xright    )
);

`ifdef DUMPLEFT

reg skip;

wire signed [15:0] dump = left;

initial skip=1;

always @(posedge clk)
    if( c1_enters && (!skip || dump) && cen) begin
        $display("%d", dump );
        skip <= 0;
    end

`endif

endmodule
