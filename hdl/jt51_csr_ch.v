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
    Date: 23-10-2019
    */

module jt51_csr_ch(
    input         rst,
    input         clk,
    input         cen,
    input  [ 7:0] din,

    input         up_rl_ch,  
    input         up_fb_ch,  
    input         up_con_ch, 
    input         up_kc_ch,  
    input         up_kf_ch,  
    input         up_ams_ch, 
    input         up_pms_ch, 

    output  [1:0] rl,
    output  [2:0] fb,
    output  [2:0] con,
    output  [6:0] kc,
    output  [5:0] kf,
    output  [1:0] ams,
    output  [2:0] pms
);

wire    [1:0]   rl_in   = din[7:6];
wire    [2:0]   fb_in   = din[5:3];
wire    [2:0]   con_in  = din[2:0];
wire    [6:0]   kc_in   = din[6:0];
wire    [5:0]   kf_in   = din[7:2];
wire    [1:0]   ams_in  = din[1:0];
wire    [2:0]   pms_in  = din[6:4];

wire [25:0] reg_in = {   
        up_rl_ch    ? rl_in     : rl,
        up_fb_ch    ? fb_in     : fb,
        up_con_ch   ? con_in    : con,
        up_kc_ch    ? kc_in     : kc,
        up_kf_ch    ? kf_in     : kf,
        up_ams_ch   ? ams_in    : ams,
        up_pms_ch   ? pms_in    : pms   };

wire [25:0] reg_out;

assign { rl, fb, con, kc, kf, ams, pms  } = reg_out;

jt51_sh #( .width(26), .stages(8)) u_regop(
    .rst    ( rst     ),
    .clk    ( clk     ),
    .cen    ( cen     ),
    .din    ( reg_in  ),
    .drop   ( reg_out )
);


endmodule