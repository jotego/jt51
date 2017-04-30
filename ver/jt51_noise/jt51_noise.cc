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

class Stim {
	unsigned char reg, val;
	ifstream of;
public:
	Stim() {		
		of.open("stimuli", ifstream::binary);
		if( of.fail() ) {
			cerr << "Cannot open stimuli file\n";
			throw 1;
		}
		Next();
	}
	unsigned Reg() { return reg; }
	unsigned Val() { return val; }
	bool Next() {
		if( !of.eof() ) {
			of.read( (char*)&reg, 1);
			of.read( (char*)&val, 1);
			//cout << "Reg = " << hex << (unsigned)reg << " Val = " << (unsigned)val << '\n';
			return true;
		}
		else return false;
	}
};

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
	Stim stim;
	ofstream of(jtname.c_str());
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
	enum { WRITE_REG, WRITE_VAL, WAIT_FINISH } state;
	state = WRITE_REG;
	int last_sample = 0;
	const int half_period=140;
	int clk_time = half_period;
	int finish_time=0;
	bool wait_nonzero=true;
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
					switch( state) {
						case WRITE_REG: 
							top->a0 = 0;
							top->d_in = stim.Reg();
							switch( stim.Reg() ) {
								case 0: 
									cout << "Done!\n"; 
									if(trace)
										finish_time=main_time+(20*1000*1000);
									else
										finish_time=main_time+(80*1000*1000);
									state = WAIT_FINISH;
									break;
								case 1: 
									wait=stim.Val()<<8; 
									//cout << "Wait for " << wait << " clock ticks\n";
									top->cs_n = 1;
									stim.Next();
									break;
								default: state = WRITE_VAL; wait=2;
							}
							break;
						case WRITE_VAL:
							top->a0 = 1;
							top->d_in = stim.Val();
							state = WRITE_REG;
							wait=2;
							stim.Next();
							break;
						case WAIT_FINISH:
							if( main_time>=finish_time ) goto finish;
						}
				}
				else top->cs_n=1;
			}
			if( clk==0 && (dout&0x80==0x80)) top->cs_n = 1;
			int sample = top->sample;

			if( clk == 0 ) {
				if( (sample != last_sample) && sample) {
					int16_t left = top->left;
					if( !(left==0 && wait_nonzero) ) {
						of << left << '\n';
						wait_nonzero = false;
					}
				}
				last_sample = sample;
			}
		}
		main_time+=2;
		if(trace && (main_time%half_period==0)) { tfp->dump(main_time); }
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