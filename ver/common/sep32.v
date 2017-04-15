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
	Version: 1.1
	Date: 15- 4-2016
	*/

/*

parameter stg is the stage of the pipelined signal
for instance if signal is xx_VIII, then set stg to 8

*/

module sep32 #(parameter width=10, parameter stg=5'd0)
(
	input 	clk,
	input [width-1:0] mixed,
	input [31:0] mask,
	input [4:0] cnt,	
	
	output ch0_slot1,
	output ch1_slot1,
	output ch2_slot1,
	output ch3_slot1,
	output ch4_slot1,
	output ch5_slot1,
	output ch6_slot1,
	output ch7_slot1,

	output ch0_slot2,
	output ch1_slot2,
	output ch2_slot2,
	output ch3_slot2,
	output ch4_slot2,
	output ch5_slot2,
	output ch6_slot2,
	output ch7_slot2,

	output ch0_slot3,
	output ch1_slot3,
	output ch2_slot3,
	output ch3_slot3,
	output ch4_slot3,
	output ch5_slot3,
	output ch6_slot3,
	output ch7_slot3,

	output ch0_slot4,
	output ch1_slot4,
	output ch2_slot4,
	output ch3_slot4,
	output ch4_slot4,
	output ch5_slot4,
	output ch6_slot4,
	output ch7_slot4,
	
	output reg [width-1:0] alland,
	output reg [width-1:0] allor );

reg [4:0] cntadj;

reg [width-1:0] ch0_slot1 /*verilator public*/;
reg [width-1:0] ch1_slot1 /*verilator public*/;
reg [width-1:0] ch2_slot1 /*verilator public*/;
reg [width-1:0] ch3_slot1 /*verilator public*/;
reg [width-1:0] ch4_slot1 /*verilator public*/;
reg [width-1:0] ch5_slot1 /*verilator public*/;
reg [width-1:0] ch6_slot1 /*verilator public*/;
reg [width-1:0] ch7_slot1 /*verilator public*/;

reg [width-1:0] ch0_slot2 /*verilator public*/;
reg [width-1:0] ch1_slot2 /*verilator public*/;
reg [width-1:0] ch2_slot2 /*verilator public*/;
reg [width-1:0] ch3_slot2 /*verilator public*/;
reg [width-1:0] ch4_slot2 /*verilator public*/;
reg [width-1:0] ch5_slot2 /*verilator public*/;
reg [width-1:0] ch6_slot2 /*verilator public*/;
reg [width-1:0] ch7_slot2 /*verilator public*/;

reg [width-1:0] ch0_slot3 /*verilator public*/;
reg [width-1:0] ch1_slot3 /*verilator public*/;
reg [width-1:0] ch2_slot3 /*verilator public*/;
reg [width-1:0] ch3_slot3 /*verilator public*/;
reg [width-1:0] ch4_slot3 /*verilator public*/;
reg [width-1:0] ch5_slot3 /*verilator public*/;
reg [width-1:0] ch6_slot3 /*verilator public*/;
reg [width-1:0] ch7_slot3 /*verilator public*/;

reg [width-1:0] ch0_slot4 /*verilator public*/;
reg [width-1:0] ch1_slot4 /*verilator public*/;
reg [width-1:0] ch2_slot4 /*verilator public*/;
reg [width-1:0] ch3_slot4 /*verilator public*/;
reg [width-1:0] ch4_slot4 /*verilator public*/;
reg [width-1:0] ch5_slot4 /*verilator public*/;
reg [width-1:0] ch6_slot4 /*verilator public*/;
reg [width-1:0] ch7_slot4 /*verilator public*/;

localparam pos0 = 33-stg;

always @(*)
	cntadj = (cnt+pos0)%32;

