`timescale 1ns / 1ps

module rom_memory(
    output reg [7:0] dataout,
    input clk,
    input [14:0] addr    
    );

   reg [7:0] mem [32767:0];
   
   initial
      $readmemh("rom.init", mem, 0, 32767);

   always @(posedge clk)
         dataout <= mem[addr];

endmodule
