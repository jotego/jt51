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


/*

    tab size 4
    
    See xapp052.pdf from Xilinx
    
    The NFRQ formula in the App. Note does not make sense:
    Output rate is 55kHz but for NFRQ=1 the formula states that
    the noise is 111kHz, twice the output rate per channel.
    
    That would suggest that the noise for LEFT and RIGHT are
    different but the rest of the system suggest that LEFT and
    RIGHT outputs are calculated at the same time, based on the
    same OP output.
    
    Also, the block diagram states a 1 bit serial input from
    EG to NOISE and that seems unnecessary too.
    
    I have not been able to measure noise in actual chip because
    operator 31 does not produce any output on my two chips.

*/

module jt51_noise(
    input           rst,
    input           clk,
    input           cen,
    input   [4:0]   nfrq,
    input   [9:0]   eg,
    input           op31_no,
    output  reg [10:0]  out
);


reg         base;
reg [3:0]   cnt;

always @(posedge clk, posedge rst)
    if( rst ) begin
        cnt  <= 4'b0;
    end
    else if(cen) begin
        if( op31_no ) begin
            if ( &cnt ) begin               
                cnt  <= nfrq[4:1]; // we do not need to use nfrq[0]
                // because I run it off P1, YM2151 probably ran off PM
                // but the result is the same, as for NFREQ=31 the YM2151
                // trips the noise output at each output sample, and for
                // NFREQ=0 (or 1), the output trips every 16 samples
                // so NFREQ[0] does not really add resolution
            end
            else cnt <= cnt + 4'b1;
            base <= &cnt;
        end
        else base <= 1'b0;
    end

wire rnd_sign;

always @(posedge clk) if(cen) begin
    if( op31_no )
        out <= { rnd_sign, {10{~rnd_sign}}^eg };
end

jt51_noise_lfsr #(.init(90)) u_lfsr (
    .rst    ( rst      ),
    .clk    ( clk      ),
    .cen    ( cen      ),
    .base   ( base     ),
    .out    ( rnd_sign )
);

endmodule
