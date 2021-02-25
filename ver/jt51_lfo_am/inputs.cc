#include <iostream>
#include <sstream>

using namespace std;

void p( int adr, int val, string comment ="" ) {
	static int k=0;
	cout << "cfg[" << dec << k++ << "] = { 8'h" << hex << adr << ", 8'h" << val << " };";
	if( comment.size() > 0 )
		cout << " // " << comment;
	cout << '\n';
}

int main( int argc, char *argv[] ) {
	// if( argc<8 ) { cerr << "Faltan argumentos\n"; return 1; }
	int con=7, tl=0, opmask=1, ch=7, fl=0, d1r=1, d2r=1, rr=15, d1l=15, ar=31, ks=3;
	int oct=0, note=0, kf=0, mul=1, dt1=0, dt2=0;
	int amd=0, ams=0, pmd=0, pms=0, lfrq=0, amsen=0, w=1; // LFO
	bool egtest = false, op0test = false, release = false;   
	for(int k=1; k<argc-1; k++ ) {
		string p;
		stringstream( argv[k] ) >> p;
		if( p=="CON" ) stringstream( argv[++k] ) >> con;
		if( p=="CH" ) stringstream( argv[++k] ) >> ch;
		if( p=="OPMASK" ) stringstream( argv[++k] ) >> opmask;
		if( p=="FL" ) stringstream( argv[++k] ) >> fl;
		if( p=="TL" ) stringstream( argv[++k] ) >> tl;
		if( p=="AR" ) stringstream( argv[++k] ) >> ar;
		if( p=="KS" ) stringstream( argv[++k] ) >> ks;
		if( p=="D1R" ) stringstream( argv[++k] ) >> d1r;
		if( p=="D2R" ) stringstream( argv[++k] ) >> d2r;
		if( p=="RR" ) stringstream( argv[++k] ) >> rr;
		if( p=="D1L" ) stringstream( argv[++k] ) >> d1l;
		if( p=="OCT" ) stringstream( argv[++k] ) >> oct;
		if( p=="NOTE" ) stringstream( argv[++k] ) >> note;
		if( p=="KF" ) stringstream( argv[++k] ) >> kf;
		if( p=="MUL" ) stringstream( argv[++k] ) >> mul;
		if( p=="DT1" ) stringstream( argv[++k] ) >> dt1;
		if( p=="DT2" ) stringstream( argv[++k] ) >> dt2;
		// LFO
		if( p=="AMSEN" ) stringstream( argv[++k] ) >> amsen;
		if( p=="LFRQ" ) stringstream( argv[++k] ) >> lfrq;
		if( p=="AMD" ) stringstream( argv[++k] ) >> amd;		
		if( p=="AMS" ) stringstream( argv[++k] ) >> ams;
		if( p=="PMD" ) stringstream( argv[++k] ) >> pmd;		
		if( p=="PMS" ) stringstream( argv[++k] ) >> pms;
		if( p=="W" ) stringstream( argv[++k] ) >> w;
		if( p=="-egtest" ) egtest=true;
		if( p=="-op0test" ) op0test=true;
		if( p=="-release" ) release = true;
	}
	//	PROGRAMACION DEL YM  
   	cout << " // connection = " << con << " OP mask = " << opmask << " total level = " << tl << '\n';
	p( 2, (egtest?1:0)|(op0test?2:0), "Enable EG test mode" );
	// LFO
	p( 0x18, lfrq, "LFO freq" );
	p( 0x19, amd, "LFO AMD"  );
	p( 0x19, 0x80 | pmd, "LFO PMD"  );
	p( 0x1b, w, "LFO waveform" );
	p( 0x28, (oct<<4)|note, "Key code" );
	p( 0x30, kf<<2, "KF" );
    for( int op=0; op<32; op++ ) {
    	p( 0xe0+op, 0xf, "Release rate" );
        p( 0x60+op, 0x7F, "TL off" );
        p( 0x80+op, op, "AR as OP number" );
    }
	for( int op=ch; op<32; op+=8 ) {
		p( 0x40+op, (dt1<<4)|mul, "MUL" );
		int este_opmask = 1 << (op>>3);
		if((op&7)==ch && (este_opmask&opmask)!=0) 
			p( 0x60+op, tl, "TL" );
		// p( 0xc0+op*8, 0x0, "DT2" );
	}
	for( int ch=0; ch<8; ch++ ) {
		p( 8, ch, "key off" );
		//p( 0x20, ch, "connection as CH number" );        
		p( 0x20+ch, 7 );        
	}
	p( 0x20+ch, 0xc0 | (fl<<3) | con, "connection" );    
	p( 0x38+ch, (pms<<4) | ams, "PMS/AMS" );
    // programa la envolvente
    for( int op=0; op<32; op++ ) {
    	if( (op&7) != ch ) continue;
	    // p( 0x80+op, (ks<<6) | ar, "KS/AR" );
        p( 0x80+op, (ks<<6) | ar, "KS/AR" );
	    p( 0xa0+op, (amsen<<7) | d1r, "AMSEN / D1R rate" );
        p( 0xc0+op, (dt2<<6)|d2r, "DT2 / D2R rate" );
    	p( 0xe0+op, (d1l<<4)|rr,  "D1L/RR" );
    }
	p( 0x8, (opmask<<3)|ch , "key on" );
	for( int k=0; k<32; k++ )
		p( 1,1, "Gives time so sound can start");
	if( release )
		p( 0x8, ch , "key off" );	
	p( 0,0, "END");
}
