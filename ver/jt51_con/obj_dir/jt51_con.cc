#include "Vjt51.h"
#include "verilated_vcd_c.h"
#include "verilated.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>

Vjt51* top;
vluint64_t main_time = 0;       // Current simulation time
// This is a 64-bit integer to reduce wrap over issues and
// allow modulus.  You can also use a double, if you wish.

double sc_time_stamp () {       // Called by $time in Verilog
   return main_time;           // converts to double, to match
                               // what SystemC does
}

int main(int argc, char **argv, char **env) {
	Verilated::commandArgs(argc, argv);
	top = new Vjt51;
	bool trace=false;
	string jtname="verilator.jt";
	for( int k=0; k<argc; k++ ) {
		if( string(argv[k])=="--trace" ) trace=true;
		if( string(argv[k])=="--jtname" ) { jtname = argv[++k];}
	}
	VerilatedVcdC* tfp = new VerilatedVcdC;
	if( trace ) {
		Verilated::traceEverOn(true);
		top->trace(tfp,99);
		tfp->open("jt51_con.vcd");
	}

	int reg[65536], val[65535];
	ofstream of(jtname.c_str());
	#include "inputs.h"
	cout << "JT51 Connection testbench\n";
	// Reset
	top->clk = 0;
	top->rst = 1;
	top->cs_n = 1;
	top->wr_n = 0;
	top->a0 = 0;
	top->d_in = 0;
	while( main_time < 100 ) {
		top->eval();
		if( main_time%10==0 ) top->clk = 1-top->clk;
		main_time++;
		if(trace) tfp->dump(main_time);
	}
	top->rst = 0;
	int last_a=0, ticks=0;
	int wait=10;
	int cmd_cnt=0;
	enum { WRITE_REG, WRITE_VAL, WAIT_FINISH } state;
	state = WRITE_REG;
	int last_sample = 0;
	const int half_period=140;
	int clk_time = half_period;
	int finish_time=0;
	while( true ) {
		top->eval();
		if( clk_time==main_time ) {
			int clk = top->clk;
			clk_time = main_time+half_period;
			top->clk = 1-clk;
			if( clk==1 ) ticks++;
			int dout = top->d_out;
			// cout << "clk = " << clk << " dout = " << dout << '\n';
			if( clk==0 ) {
				if( wait>0) 
					wait--;
				else if( (dout&0x80)==0 ) {
					top->cs_n = 0;
					// cout << "#" << main_time;
					// cout << "\tstate= " << state << " cmd_cnt = " << cmd_cnt;
					// cout << " reg=" << reg[cmd_cnt] << '\n';
					switch( state) {
						case WRITE_REG: 
							top->a0 = 0;
							top->d_in = reg[cmd_cnt];
							switch( reg[cmd_cnt] ) {
								case 0: 
									cout << "Done!\n"; 
									finish_time=main_time+(80*1000*1000);
									state = WAIT_FINISH;
									break;
								case 1: 
									wait=val[cmd_cnt]<<8; 
									//cout << "Wait for " << wait << " clock ticks\n";
									top->cs_n = 1;
									cmd_cnt++;
									break;
								default: state = WRITE_VAL; wait=2;
							}
							break;
						case WRITE_VAL:
							top->a0 = 1;
							top->d_in = val[cmd_cnt++];
							state = WRITE_REG;
							wait=2;
							break;
						case WAIT_FINISH:
							if( main_time>=finish_time ) goto finish;
						}
				}
				else top->cs_n=1;
			}
			if( clk==0 && (dout&0x80==0x80)) top->cs_n = 1;
			int sample = top->sample;

			if( (sample != last_sample) && sample) {
				int left = top->left;
				of << left << '\n';
			}
			last_sample = sample;
		}
		main_time++;
		if(trace) tfp->dump(main_time);
		/*
		int a = top->reg_a;
		if( a!= last_a) {
			cout << "A=" << a << "\n";
			last_a = a;
		}
		*/
	}
finish:
	cout << "$finish: #" << main_time << '\n';
	if(trace) tfp->close();
	delete top;
	return 0;
}