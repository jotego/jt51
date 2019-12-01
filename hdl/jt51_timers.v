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
    (*direct_enable *) input cen,
    input [9:0]   value_A,
    input [7:0]   value_B,
    input         load_A,
    input         load_B,
    input         clr_flag_A,
    input         clr_flag_B,
    input         set_run_A,
    input         set_run_B,  
    input         clr_run_A,
    input         clr_run_B,  
    input         enable_irq_A,
    input         enable_irq_B,
    (*keep*) output        flag_A,
    (*keep*) output        flag_B,
    output        overflow_A,
    (*keep*) output        irq_n
);

assign irq_n = ~( (flag_A&enable_irq_A) | (flag_B&enable_irq_B) );

jt51_timer #(.BW(6), .CW(10)) timer_A(
    .rst        ( rst       ),
    .clk        ( clk       ), 
    .cen        ( cen       ), 
    .start_value( value_A   ),  
    .load       ( load_A    ),
    .clr_flag   ( clr_flag_A),
    .set_run    ( set_run_A ),
    .clr_run    ( clr_run_A ),
    .flag       ( flag_A    ),
    .overflow   ( overflow_A)
);

jt51_timer #(.BW(10), .CW(8)) timer_B(
    .rst        ( rst       ),
    .clk        ( clk       ), 
    .cen        ( cen       ), 
    .start_value( value_B   ),  
    .load       ( load_B    ),
    .clr_flag   ( clr_flag_B),
    .set_run    ( set_run_B ),
    .clr_run    ( clr_run_B ),
    .flag       ( flag_B    ),
    .overflow   (           )
);

endmodule

module jt51_timer #(parameter CW=10, BW=5 )
(
    input   rst,
    input   clk, 
    (*direct_enable *) input cen, 
    input   [CW-1:0] start_value,
    input   load,
    input   clr_flag,
    input   set_run,
    input   clr_run,
    output reg flag,
    (*keep*) output reg overflow
);

(*keep*) reg [BW+CW-1:0] cnt;
reg  last_load, last_clr;
wire posedge_load = load && !last_load;
wire posedge_clr  = clr_flag  && !last_clr;
wire [BW+CW-1:0] cnt0 = { start_value, {BW{1'b0}}};

always @(posedge clk) begin : edges
    last_load <= load;    
    last_clr  <= clr_flag;
end

always @(posedge clk) begin : flag_latch
    if( clr_flag )
        flag <= 1'b0;
    else if( overflow && cen) flag<=1'b1;
end

always @(posedge clk) begin
    if( load )
        cnt <= cnt0;
    else if(cen) begin
        overflow <= &cnt;
        cnt <= cnt + 1'b1;
    end
end

endmodule
