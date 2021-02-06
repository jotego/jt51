#ifndef __VCDWR_H
#define __VCDWR_H

#include <map>
#include <cstdlib>

struct VCDsignal{
    int id;
    int width;
    uint64_t last;
};

class VCDwr {
    typedef std::map<std::string, VCDsignal*> LastValues;
    LastValues last;
    FILE *fout;
    int vars;
    bool init, tchange;
    uint64_t time;
public:
    VCDwr( const char *name );
    ~VCDwr();
    void define(const char *name, int width );
    void set_time( uint64_t t );
    void set_value( const char *name, uint64_t value );
};

#endif
