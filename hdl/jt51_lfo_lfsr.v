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

module jt51_lfo_lfsr #(parameter init=220 )(
    input   rst,
    input   clk,
    input   cen,
    input   base,
    output  out
);

reg [18:0] bb;
assign out = bb[18];

reg last_base;

always @(posedge clk, posedge rst) begin : base_counter
    if( rst ) begin
        bb          <= init[18:0];
        last_base   <= 1'b0;
    end
    else if(cen) begin
        last_base <= base;
        if( last_base != base ) begin   
            bb[18:1]    <= bb[17:0];
            bb[0]       <= ^{bb[0],bb[1],bb[14],bb[15],bb[17],bb[18]};
        end
    end
end

endmodule
