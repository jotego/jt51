	// Inputs
	reg clk;
	reg rst;
	reg cs_n;
	reg wr_n;
	reg a0;
	reg [7:0] d_in;

	// Outputs
	wire [7:0] d_out;
	wire ct1;
	wire ct2;
	wire irq_n;
	wire p1;
	wire signed [15:0] left, right, xleft, xright, dacleft, dacright;
    wire sample;


	jt51 uut (
		.clk(clk),
		.rst(rst),
		.cs_n(cs_n),
		.wr_n(wr_n),
		.a0(a0),
		.d_in(d_in),
		.d_out(d_out),
		.ct1(ct1),
		.ct2(ct2),
		.irq_n(irq_n),
		.p1(p1),
		.left	( left		),
		.right	( right		),
        .xleft	( xleft		),
        .xright	( xright	),
        .dacleft( dacleft	),
        .dacright(dacright  ),
        .sample	( sample	)
	);

	initial begin
		clk = 0;
		forever #140 clk = ~clk;
	end

	initial begin
	`ifdef DUMPSIGNALS
		`ifdef NCVERILOG
			$shm_open("jt51_test.shm");
			$shm_probe(jt51_test,"AS");
		`else
			$dumpfile("jt51_test.lxt");
			$dumpvars();
			$dumpon;
		`endif
	`endif
	end

	integer int_ch, int_op;

	initial begin
		// Initialize Inputs
		rst = 0;
		cs_n = 1;
		wr_n = 1;
		a0 = 0;
		d_in = 0;
		#10		rst = 1;
		#800	rst = 0;
		$display("dump start");
	end


	reg [15:0] cfg[0:511];
    
	initial begin
		`include "inputs.vh"
	end

	reg [8:0] data_cnt;
	reg [3:0] state, next;
	reg prog_done;
	reg [15:0] waitcnt;

	parameter WAIT_FREE=0, WR_ADDR=1, WR_VAL=2, DONE=3, WRITE=4, 
		BLANK=5, WAIT_CNT=6;

	always @(posedge clk or posedge rst) begin
		if( rst ) begin
			data_cnt 	<= 0;
			prog_done	<= 0;
			next 		<= WR_ADDR;
			state		<= WAIT_FREE;
			waitcnt		<= 16'h0;
		end
		else begin
			case( state )
				BLANK:	state <= WAIT_FREE;
				WAIT_FREE: begin
					// a0 <= 1'b0;
					{ cs_n, wr_n } = 2'b01;
					if( !d_out[7] ) begin
						case( cfg[data_cnt][15:8] )
							8'h0: state <= DONE;
							8'h1: begin
								waitcnt <= { cfg[data_cnt][7:0], 8'h0 };
								state <= WAIT_CNT;							
							end
							// Wait for timer flag:
							8'h3: if( d_out[1:0]&cfg[data_cnt][1:0] ) state<=next;
							default: state <= next;
						endcase
					end
				end
				WAIT_CNT: begin
						if( !waitcnt ) begin
							data_cnt <= data_cnt + 1'b1;
							state <= WAIT_FREE;
						end
						else waitcnt <= waitcnt-1'b1;
					end
				WRITE: begin
					{ cs_n, wr_n } = 2'b00;
					state<= BLANK;
				end
				WR_ADDR: begin
					a0   <= 1'b0;
					d_in <= cfg[data_cnt][15:8];
					next <= WR_VAL;
					state<= WRITE;
				end
				WR_VAL: begin
					a0   <= 1'b1;
					d_in <= cfg[data_cnt][7:0];
					state<= WRITE;
					if( data_cnt == 9'd511 ) begin
						next      <= DONE;
					end
					else begin
						data_cnt <= data_cnt + 1'b1;
						next <= WR_ADDR;
					end
				end
				DONE: prog_done <= 1'b1;
			endcase
		end
	end
