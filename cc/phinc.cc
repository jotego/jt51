// This program produces the phase increment value as derived
// from my own chip measurements
// Jose Tejada, 2014-2021

#include <sstream>
#include <iostream>
#include <fstream>
#include "tables.h"

using namespace std;

int dt1_lut[] = { 0,1,2,3, 0, -3,-2,-1 }; // mejor

void dump_table() {
	for( int dt2=0; dt2<4; dt2++ )
	for( int dt1=0; dt1<8; dt1++ )
	for( int o=0; o<8; o++ )
	for( int n=0; n<16; n++ )
	for( int kf=0; kf<64; kf++ )
	{
		int vdt1 = dt1_lut[dt1];
		cout << o <<'\t'<< n << "\t1\t" << vdt1 <<'\t'<< dt2 <<'\t'<<  kf <<'\t';
		cout << ym_phaseinc( o, n, kf, vdt1, dt2, 1 ) << '\n';
	}
}

int compare( char* sim, bool dt1_null, bool pure ) {
	ifstream fin(sim);
	if( fin.fail() ) {
		cerr << "Couldn't open file " << sim << endl;
		return 1;
	}
	int bad=0;
	for( int dt2=0; dt2<4; dt2++ )
	for( int dt1=0; dt1<8; dt1++ )
	for( int o=0; o<8; o++ )
	for( int n=0; n<16; n++ )
	for( int kf=0; kf<64; kf++ )
	{
		int s;
		fin >> s;
		if( dt1_null && dt1!=0 ) continue;
		if( pure && (n&3)==3 ) continue;
		int vdt1 = dt1_lut[dt1];
		int good = ym_phaseinc( o, n, kf, vdt1, dt2, 1 );
		if( s!= good ) {
			cout << o <<'\t'<< n << "\t1\t" << vdt1 <<'\t'<< dt2 <<'\t'<<  kf <<'\t';
			cout << good <<'\t' << s << '\t' << s-good << '\n';
			bad++;
		}
		if( fin.eof() ) return 0;
	}
	cerr << bad << " errors\n";
	return 0;
}

int main( int argc, char *argv[]) {
	init_tables();
	if( argc !=7 ) {
    	for( int j=1; j<argc; j++ ){
		    if( string(argv[j])=="-t" ) {
			    dump_table();
			    return 0;
		    }
		    if( string(argv[j])=="-c" ) {
            	j++;
			    bool dt1_null=false, pure=false;
			    for( int k=j+1; k<argc; k++ ) {
				    if( string(argv[k])=="-dt1" ) dt1_null=true;
				    if( string(argv[k])=="-pure" ) pure=true;
				    //cout << argv[k] << '\n';
			    }
			    return compare(argv[j], dt1_null, pure);
		    }
	        cerr << "Not enough arguments\n";
	        return 2;
        }
	    cerr << "Not enough arguments. Use\nphinc octave note mul dt1 dt2 kf\n";
	    return 1;
	}

	int octave=0, note=0, mul=0, dt1=0, dt2=0, kf=0;
	int k=1;

	stringstream(argv[k++]) >> octave;
	stringstream(argv[k++]) >> note;
	stringstream(argv[k++]) >> mul;
	stringstream(argv[k++]) >> dt1;
	stringstream(argv[k++]) >> dt2;
	stringstream(argv[k++]) >> kf;

	int phinc = ym_phaseinc( octave, note, kf, dt1, dt2, mul );
	cout << phinc ;
	cout << endl;
	return 0;
}
