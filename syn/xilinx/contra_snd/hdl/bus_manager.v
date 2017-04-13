`timescale 1ns / 1ps

module bus_manager #(parameter RAM_MSB=10)(
//	input		rst50,
//	input		clk50,
//	input		clk_per,
//	input [7:0] cpu_data_out,
	input [7:0] ROM_data_out,
	input [7:0]	RAM_data,
	input [7:0] jt_data_out,
	// Other system elements
	input		game_sel,
	input [7:0]	sound_latch,
	output		clear_irq,
	// CPU control
	output reg [7:0] cpu_data_in,	
	input [15:0]addr,
	input		cpu_vma,
	input		cpu_rw,
	// select signals
	output	reg	RAM_cs,
	output	reg	opm_cs_n
	
);

wire ROM_cs = addr[15];

parameter RAM_START = 16'h6000;
parameter RAM_END = RAM_START+(2**(RAM_MSB+1));
parameter ROM_START=16'h8000;

wire [15:0] ram_start_contra	= 16'h6000;
wire [15:0] ram_end_contra		= 16'h7FFF;
wire [15:0] ram_start_ddragon	= 16'h0000;
wire [15:0] ram_end_ddragon		= 16'h0FFF;

// wire [15:0] rom_start_addr = ROM_START;
// wire [15:0] ym_start_addr	= 16'h2000;
// wire [15:0] ym_end_addr		= 16'h2002;
reg [15:0] irq_clear_addr;

reg LATCH_rd;

//reg	[7:0]	ym_final_d;

always @(*) begin
	if( cpu_rw && cpu_vma)
		casex( {~opm_cs_n, RAM_cs, ROM_cs, LATCH_rd } )
			4'b1XXX: cpu_data_in = jt_data_out;
			4'b01XX: cpu_data_in = RAM_data;
			4'b001X: cpu_data_in = ROM_data_out;
			4'b0001: cpu_data_in = sound_latch;
			default: cpu_data_in = 8'h0;
		endcase
	else
		cpu_data_in = 8'h0;
end

// RAM
wire opm_cs_contra = !addr[15] && !addr[14] && addr[13]; 
wire opm_cs_ddragon= addr>=16'h2800 && addr<=16'h2801;

always @(*)
	if( game_sel ) begin
		RAM_cs	= cpu_vma && (addr>=ram_start_ddragon && addr<=ram_end_ddragon);
		opm_cs_n= !(cpu_vma && opm_cs_ddragon);
		LATCH_rd= cpu_vma && addr==16'h1000; // Sound latch at $1000
		irq_clear_addr = 16'h1000;
	end
	else begin
		RAM_cs 	= cpu_vma && (addr>=ram_start_contra && addr<=ram_end_contra);
		opm_cs_n= !(cpu_vma && opm_cs_contra);
		LATCH_rd= cpu_vma && addr==16'h0; // Sound latch at $0000
		irq_clear_addr = 16'h4000;
	end
	
// Clear IRQ
assign clear_irq = (addr==irq_clear_addr) && cpu_vma ? 1'b1 : 1'b0;
	
endmodule
