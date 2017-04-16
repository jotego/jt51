#include "Vjt51.h"
#include "Vjt51_jt51.h"
#include "Vjt51_jt51_eg.h"
#include "Vjt51_jt51_mmr.h"
#include "verilated_vcd_c.h"
#include "verilated.h"
// sep32 verilog instances
#include "Vjt51_sep32__W7_S7.h"
#include "Vjt51_sep32__W2_S1.h"
#include "Vjt51_sep32__W2_S3.h"


#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <string>

Vjt51* top;
vluint64_t main_time = 0;	   // Current simulation time
// This is a 64-bit integer to reduce wrap over issues and
// allow modulus.  You can also use a double, if you wish.

double sc_time_stamp () {	   // Called by $time in Verilog
   return main_time;		   // converts to double, to match
							   // what SystemC does
}

#define CHCOUNT 8
#define OPCOUNT (CHCOUNT*4)

class unsignedX {
public:
	unsigned 	val;
	bool		assigned;
	unsignedX() { val=0; assigned=false; }
	unsigned operator=(unsigned v) {
		val = v;
		assigned = true;
		return v;
	}
	bool operator==(unsigned v) {
		if( !assigned ) return true;
		return v == val;
	}
	operator unsigned() { return val; }
};

unsigned random_reg() {
	unsigned v = rand()%224;
	return v+0x20;
}

unsigned getSlot( int slot, int stg, Vjt51_sep32__W7_S7*sep ) {
	unsigned v;
	slot = (slot+stg-1)&0x1f;
	// cout << "TL Slot=" << slot << '\n';
	switch(slot) { // octal constants!
		case 000: v = sep->slot_00; break; 
		case 001: v = sep->slot_01; break; 
		case 002: v = sep->slot_02; break; 
		case 003: v = sep->slot_03; break; 
		case 004: v = sep->slot_04; break; 
		case 005: v = sep->slot_05; break; 
		case 006: v = sep->slot_06; break; 
		case 007: v = sep->slot_07; break; 

		case 010: v = sep->slot_10; break; 
		case 011: v = sep->slot_11; break; 
		case 012: v = sep->slot_12; break; 
		case 013: v = sep->slot_13; break; 
		case 014: v = sep->slot_14; break; 
		case 015: v = sep->slot_15; break; 
		case 016: v = sep->slot_16; break; 
		case 017: v = sep->slot_17; break; 

		case 020: v = sep->slot_20; break; 
		case 021: v = sep->slot_21; break; 
		case 022: v = sep->slot_22; break; 
		case 023: v = sep->slot_23; break; 
		case 024: v = sep->slot_24; break; 
		case 025: v = sep->slot_25; break; 
		case 026: v = sep->slot_26; break; 
		case 027: v = sep->slot_27; break; 

		case 030: v = sep->slot_30; break; 
		case 031: v = sep->slot_31; break; 
		case 032: v = sep->slot_32; break; 
		case 033: v = sep->slot_33; break; 
		case 034: v = sep->slot_34; break; 
		case 035: v = sep->slot_35; break; 
		case 036: v = sep->slot_36; break; 
		case 037: v = sep->slot_37; break; 
	}	
}

unsigned getSlot( int slot, int stg, Vjt51_sep32__W2_S1*sep ) {
	unsigned v;
	slot = (slot+stg-1)&0x1f;
	// cout << "RL Slot=" << slot << '\n';
	switch(slot) { // octal constants!
		case 000: v = sep->slot_00; break; 
		case 001: v = sep->slot_01; break; 
		case 002: v = sep->slot_02; break; 
		case 003: v = sep->slot_03; break; 
		case 004: v = sep->slot_04; break; 
		case 005: v = sep->slot_05; break; 
		case 006: v = sep->slot_06; break; 
		case 007: v = sep->slot_07; break; 

		case 010: v = sep->slot_10; break; 
		case 011: v = sep->slot_11; break; 
		case 012: v = sep->slot_12; break; 
		case 013: v = sep->slot_13; break; 
		case 014: v = sep->slot_14; break; 
		case 015: v = sep->slot_15; break; 
		case 016: v = sep->slot_16; break; 
		case 017: v = sep->slot_17; break; 

		case 020: v = sep->slot_20; break; 
		case 021: v = sep->slot_21; break; 
		case 022: v = sep->slot_22; break; 
		case 023: v = sep->slot_23; break; 
		case 024: v = sep->slot_24; break; 
		case 025: v = sep->slot_25; break; 
		case 026: v = sep->slot_26; break; 
		case 027: v = sep->slot_27; break; 

		case 030: v = sep->slot_30; break; 
		case 031: v = sep->slot_31; break; 
		case 032: v = sep->slot_32; break; 
		case 033: v = sep->slot_33; break; 
		case 034: v = sep->slot_34; break; 
		case 035: v = sep->slot_35; break; 
		case 036: v = sep->slot_36; break; 
		case 037: v = sep->slot_37; break; 
	}	
}

unsigned random_val() { return rand()%255; }

class JT51_REG {
	unsignedX 	dt1[OPCOUNT],	dt2[OPCOUNT],	mul[OPCOUNT],	tl[OPCOUNT],	ks[OPCOUNT],	
				ar[OPCOUNT],	am[OPCOUNT],	dr[OPCOUNT],	sr[OPCOUNT],	sl[OPCOUNT],
				rr[OPCOUNT];

