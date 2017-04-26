`timescale 1ns / 1ps

module pm_clk_real(
	input             clk,
	input             rst,
	input             real_speed,
	input             rst_counter,
	input             irq_n,			// the pm counter does not count when irq_n is low
	output reg        ym_pm,
	output reg [31:0] pm_counter
);
parameter stop=5'd07;
reg [4:0] div_cnt, cambio0, cambio1; 

always @(posedge clk or posedge rst) begin : speed_mux
	if( rst ) begin
		cambio0 <= 5'd2;
		cambio1 <= 5'd4;
	end
	else begin
		if( real_speed ) begin
			cambio0 <= 5'd2;
			cambio1 <= 5'd4;
		end
		else begin // con 8/16 he visto fallar el STATUS del YM una vez
			cambio0 <= 5'd7;
			cambio1 <= 5'd15;
		end
	end
end

always @(posedge clk or posedge rst) begin : ym_pm_ff
	if( rst ) begin
		div_cnt    <= 5'd0;
		ym_pm      <= 1'b0;
	end
	else begin	
		if(div_cnt>=cambio1) begin // =5'd4 tiempo real del YM
			ym_pm   <= 1'b1;
			div_cnt <= 5'd0;
		end
		else begin
			if( div_cnt==cambio0 ) ym_pm <= 1'b0; // =5'd2 tiempo real
			div_cnt <= div_cnt + 1'b1;
		end
	end
end

reg ultpm;

always @(posedge clk or posedge rst) begin : pm_counter_ff
	if( rst )  begin
		pm_counter <= 32'd0;
		ultpm      <= 1'b0;
	end
	else begin
		ultpm <= ym_pm;
		if(rst_counter) 
			pm_counter <= 32'd0;
		else
			if( irq_n && ym_pm && !ultpm ) 
				pm_counter <= pm_counter + 1'd1;		
	end
end

endmodule
