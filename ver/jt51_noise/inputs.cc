#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>

using namespace std;

ofstream verilator;


void pverilator( int k, unsigned char adr, unsigned char val, string comment ) {
	// verilator input
	/*
	if( comment.size() > 0 )
		verilator << " // " << comment << '\n';
	verilator << "reg[" << dec << k << "] = 0x" << hex << adr << "; \t";
	verilator<< "val[" << dec << k << "] = 0x" << hex << val << ";";
	verilator<< '\n';*/
	verilator << adr << val;
}


void p( int adr, int val, string comment ="" ) {
	// iverilog/ncverilog input
	static int k=0;	
	pverilator( k, adr, val, comment );
	cout << "cfg[" << dec << k++ << "] = { 8'h" << hex << adr << ", 8'h" << val << " };";
	if( comment.size() > 0 )
		cout << " // " << comment;
	cout << '\n';
}

bool op_in_mask( int op, int opmask ) {
	if( op==0 && (opmask&1) ) return true;
	if( op==1 && (opmask&4) ) return true;
	if( op==2 && (opmask&2) ) return true;
	if( op==3 && (opmask&8) ) return true;
	return false;
}

int main( int argc, char *argv[] ) {
	int con=7, tl=0, opmask=8, ch=7, fl=0;
    int d1r=0, d2r=0, rr=15, d1l=15, ar=31, ks=3;
    int ams_en=0, nfreq=0;
	//int oct=0, note=0, kf=0, dt1=0, dt2=0;
	int mul=1;
	bool egtest = false, op0test=false;
	int seed=0;
	bool rand_fill=true;

	srand(seed);

	verilator.open("obj_dir/stimuli", ofstream::binary);

	for(int k=1; k<argc-1; k++ ) {
		string p( argv[k] );
		if( p=="CON" ) stringstream( argv[++k] ) >> con;
		if( p=="CH" ) stringstream( argv[++k] ) >> ch;
		if( p=="OPMASK" ) stringstream( argv[++k] ) >> opmask;
		if( p=="FL" ) stringstream( argv[++k] ) >> fl;
		if( p=="TL" ) stringstream( argv[++k] ) >> tl;
		if( p=="--norand" ) { cerr << "Using 0's as fill value\n"; rand_fill = false; }        
		if( p=="AR" ) stringstream( argv[++k] ) >> ar;
		if( p=="KS" ) stringstream( argv[++k] ) >> ks;
		if( p=="D1R" ) stringstream( argv[++k] ) >> d1r;
		if( p=="D2R" ) stringstream( argv[++k] ) >> d2r;
		if( p=="RR" ) stringstream( argv[++k] ) >> rr;
		if( p=="D1L" ) stringstream( argv[++k] ) >> d1l;
		if( p=="NFREQ" ) stringstream( argv[++k] ) >> nfreq;
        /*
		if( p=="OCT" ) stringstream( argv[++k] ) >> oct;
		if( p=="NOTE" ) stringstream( argv[++k] ) >> note;
		if( p=="KF" ) stringstream( argv[++k] ) >> kf; */
		if( p=="MUL" ) stringstream( argv[++k] ) >> mul;
		/*
		if( p=="DT1" ) stringstream( argv[++k] ) >> dt1;
		if( p=="DT2" ) stringstream( argv[++k] ) >> dt2;*/
		if( p=="-egtest" ) egtest=true;
		if( p=="-op0test" ) op0test=true;
	}
	cout << " // connection = " << con << " OP mask = " << opmask << " total level = " << tl << '\n';
	p( 2, (egtest?1:0)|(op0test?2:0), "Enable EG test mode" );	
	// Random values for all registers
    for( int k=0x20; k<0xff; k++ ) {
    	int val = rand_fill ? (rand()%256) : 0;
    	if( (k&0xf8)==0x20 ) val &= 0x3f; // output RL disabled channels
    	p( k, val, "fill value" );
    }
    // Now program the channel we want
	p( 0x28+ch, 0x19, "Key code" );
	p( 0x30+ch, 57<<2, "KF" );
	for( int op=ch; op<32; op+=8 ) {        
		p( 0x80+op, 0x1f, "Attack rate" );
		p( 0xc0+op, op, "D2R, used as marker" );
        p( 0xe0+op, 0xf, "Release rate" );
    }
    /**
    for( int op=0; op<32; op++ ) {
    	p( 0xe0+op, 0xf, "Release rate" );
    }*/  
    for( int kcycle=2; kcycle>0; kcycle-- ) {
		for( int ch_k=0; ch_k<8; ch_k++ ) {
			p( 8, (0xf<<3)|ch_k, "key on, to force key off next" );
			p( 8, ch_k, "key off" );
		}
		for( int k=0; k<96; k++ )
		 	p( 1,1, "Gives time so keyoff works");
	}
	for( int op=0; op<4; op++ ) {
		if( op_in_mask( op, opmask ) ) {
			p( 0x40+op*8+ch, mul, "MUL" );
			p( 0x60+op*8+ch, tl, "TL" );
			p( 0x80+op*8+ch, (ks<<6) | ar, "KS/AR" );
			p( 0xa0+op*8+ch, (ams_en<<8) | d1r, "AMS-EN/D1R" );
			p( 0xa0+op*8+ch, 0, "AMS-EN/D1R" );
			p( 0xc0+op*8+ch, 0x0 | d2r, "DT2/D2R" );
			// p( 0xc0+op*8+ch, 0x0 | d2r, "DT2/D2R" );
		}
	}
	p( 0x20+ch, 0xc0 | (fl<<3) | con, "connection" );
	p( 0x0f, 0x80 | nfreq );
	p( 0x8, (opmask<<3) | ch, "key on" );
	p( 1, 255 ); // Wait
	p( 0,0, "END");
}
