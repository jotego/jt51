#include "Vjt51.h"
#include "Vjt51_jt51.h"
#include "Vjt51_jt51_eg.h"
#include "Vjt51_jt51_mmr.h"
#include "verilated_vcd_c.h"
#include "verilated.h"
// sep32 verilog instances
#include "Vjt51_sep32__W1_S1.h"
#include "Vjt51_sep32__W1_S7.h"
//#include "Vjt51_sep32__W1_S8.h"
#include "Vjt51_sep32__W2_S1.h"
#include "Vjt51_sep32__W2_S2.h"
#include "Vjt51_sep32__W2_S3.h"
#include "Vjt51_sep32__W2_S7.h"
#include "Vjt51_sep32__W3_S1.h"
#include "Vjt51_sep32__W3_S2.h"
#include "Vjt51_sep32__W4_S1.h"
#include "Vjt51_sep32__W4_S2.h"
#include "Vjt51_sep32__W4_S6.h"
#include "Vjt51_sep32__W5_S1.h"
#include "Vjt51_sep32__W5_S2.h"
#include "Vjt51_sep32__W5_S6.h"
#include "Vjt51_sep32__W6_S1.h"
#include "Vjt51_sep32__W7_S1.h"
#include "Vjt51_sep32__W7_S7.h"
#include "Vjt51_sep32__W9_S7.h"
#include "Vjt51_sep32__We_S11.h"

#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <string>

using namespace std;

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
	unsigned v = 0;
	while( !(v>=0x20 && v<=0xff ) ) v = rand()%256;
	return v;
}

unsigned random_val() { return rand()%255; }

class JT51_REG {
	unsignedX 	dt1[OPCOUNT],	dt2[OPCOUNT],	mul[OPCOUNT],	tl[OPCOUNT],	ks[OPCOUNT],	
				ar[OPCOUNT],	ame[OPCOUNT],	dr1[OPCOUNT],	dr2[OPCOUNT],	d1l[OPCOUNT],
				rr[OPCOUNT];

	unsignedX 	kc[CHCOUNT], kf[CHCOUNT],	fb[CHCOUNT], con[CHCOUNT],
				rl[CHCOUNT], ams[CHCOUNT],	pms[CHCOUNT];
	/*
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
*/
	bool check( string name, unsignedX* ref, CData* sim, int slot ) {
		unsigned s = sim[slot];
		bool b = ref[slot] == s;
		if( !b )
			cout 	<< name << " #0" << oct << slot << " -> " 
					<< hex << ref[slot] << " =? " << s << "\n";
		return b;
	}
	public:

	int checked() {
		int c=0;
		for( int k=0; k<32; k++ ) {
			c += dt1[k].assigned ? 0 : 1;
			c += dt2[k].assigned ? 0 : 1;
			c += mul[k].assigned ? 0 : 1;
			c +=  tl[k].assigned ? 0 : 1;
			c +=  ks[k].assigned ? 0 : 1;
			c +=  ar[k].assigned ? 0 : 1;
			c += ame[k].assigned ? 0 : 1;
			c += dr1[k].assigned ? 0 : 1;
			c += dr2[k].assigned ? 0 : 1;
			c += d1l[k].assigned ? 0 : 1;
			c +=  rr[k].assigned ? 0 : 1;
		}
		for( int k=0; k<8; k++ ) {
			c +=  kc[k].assigned ? 0 : 1;
			c +=  kf[k].assigned ? 0 : 1;
			c +=  fb[k].assigned ? 0 : 1;
			c += con[k].assigned ? 0 : 1;
			c +=  rl[k].assigned ? 0 : 1;
			c += ams[k].assigned ? 0 : 1;
			c += pms[k].assigned ? 0 : 1;
		}
		return c;
	}

