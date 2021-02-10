#include "opm.h"
#include "Vjt51.h"

class Cmp {
    opm_t *ref;
    Vjt51 *rtl;
    int fails;
public:
    Cmp( opm_t *pref, Vjt51 *prtl ) {
        ref = pref;
        rtl = prtl;
        fails = 0;
    }
    bool diff( int a, int b ) {
        a &= 0xffff;
        b &= 0xffff;
        if ( a&0x8000 ) a |= ~0x7FFF;
        if ( b&0x8000 ) b |= ~0x7FFF;
        int d = a-b;
        if( d<0 ) d=-d;
        if( d>10 )
            printf("%d <> %d\n", a,b );
        return d>10;
    }
    bool equal() {
        bool bad=false;
        if( diff(ref->dac_output[0], rtl->xleft) || diff(ref->dac_output[1], rtl->xright) ) {
            bad=true;
        }
        if( bad ) fails++;
        return fails<5;
    }
};