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
    output        load_ack_A,
    output        load_ack_B,
    input         clr_flag_A,
    input         clr_flag_B,
    output        clr_ack_A,
    output        clr_ack_B,
    input         enable_irq_A,
    input         enable_irq_B,
    (*keep*) output        flag_A,
    (*keep*) output        flag_B,
    output        overflow_A,
    (*keep*) output reg    irq_n
);

always @(posedge clk) irq_n <= !(flag_A || flag_B);

jt51_timer #(.BW(6), .CW(10)) timer_A(
    .rst        ( rst       ),
    .clk        ( clk       ), 
    .cen        ( cen       ), 
    .start_value( value_A   ),  
    .load       ( load_A    ),
    .load_ack   ( load_ack_A),
    .clr_flag   ( clr_flag_A),
    .clr_ack    ( clr_ack_A ),
    .flag       ( flag_A    ),
    .flag_enable( enable_irq_A ),
    .overflow   ( overflow_A)
);

jt51_timer #(.BW(10), .CW(8)) timer_B(
    .rst        ( rst       ),
    .clk        ( clk       ), 
    .cen        ( cen       ), 
    .start_value( value_B   ),  
    .load       ( load_B    ),
    .load_ack   ( load_ack_B),
    .clr_flag   ( clr_flag_B),
    .clr_ack    ( clr_ack_B ),
    .flag       ( flag_B    ),
    .flag_enable( enable_irq_B ),
    .overflow   (           )
);

endmodule

module jt51_timer #(parameter CW=10, BW=5 )
(
    input          rst,
    input          clk, 
    input          cen, 
    input [CW-1:0] start_value,
    (*keep*) input          load,
    (*keep*) input          clr_flag,
    (*keep*) output reg     clr_ack,
    (*keep*) output reg     load_ack,
    (*keep*) output reg     flag,
    (*keep*) input          flag_enable,
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

always @(posedge clk, posedge rst) begin : flag_latch
    if( rst ) begin
        clr_ack <= 1'b0;
        flag    <= 1'b0;
    end else begin
        if( clr_flag ) begin
            flag    <= 1'b0;
            clr_ack <= 1'b1;
        end else begin
            clr_ack <= 1'b0;
            if( flag_enable && overflow ) flag<=1'b1;
        end
    end
end

(*keep*) reg cntup;

always @(posedge clk) cntup <= cen;

always @(posedge clk, posedge rst) begin
    if( rst ) begin
        cnt <= {BW+CW{1'b0}};
        overflow <= 1'b0;
        load_ack <= 1'b0;
    end else begin
        if( load ) begin
            cnt      <= cnt0;
            overflow <= 1'b0;
            load_ack <= 1'b1;
        end else begin
            load_ack <= 1'b0;
            {overflow, cnt } <= { 1'b0, cnt } + cntup;
        end
    end
end

endmodule