	bool check(Vjt51* t) {
		bool e=true;
		for( int slot=0; slot<=037; slot++ ) {
			e = e && check( "dt1", dt1, t->jt51->u_mmr->sep_dt1->slots, slot );
			e = e && check( "dt2", dt2, t->jt51->u_mmr->sep_dt2->slots, slot );
			e = e && check( "mul", mul, t->jt51->u_mmr->sep_mul->slots, slot );
			e = e && check( " tl",  tl, t->jt51->u_mmr->sep_tl->slots,  slot );
			e = e && check( " ks",  ks, t->jt51->u_mmr->sep_ks->slots,  slot );
			e = e && check( " ar",  ar, t->jt51->u_mmr->sep_ar->slots,  slot );
			e = e && check( "ame", ame, t->jt51->u_mmr->sep_ame->slots, slot );
			e = e && check( "ame", ame, t->jt51->u_mmr->sep_ame->slots, slot );
			e = e && check( "dr1", dr1, t->jt51->u_mmr->sep_dr1->slots, slot );
			e = e && check( "dr2", dr2, t->jt51->u_mmr->sep_dr2->slots, slot );
			e = e && check( " rr",  rr, t->jt51->u_mmr->sep_rr->slots,  slot );
			e = e && check( "d1l", d1l, t->jt51->u_mmr->sep_d1l->slots, slot );
		}
		for( int ch=0; ch<8; ch++ ) {
			e = e && check( " rl",  rl, t->jt51->u_mmr->sep_rl->slots,  ch );
			e = e && check( " fb",  fb, t->jt51->u_mmr->sep_fb->slots,  ch );
			e = e && check( "con", con, t->jt51->u_mmr->sep_con->slots, ch );
			e = e && check( " kc",  kc, t->jt51->u_mmr->sep_kc->slots,  ch );
			e = e && check( " kf",  kf, t->jt51->u_mmr->sep_kf->slots,  ch );
			e = e && check( "pms", pms, t->jt51->u_mmr->sep_pms->slots, ch );
			e = e && check( "ams", ams, t->jt51->u_mmr->sep_ams->slots, ch );
		}
		return e;
	}
	void write( int reg, unsigned val ) {
		int op   = (reg>>3)&3;
		int ch   = reg&7;
		int slot = reg & 0x1f;
		//cout << "MMR[0x" << hex << reg << "] = 0x" << val << "\n";
		if( reg>=0x20 && reg<0x40 ) {
			switch(reg>>3) {
			// Channel registers
			case (0x20>>3):
				rl[ch]	= (val>>6) & 3;
				fb[ch]	= (val>>3) & 7;
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
					ame[slot] = (val>>7)&1;
					dr1[slot] = val&0x1f;
					break;
				case (0xc0>>5):
					dt2[slot] = (val>>6)&3;
					dr2[slot] = val&0x1f;
					break;
				case (0xe0>>5):
					d1l[slot] = (val>>4)&0xf;
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
	int reps=0;
	for( int k=0; k<argc; k++ ) {
		if( string(argv[k])=="--trace" ) { trace=true; continue; }
		if( string(argv[k])=="--jtname" ) { jtname = argv[++k]; continue; }
	}
	VerilatedVcdC* tfp = new VerilatedVcdC;
	if( trace ) {
		Verilated::traceEverOn(true);
		top->trace(tfp,99);
		tfp->open("../jt51_mmr.vcd");	
	}

	cout << "JT51 MMR testbench\n";
	// Reset
	top->clk = 0;
	top->rst = 1;
	top->cen = 1;
	top->cen_p1 = 1;
	top->cs_n = 1;
	top->wr_n = 0;
	top->a0 = 0;
	top->din = 0;
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
	int state=0;
	const int half_period=140;
	vluint64_t clk_time = half_period;
	bool wait_nonzero=true;
	const int check_step = 200;
	int next_check=check_step;
	JT51_REG ref_mmr;
	int reg, val;
	bool fail=true;

	while( true ) {
		top->eval();
		if( clk_time==main_time ) {
			int clk = top->clk;
			clk_time = main_time+half_period;
			top->clk = 1-clk;
			if( clk==1 ) ticks++;
			int dout = top->dout;
			// cout << "clk = " << clk << " dout = " << dout << '\n';
			if( clk==0 ) {
				if( (dout&0x80)==0 && (--wait<=0)) {
					if( next_check==0 ) { 
						bool b = ref_mmr.check(top);
						reps += check_step;
						next_check = check_step;
						cout << "#" << main_time << "\t" << reps;
						int unchecked = ref_mmr.checked();
						if (unchecked) cout << " unchecked " << unchecked;
						cout << endl;
						if ( /*--reps==0 ||*/ !b ) { fail=true; goto finish; }
						if ( reps>50000 ) { fail=false; goto finish; }
					}
					top->cs_n = 0;
					// cout << "#" << main_time;
					// cout << "\tstate= " << state << " cmd_cnt = " << cmd_cnt;
					// cout << " reg=" << reg[cmd_cnt] << '\n';
					switch( state) {
						case 0: 
							top->a0 = 0;
							reg = random_reg();
							// cout << "Wr to " << reg << " ";
							top->din = reg;
							state++;
							wait=rand()%8;							
							break;
						case 2:
							top->a0 = 1;
							val = random_val();
							top->din = val;
							state++;
							wait=64+(rand()%256);
							ref_mmr.write( reg, val );
                            break;
                        case 4:
							next_check--;
                        default: 
                            state++;
                            state&=0xf;
                            break;
						}
				}
				else top->cs_n=1;
			}
			if( clk==0 && (dout&0x80==0x80)) top->cs_n = 1;
		}
		main_time+=2;
		if(trace && (main_time%70==0)) { tfp->dump(main_time); }
	}
finish:
	cout << "$finish: #" << dec << main_time << '\n';
	if(trace) tfp->close();	
	delete top;
	if( fail ) {
		cout << "Test FAIL\n";
		return 1;
	}
	else {
		cout << "Test PASS\n";
		return 0;
	}
}