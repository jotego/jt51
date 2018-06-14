#include <iostream>
#include <sstream>
#include <string>
#include "tables.h"

using namespace std;

int main( int argc, char *argv[] ) {
	int con=7, tl=0, opmask=1, fl=0;
    bool exact=false;
    int len=1024;
	for(int k=1; k<argc; k++ ) {
		string p(argv[k]);
		if( p=="CON" ) {  stringstream( argv[++k] ) >> con; continue; }
		if( p=="OPMASK" ) {  stringstream( argv[++k] ) >> opmask; continue; }
		if( p=="FL" ) {  stringstream( argv[++k] ) >> fl; continue; }
		if( p=="TL" ) {  stringstream( argv[++k] ) >> tl; continue; }
        if( p=="LEN" ) {  stringstream( argv[++k] ) >> len; continue; }
        if( p=="-x" ) { exact=true; continue; }
        cout << "Argumento invalido: " << p << '\n';
        return 1;
	}
    init_tables();
    int* out = new int[len+50];
    canal( con, opmask, tl, fl, out, len+50, exact );
    bool primo=true;
    for( int k=0, l=len+1; l>0; k++ ) {
    	if( !out[k] && primo ) continue;
        primo=false;
    	cout << out[k] << '\n';
        l--;
    }
    delete []out;
}
