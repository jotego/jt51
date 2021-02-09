#include "vcdwr.h"
#include <cstdio>
#include <exception>

using namespace std;

VCDwr::VCDwr( const char *name, const char *toplevel ) {
    fout = fopen( name, "wb" );
    fprintf( fout, "$date\n No particular date\n$end\n");
    fprintf( fout, "$version JT51 $end\n");
    fprintf( fout, "$timescale 1ns $end\n");
    fprintf( fout, "$scope module %s $end\n", toplevel);
    vars=0;
    scopes=1;
    tchange=true;
    force=true;
    time = 0;
}

VCDwr::~VCDwr() {
    for( auto k : signals )
        delete k;
    fclose(fout);
}


void VCDwr::set_time( uint64_t t ) {
    bool closed=true;
    while( scopes-->0 ) {
        fprintf(fout, "$upscope $end\n");
        closed=false;
    }
    if( !closed ) {
        fprintf(fout, "$enddefinitions $end\n$dumpvars\n");
    }
    tchange = true;
    time = t;
    for( auto k : signals ) k->update(fout, time, tchange, force );
    force = false;
}

void VCDwr::add_scope( const char *name ) {
    fprintf( fout, "$scope module %s $end\n", name );
    scopes++;
}

void VCDwr::close_scope() {
    fprintf(fout, "$upscope $end\n");
    scopes--;
}
