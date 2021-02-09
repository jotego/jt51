#ifndef __VCDWR_H
#define __VCDWR_H

#include <list>
#include <string>
#include <cstdlib>

class VCDsignal{
public:
    virtual bool update(FILE *fout, uint64_t simtime, bool& tchange, bool force)=0;
};

template <typename A> class VCDsignalX : public VCDsignal{
    A* ref;
    A  last;
    std::string name;
public:
    int id;
    int width;
    bool update(FILE *fout, uint64_t simtime, bool& tchange, bool force);
    VCDsignalX( std::string _name, A* _ref, int _width, int _id ) : name(_name) {
        ref  = _ref;
        width = _width;
        id = _id;
        last = 0;
    }
};

class VCDwr {
    typedef std::list<VCDsignal*> SignalList;
    SignalList signals;
    FILE *fout;
    int vars, scopes;
    bool tchange, force;
    uint64_t time;
public:
    VCDwr( const char *name, const char *toplevel );
    ~VCDwr();
    template <typename A> void define(const char *name, A* ref, int width );
    void set_time( uint64_t t );
    void add_scope( const char *name );
    void close_scope();
    void set_value( const char *name, uint64_t value );
};


template <typename A> void VCDwr::define(const char *name, A* ref, int width ) {
    fprintf( fout, "$var wire %d $%X %s $end\n", width, vars, name );
    VCDsignalX<A> *s = new VCDsignalX<A>( name, ref, width, vars );
    signals.push_back(s);
    vars++;
}

template <typename A> bool VCDsignalX<A>::update(FILE *fout, uint64_t simtime, bool& tchange, bool force) {
    const int intw = sizeof(int)<<3;

    A newval = *ref;
    newval &= (1<<width)-1;
    if( newval != last || force ) {
        last = newval;
        if( tchange ) {
            fprintf( fout, "#%ld\n", simtime);
            tchange = false;
        }
        if( width==1 ) {
            fprintf( fout, "%d$%X\n", (int)newval, id );
        } else {
            int aux=newval<<( intw-width);
            bool first=true;
            fprintf( fout, "b" );
            for( int j=width; j; j--, aux<<=1 ) {
                int v = (aux & (1L<<intw))!=0;
                if( !v && first && j!=1 ) continue;
                first= false;
                fprintf( fout, "%d", v ? 1: 0 );
            }
            fprintf( fout, " $%X\n", id );
        }
        return true;
    } else return false;
}


#endif
