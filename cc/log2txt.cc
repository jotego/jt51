#include <iostream>
#include <sstream>
#include <cstdio>

using namespace std;

int main() {
    char line[512];
    cin.getline( line, sizeof(line) );
    while( !cin.eof() ) {
        int sample, reg, val;
        if( sscanf(line,"%d,%X,%X", &sample, &reg, &val ) != 3 ) break;
        string com, ch, op;
        stringstream saux;
        int iaux;
        switch( reg&7 ) {
            case 0: ch = " 0 "; break;
            case 1: ch = " 1 "; break;
            case 2: ch = " 2 "; break;
            case 3: ch = " 3 "; break;
            case 4: ch = " 4 "; break;
            case 5: ch = " 5 "; break;
            case 6: ch = " 6 "; break;
            case 7: ch = " 7 "; break;
        } 
        switch( (reg>>3)&3 ) {
            case 0: op = " 0 "; break;
            case 1: op = " 1 "; break;
            case 2: op = " 2 "; break;
            case 3: op = " 3 "; break;
        }           
        if( reg < 0x20 ) {
            switch( reg ) {
                case    1: com = "test      "; break;
                case    2: com = "test2     "; break;
                case    8: com = "key on    "; 
                    switch( val&7 ) {
                        case 0: ch = " 0 "; break;
                        case 1: ch = " 1 "; break;
                        case 2: ch = " 2 "; break;
                        case 3: ch = " 3 "; break;
                        case 4: ch = " 4 "; break;
                        case 5: ch = " 5 "; break;
                        case 6: ch = " 6 "; break;
                        case 7: ch = " 7 "; break;
                    }           
                    com = com + ch;      
                    iaux = (val>>3)&0xf;
                    com = com + (iaux&8? "*" : "." );
                    com = com + (iaux&4? "*" : "." );
                    com = com + (iaux&2? "*" : "." );
                    com = com + (iaux&1? "*" : "." );
                    break;
                case 0x0f: com = "noise     "; break;
                case 0x10: com = "CLKA1     "; break;
                case 0x11: com = "CLKA2     "; break;
                case 0x12: com = "CLKB      "; break;
                case 0x14: com = "Timer     "; break;
                case 0x18: com = "LFRQ      "; break;
                case 0x19: com = "PMD/AMD   "; break;
                case 0x1B: com = "CTW       "; break;
            }
        }
        if( reg>=0x20 && reg <0x40 ) {
            switch( (reg>>3)&3 ) {
                case 0:    com = "RL/FB/CON "; break;
                case 1:    com = "Keycode   "; break;
                case 2:    com = "KF        "; break;
                case 3:    com = "PMS       "; break;
            }
            com = com + ch;
        }
        if( reg >= 0x40 ) {
            switch( (reg>>5)&7 ) {
                case 2:    com = "DT1       " + ch +"/"+op; break;
                case 3:    com = "TL        " + ch +"/"+op; break;
                case 4:    com = "KS        " + ch +"/"+op; break;
                case 5:    com = "AMS EN    " + ch +"/"+op; break;
                case 6:    com = "DT2       " + ch +"/"+op; break;
                case 7:    com = "D1L       " + ch +"/"+op; break;
            }
        }
        cout << line << " # " << com << '\n';
        cin.getline( line, sizeof(line) );
    }
    return 0;
}