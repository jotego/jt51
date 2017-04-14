#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>

using namespace std;

ofstream verilator;


void pverilator( int k, int adr, int val, string comment ) {
	// verilator input
	if( comment.size() > 0 )
		verilator << " // " << comment << '\n';
	verilator << "reg[" << dec << k << "] = 0x" << hex << adr << "; \t";
	verilator<< "val[" << dec << k << "] = 0x" << hex << val << ";";
	verilator<< '\n';
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

int main( int argc, char *argv[] ) {
	int con=7, tl=0, opmask=1, ch=7, fl=0;
    //int d1r=1, d2r=1, rr=15, d1l=15, ar=31, ks=3;
	//int oct=0, note=0, kf=0, mul=1, dt1=0, dt2=0;
	bool egtest = false, op0test=false;
	int seed=0;

	srand(seed);

	verilator.open("obj_dir/inputs.h");

	for(int k=1; k<argc-1; k++ ) {
		string p( argv[k] );
		if( p=="CON" ) stringstream( argv[++k] ) >> con;
		if( p=="CH" ) stringstream( argv[++k] ) >> ch;
		if( p=="OPMASK" ) stringstream( argv[++k] ) >> opmask;
		if( p=="FL" ) stringstream( argv[++k] ) >> fl;
		if( p=="TL" ) stringstream( argv[++k] ) >> tl;
        /*
		if( p=="AR" ) stringstream( argv[++k] ) >> ar;
		if( p=="KS" ) stringstream( argv[++k] ) >> ks;
		if( p=="D1R" ) stringstream( argv[++k] ) >> d1r;
		if( p=="D2R" ) stringstream( argv[++k] ) >> d2r;
		if( p=="RR" ) stringstream( argv[++k] ) >> rr;
		if( p=="D1L" ) stringstream( argv[++k] ) >> d1l;*/
        /*
		if( p=="OCT" ) stringstream( argv[++k] ) >> oct;
		if( p=="NOTE" ) stringstream( argv[++k] ) >> note;
		if( p=="KF" ) stringstream( argv[++k] ) >> kf;
		if( p=="MUL" ) stringstream( argv[++k] ) >> mul;
		if( p=="DT1" ) stringstream( argv[++k] ) >> dt1;
		if( p=="DT2" ) stringstream( argv[++k] ) >> dt2;*/
		if( p=="-egtest" ) egtest=true;
		if( p=="-op0test" ) op0test=true;
	}
	cout << " // connection = " << con << " OP mask = " << opmask << " total level = " << tl << '\n';
	p( 2, (egtest?1:0)|(op0test?2:0), "Enable EG test mode" );	
	// Random values for all registers
    for( int ch=0x20; ch<0xff; ch++ ) {
    	p( ch, rand()%256, "random value" );
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
	for( int ch_k=0; ch_k<8; ch_k++ ) {
		p( 8, (0xf<<3)|ch_k, "key on, to force key off next" );
		if( ch_k==ch ) 
			p( 8, ch_k, "key off" );
	}
	for( int k=0; k<96; k++ )
	 	p( 1,1, "Gives time so keyoff works");	
	for( int op=0; op<4; op++ ) {
		p( 0x40+op*8+ch, 0x1, "MUL" );
		p( 0x60+op*8+ch, tl, "TL" );
		p( 0xc0+op*8+ch, 0x0, "DT2" );
	}
	p( 0x20+ch, 0xc0 | (fl<<3) | con, "connection" );
	p( 0x8, (opmask<<3) | ch, "key on" );
	p( 0,0, "END");
}