	unsignedX 	kc[CHCOUNT], kf[CHCOUNT],	fb[CHCOUNT], con[CHCOUNT],
				rl[CHCOUNT], ams[CHCOUNT],	pms[CHCOUNT];
	
	void printall_tl(Vjt51_sep32__W7_S7*sep) {
		for( int slot=0; slot<040; slot++ ) {
			unsigned v = getSlot(slot,1,sep);
			cout << oct << slot << "-> " << hex << tl[slot];
			if( tl[slot]==v ) cout << "=="; else cout << "!=";
			cout << v;
			if( tl[slot]!=v ) cout << " *";
			cout << '\n';
		}
	}
/*
	bool check_any( int slot, Vjt51_sep32__W7_S7*sep ) {
		unsigned v = getSlot( slot, 1, sep );
		bool b = tl[slot] == v;
		if( !b ) printall_tl(sep);
		return b;
	}
*/
	bool check_tl( int slot, Vjt51_sep32__W7_S7*sep ) {
		unsigned v = getSlot( slot, 1, sep );
		bool b = tl[slot] == v;
		if( !b ) printall_tl(sep);
		return b; //check_any( tl[slot], v, "TL" );
	}

	bool check_rl( int slot, Vjt51_sep32__W2_S1*sep ) {
		unsigned v = getSlot( slot, 1, sep );
		bool b = rl[slot] == v;
		if( !b ) cout << "Error RL\n";
		return b;
	}
/*
	bool check_ks( int slot, Vjt51_sep32__W2_S3*sep ) {
		unsigned v = getSlot( slot, 1, sep );
		bool b = rl[slot] == v;
		if( !b ) cout << "Error RL\n";
		return b;
	}
*/
	public:

	bool check(Vjt51* t) {
		bool e=true;
		for( int slot=0; slot<=037; slot++ ) {
			e = e && check_tl(slot,t->jt51->u_mmr->sep_tl);
			// e = e && check_rl(slot,t->jt51->u_mmr->sep_rl);
		}
		return e;
	}
	void write( int reg, unsigned val ) {
		int op   = (reg>>3)&3;
		int ch   = reg&7;
		int slot = reg & 0x1f;
		//cout << "MMR[0x" << hex << reg << "] = 0x" << val << "\n";
		if( reg>=0x20 && reg<0x40 ) {
			switch(reg) {
			// Channel registers
			case (0x20>>3):
				rl[ch]	= (val>>6) & 3;
				fb[ch]	= (val>>3) & 3;
				con[ch]	= val&7;
				break;
			case (0x28>>3):
				kc[ch]	= val & 0x7f;
				break;
			case (0x30>>3):
				kf[ch]	= (val>>2)&0x3f;
				break;
			case (0x38>>3):
				ams[ch]= val & 3;
				pms[ch]= (val>>4) & 7;
				break;
			}
		}
		else {
			switch( reg>>5 ) {
				case (0x40>>5): 
					dt1[slot] = (val>>4)&7;
					mul[slot]= val&0xf;
					break;
				case (0x60>>5):
					tl[slot] = val&0x7f;
					break;
				case (0x80>>5):
					ks[slot] = (val>>6)&3;
					ar[slot] = val&0x1f;
					break;
				case (0xa0>>5):
					am[slot] = (val>>7)&1;
					dr[slot] = val&0x1f;
					break;
				case (0xc0>>5):
					dt2[slot] = (val>>6)&3;
					sr[slot] = val&0x1f;
					break;
				case (0xe0>>5):
					sl[slot] = (val>>4)&0xf;
					rr[slot] = val&0xf;
					break;
			}
		}
	}
};

int main(int argc, char **argv, char **env) {
	Verilated::commandArgs(argc, argv);
	top = new Vjt51;
	bool trace=false;
	string jtname="verilator.jt";
	int reps=64;
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

	cout << "JT51 MMR testbench\n";
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
	const int half_period=140;
	int clk_time = half_period;
	bool wait_nonzero=true;
	int next_check=32;
	JT51_REG ref_mmr;
	int reg, val;

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
				if( (dout&0x80)==0 && (--wait<=0)) {
					if( next_check==0 ) { 
						bool b = ref_mmr.check(top);
						cout << reps << '\t';
						next_check = 32;
						if ( --reps==0 || !b ) goto finish;						
					}
					top->cs_n = 0;
					// cout << "#" << main_time;
					// cout << "\tstate= " << state << " cmd_cnt = " << cmd_cnt;
					// cout << " reg=" << reg[cmd_cnt] << '\n';
					switch( state) {
						case WRITE_REG: 
							top->a0 = 0;
							reg = random_reg();
							top->d_in = reg;
							state = WRITE_VAL; 
							wait=2;							
							break;
						case WRITE_VAL:
							top->a0 = 1;
							val = random_val();
							top->d_in = val;
							state = WRITE_REG;
							wait=64;
							ref_mmr.write( reg, val );
							next_check--;
							break;
						}
				}
				else top->cs_n=1;
			}
			if( clk==0 && (dout&0x80==0x80)) top->cs_n = 1;
		}
		main_time+=2;
		//if( main_time > 10*1000*1000 ) break;
		if(trace) tfp->dump(main_time);
	}
finish:
	cout << "$finish: #" << dec << main_time << '\n';
	if(trace) tfp->close();
	delete top;
	return 0;
}