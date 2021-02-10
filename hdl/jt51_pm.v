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


module jt51_pm(
    input      [ 6:0] kc,
    input      [ 5:0] kf,
    input      [ 7:0] pm,
    input      [ 2:0] pms, // 0=insensitive, 7=most sensitive
    input      [ 1:0] dt,  // detune 2
    output reg [12:0] kcex
);

wire       pms7 = &pms;
reg  [3:0] pm_msb;
reg  [4:0] pm_msb2;
reg  [7:0] mod;
reg        t;
reg  [9:0] scaled;

always @(*) begin
    pm_msb  = {1'b0,pm[6:4]} >> ~pms7;
    t       = &pm_msb[2:1] || (pms>=3'd6 && &pm_msb[1:0]);
    pm_msb2 = {pm_msb, pm[4] } + { 3'd0, pm_msb[2], 1'b0 } + {3'd0, t, 1'b0};
    mod = { pms7 ? pm_msb2[4:1] : pm_msb2[3:0], pm[3:0] };
    case( pms )
        3'd0: scaled = 10'd0;
        3'd1: scaled = { 8'd0, mod[6:5] };
        3'd2: scaled = { 7'd0, mod[6:4] };
        3'd3: scaled = { 6'd0, mod[6:3] };
        3'd4: scaled = { 5'd0, mod[6:2] };
        3'd5: scaled = { 4'd0, mod[6:1] };
        3'd6: scaled = { 1'd0, mod, 1'd0};
        3'd7: scaled = {       mod, 2'd0};
    endcase
end

// Keycode modification
wire        lfo_sign = !(pm[7] && pms!=3'd0);
reg  [12:0] ext;
reg  [ 7:0] lower_sum;
reg  [ 4:0] upper_sum;
wire [12:0] kcf = { kc, kf };
reg  [13:0] pre;
reg         cr;
reg         ov1, ov2;
reg         negov;
reg  [12:0] kcpm;

always @(*) begin
    ext = { 3'd0, scaled} ^ {13{~lfo_sign}};
    {cr, lower_sum } = { 1'b0, kcf[7:0] } + {1'b0, ext[7:0] } + {8'd0, ~lfo_sign};
    {ov1, upper_sum} = { 1'b0, kcf[12:8]} + {1'b0, ext[12:8]} + {5'd0, cr};
    pre = { 1'd0, upper_sum, lower_sum };
    negov = 0;
    if( lfo_sign ) begin
        if (pre[7:6]==2'b11 || cr) begin
            //$display("before +64: pre=%X",pre);
            pre = pre + 14'd64;
        end
    end else begin
        if( !cr ) begin
            pre = pre+14'h1fc0;
            negov = 1;
        end
    end
    { ov2, kcpm } = pre;
    if( (!lfo_sign && !ov1) || (negov && !ov2) ) begin
        kcpm = 13'd0;
    end
    if( lfo_sign && (ov1 || ov2) ) begin
        kcpm = 13'd8127;
    end
end

// Apply dt2
reg [5:0] t2;
reg [7:0] t3;
reg       w2, w3, w6;
reg       b0, b1, b3;

always @(*) begin
    kcex = kcpm;
    b1 = dt==2'd2;
    {b0, t2 } = {1'b0, kcpm[5:0]} + { 1'd0, dt[1], b1, 1'b0, b1, 2'd0};
    b3 = kcpm[7];
    w2 = b0 & b1 & kcpm[6];
    w3 = b0 & b3;
    w6 = (b0 & ~w2 & ~w3) | (b3 & ~b0 & b1);
    t3 = {1'd0, kcpm[12:6] } + {7'd0, w6 } + { 4'd0, dt!=2'd0, dt==2'd3, w2|w3, b1 };
    kcex = t3[7] ? {7'd126,6'd63} : { t3[6:0], t2 };
end

endmodule
