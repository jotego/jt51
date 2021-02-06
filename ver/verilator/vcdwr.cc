#include "vcdwr.h"
#include <cstdio>
#include <exception>

using namespace std;

VCDwr::VCDwr( const char *name ) {
    fout = fopen( name, "wb" );
    fprintf( fout, "$timescale 1ns $end\n");
    vars=0;
    init=true;
    tchange=true;
    time = 0;
}

VCDwr::~VCDwr() {
    for( auto k : last )
        delete k.second;
    last.erase( last.begin(), last.end() );
    fclose(fout);
}

void VCDwr::define(const char *name, int width ) {
    if( !init ) throw runtime_error("added new VCD signal after setting values or time");
    fprintf( fout, "$var wire %d $%X %s $end\n", width, vars, name );
    VCDsignal *s = new VCDsignal( {vars, width, 0} );
    last[name] = s;
    vars++;
}

void VCDwr::set_time( uint64_t t ) {
    init = false;
    tchange = true;
    time = t;
}

void VCDwr::set_value( const char *name, uint64_t value ) {
    string n(name);
    auto k = last.find(n);
    VCDsignal *s;
    if( k==last.end()) {
        throw runtime_error("cannot find VCD signal");
    }
    else s = k->second;
    value &= (1<<s->width)-1;
    if( value!=s->last ) {
        if( tchange ) {
            fprintf( fout, "#%ld\n", time);
            tchange = false;
        }
        s->last = value;
        if( s->width==1 ) {
            fprintf( fout, "%ld$%d\n", value, s->id );
        } else {
            uint64_t aux=value<<(64-s->width);
            bool first=true;
            //fprintf( fout, "value=%lx\n", value );
            fprintf( fout, "b" );
            for( int j=s->width; j; j--, aux<<=1 ) {
                int v = (aux & (1L<<63))!=0;
                if( !v && first ) continue;
                first= false;
                fprintf( fout, "%d", v ? 1: 0 );
            }
            fprintf( fout, " $%X\n", s->id );
        }
    }
    init = false;
}