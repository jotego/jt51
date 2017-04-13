#include <iostream>
#include <iomanip>

using namespace std;

int k=0;

void wr( int port, int val) {
	cout << "cfg["<< dec << k<<"] = { 8'h" << hex << port;
	cout << ", 8'h" << val << " };\n";
	k++;
}

void wait( int ms ) {
	// wr(1,100) => wait for 7.19ms
	int c = 100.0*ms/7.19;
	while(c>0) {
		int p = c>255? 255 : c;
		wr( 1, p );
		c-=p;
	}
}

int main() {
	for( int j=0; j<1; j++) {
		wr( 0x20+j, 0xc7 );
		wr( 0x28+j, 0x40 );
		wr( 0x30+j, 0 );
		wr( 0x38+j, 0 );		
		for( int op=0; op<4; op++ ) {
			wr( 0x40+j+(op<<3), 1); // MUL)
			wr( 0x60+j+(op<<3), op==0?0 : 127); // TL
			wr( 0x80+j+(op<<3), 31); // AR
			wr( 0xA0+j+(op<<3), 0); // D1R
			wr( 0xC0+j+(op<<3), 0); // D2R
			wr( 0xE0+j+(op<<3), 0); // RR
		}		
	}
	wr( 0x8, 8 | 0); // Key on
	wait( 6 ); // ms to simulate of the sine wave
	wr( 0, 0); // finish
	return 0;
}