always @(posedge clk) begin
	case( cntadj )
		5'd0:  ch0_slot1 <= mixed;
		5'd1:  ch1_slot1 <= mixed;
		5'd2:  ch2_slot1 <= mixed;
		5'd3:  ch3_slot1 <= mixed;  		   
		5'd4:  ch4_slot1 <= mixed;
		5'd5:  ch5_slot1 <= mixed;
		5'd6:  ch6_slot1 <= mixed;
		5'd7:  ch7_slot1 <= mixed;
		
		5'd8: ch0_slot3 <= mixed;
		5'h9: ch1_slot3 <= mixed;
		5'ha: ch2_slot3 <= mixed;
		5'hb: ch3_slot3 <= mixed;  		   
		5'hc: ch4_slot3 <= mixed;
		5'hd: ch5_slot3 <= mixed; 
		5'he: ch6_slot3 <= mixed; 
		5'hf: ch7_slot3 <= mixed; 

		5'h10: ch0_slot2 <= mixed;
		5'h11: ch1_slot2 <= mixed;
		5'h12: ch2_slot2 <= mixed;
		5'h13: ch3_slot2 <= mixed;  		   
		5'h14: ch4_slot2 <= mixed;
		5'h15: ch5_slot2 <= mixed;    
		5'h16: ch6_slot2 <= mixed;    
		5'h17: ch7_slot2 <= mixed;    
		
		5'h18: ch0_slot4 <= mixed;
		5'h19: ch1_slot4 <= mixed;
		5'h1a: ch2_slot4 <= mixed;
		5'h1b: ch3_slot4 <= mixed; 			   
		5'h1c: ch4_slot4 <= mixed;
		5'h1d: ch5_slot4 <= mixed; 		   
		5'h1e: ch6_slot4 <= mixed; 		   
		5'h1f: ch7_slot4 <= mixed; 		   
	endcase
	
	alland <= 	({width{~mask[0]}} | ch0_slot1) &
				({width{~mask[1]}} | ch1_slot1) &
				({width{~mask[2]}} | ch2_slot1) &
				({width{~mask[3]}} | ch3_slot1) &
				({width{~mask[4]}} | ch4_slot1) &
				({width{~mask[5]}} | ch5_slot1) &
				({width{~mask[6]}} | ch6_slot1) &
				({width{~mask[7]}} | ch7_slot1) &
				({width{~mask[8]}} | ch0_slot2) &
				({width{~mask[9]}} | ch1_slot2) &
				({width{~mask[10]}} | ch2_slot2) &
				({width{~mask[11]}} | ch3_slot2) &
				({width{~mask[12]}} | ch4_slot2) &
				({width{~mask[13]}} | ch5_slot2) &
				({width{~mask[14]}} | ch6_slot2) &
				({width{~mask[15]}} | ch7_slot2) &
				({width{~mask[16]}} | ch0_slot3) &
				({width{~mask[17]}} | ch1_slot3) &
				({width{~mask[18]}} | ch2_slot3) &
				({width{~mask[19]}} | ch3_slot3) &
				({width{~mask[20]}} | ch4_slot3) &
				({width{~mask[21]}} | ch5_slot3) &
				({width{~mask[22]}} | ch6_slot3) &
				({width{~mask[23]}} | ch7_slot3) &
				({width{~mask[24]}} | ch0_slot4) &
				({width{~mask[25]}} | ch1_slot4) &
				({width{~mask[26]}} | ch2_slot4) &
				({width{~mask[27]}} | ch3_slot4) &
				({width{~mask[28]}} | ch4_slot4) &
				({width{~mask[29]}} | ch5_slot4) &
				({width{~mask[30]}} | ch6_slot4) &
				({width{~mask[31]}} | ch7_slot4);

	allor <= 	({width{~mask[0]}} | ch0_slot1) |
				({width{~mask[1]}} | ch1_slot1) |
				({width{~mask[2]}} | ch2_slot1) |
				({width{~mask[3]}} | ch3_slot1) |
				({width{~mask[4]}} | ch4_slot1) |
				({width{~mask[5]}} | ch5_slot1) |
				({width{~mask[6]}} | ch6_slot1) |
				({width{~mask[7]}} | ch7_slot1) |
				({width{~mask[8]}} | ch0_slot2) |
				({width{~mask[9]}} | ch1_slot2) |
				({width{~mask[10]}} | ch2_slot2) |
				({width{~mask[11]}} | ch3_slot2) |
				({width{~mask[12]}} | ch4_slot2) |
				({width{~mask[13]}} | ch5_slot2) |
				({width{~mask[14]}} | ch6_slot2) |
				({width{~mask[15]}} | ch7_slot2) |
				({width{~mask[16]}} | ch0_slot3) |
				({width{~mask[17]}} | ch1_slot3) |
				({width{~mask[18]}} | ch2_slot3) |
				({width{~mask[19]}} | ch3_slot3) |
				({width{~mask[20]}} | ch4_slot3) |
				({width{~mask[21]}} | ch5_slot3) |
				({width{~mask[22]}} | ch6_slot3) |
				({width{~mask[23]}} | ch7_slot3) |
				({width{~mask[24]}} | ch0_slot4) |
				({width{~mask[25]}} | ch1_slot4) |
				({width{~mask[26]}} | ch2_slot4) |
				({width{~mask[27]}} | ch3_slot4) |
				({width{~mask[28]}} | ch4_slot4) |
				({width{~mask[29]}} | ch5_slot4) |
				({width{~mask[30]}} | ch6_slot4) |
				({width{~mask[31]}} | ch7_slot4);
				
end
	
endmodule
	
