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

module jt51_phinc_rom(
	// input				clk,
	input  		[9:0]	keycode,
	output reg	[11:0]	phinc
);

always @(* ) begin : read_lut	
	case( keycode )   
		`include "phinc_lut.vh"
    endcase
end

endmodule
