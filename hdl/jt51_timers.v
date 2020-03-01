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

module jt51_timers(
    input         rst,
    input         clk,
    input         cen,
    input         zero,
    input [9:0]   value_A,
    input [7:0]   value_B,
    input         load_A,
    input         load_B,
    input         clr_flag_A,
    input         clr_flag_B,
    input         enable_irq_A,
    input         enable_irq_B,
    output        flag_A,
    output        flag_B,
    output        overflow_A,
    output        irq_n
);

assign irq_n = ~( (flag_A&enable_irq_A) | (flag_B&enable_irq_B) );

jt51_timer #(.counter_width(10)) timer_A(
    .rst        ( rst       ),
    .clk        ( clk       ), 
    .cen        ( cen       ), 
    .zero       ( zero      ),
    .start_value( value_A   ),  
    .load       ( load_A    ),
    .clr_flag   ( clr_flag_A),
    .flag       ( flag_A    ),
    .overflow   ( overflow_A)
);

jt51_timer #(.counter_width(12)) timer_B(
    .rst        ( rst           ),
    .clk        ( clk           ), 
    .cen        ( cen           ), 
    .zero       ( zero          ),
    .start_value( {value_B,4'b0}),  
    .load       ( load_B        ),
    .clr_flag   ( clr_flag_B    ),
    .flag       ( flag_B        ),
    .overflow   (               )
);

endmodule

module jt51_timer #(parameter counter_width = 10 )
(
    input   rst,
    input   clk, 
    input   cen, 
    input   zero, 
    input   [counter_width-1:0] start_value,
    input   load,
    input   clr_flag,
    output reg flag,
    output reg overflow
);

reg last_load;
reg [counter_width-1:0] cnt, next;

always@(posedge clk, posedge rst)
    if( rst )
        flag <= 1'b0;
    else /*if(cen)*/ begin
        if( clr_flag )
            flag <= 1'b0;
        else if(overflow) flag<=1'b1;
    end

always @(*) begin
    {overflow, next } = { 1'b0, cnt } + 1'b1;
end

always @(posedge clk) if(cen && zero) begin : counter
    last_load <= load;
    if( (load && !last_load) || overflow ) begin
      cnt  <= start_value;
    end     
    else if( last_load ) cnt <= next;
end
endmodule
