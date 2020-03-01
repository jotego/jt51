#include <cstdio>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <list>
#include "set_trace.h"
#include "Vtest.h"
#include "verilated_vcd_c.h"
#include "WaveWritter.hpp"

  // #include "verilated.h"

using namespace std;

class SimTime {
    vluint64_t main_time, time_limit, fast_forward;
    vluint64_t main_next, ticks;
    int toggle_cnt, toggle_step;
    int PERIOD, SEMIPERIOD, CLKSTEP;
    class Vtest *top;
public:
    vluint64_t get_ticks() { return ticks; }
    void set_period( int _period ) {
        PERIOD =_period;
        PERIOD += PERIOD%2; // make it even
        SEMIPERIOD = PERIOD>>1;
        CLKSTEP = SEMIPERIOD>>2;
        toggle_cnt = toggle_step = 4;
        //CLKSTEP = SEMIPERIOD;
    }
    int period() { return PERIOD; }
    SimTime(Vtest *_top) {
        top = _top;
        main_time=0; fast_forward=0; time_limit=0; toggle_cnt=2;
        set_period(22);
    }
    int advance_clock() {
        int clk = top->clk;
        clk=1-clk;
        top->clk = clk;
        top->eval();
        if( clk ) ticks++;
        main_time += SEMIPERIOD;
        return clk;
    }

    void set_time_limit(vluint64_t t) { time_limit=t; }
    bool limited() { return time_limit!=0; }
    vluint64_t get_time_limit() { return time_limit; }
    vluint64_t get_time() { return main_time; }
    int get_time_s() { return main_time/1000000000; }
    int get_time_ms() { return main_time/1000'000; }
    bool finish() { return main_time > time_limit && limited(); }
};

vluint64_t main_time = 0;      // Current simulation time
// This is a 64-bit integer to reduce wrap over issues and
// allow modulus.  You can also use a double, if you wish.

double sc_time_stamp () {      // Called by $time in Verilog
   return main_time;           // converts to double, to match
                               // what SystemC does
}


class WaveOutputs {
    class WaveWritter* mixed;
public:
    WaveOutputs( const string& filename, int sample_rate, bool dump_hex );
    ~WaveOutputs();
    void write( class Vtest *top );
};

class TimeWrites {
    ifstream fin;
    int t0;
    void read_next();
public:
    TimeWrites();
    void report();
    int next_time;
    int next_din;
    int next_a0;
    bool adv();
    bool eof() { return fin.eof(); }
};

void TimeWrites::report() {
    cerr << dec << next_time << ", " << next_a0 << ", " << hex << next_din
        << '\n';
}

void TimeWrites::read_next() {
    char buf[128];
    fin.getline(buf,127);
    sscanf(buf,"%d,%d,%x",&next_time, &next_a0, &next_din);
}

TimeWrites::TimeWrites() {
    fin.open("test_cmd.txt");
    if( fin.bad() || fin.eof() || fin.fail() || !fin.is_open()) {
        cerr << "ERROR: cannot open test_cmd.txt\n";
    }
    read_next();
    t0 = next_time - 16;
    next_time -= t0;
}

bool TimeWrites::adv() {
    read_next();
    next_time -= t0;
    return !fin.eof();
}

int main(int argc, char** argv, char** env) {
    Verilated::commandArgs(argc, argv);
    Vtest* top = new Vtest;
    bool trace = false, forever=true;
    SimTime sim_time(top);
    int SAMPLERATE=0;
    vluint64_t trace_start_time=0;
    string wav_filename="out.wav";

    for( int k=1; k<argc; k++ ) {
        if( string(argv[k])=="-trace" ) { trace=true; continue; }
        if( string(argv[k])=="-trace_start" ) { 
            int aux;
            sscanf(argv[++k],"%d",&aux);
            cerr << "Trace will start at time " << aux << "ms\n";
            trace_start_time = aux;
            trace_start_time *= 1000'000;
            trace=true;
            continue; 
        }
        if( string(argv[k])=="-time" || string(argv[k])=="-t" ) {
            int aux;
            sscanf(argv[++k],"%d",&aux);
            vluint64_t time_limit = aux;
            time_limit *= 1000'000;
            forever=false;
            cerr << "Simulate until " << time_limit/1000'000 << "ms\n";
            sim_time.set_time_limit( time_limit );
            continue;
        }
        cerr << "ERROR: Unknown argument " << argv[k] << "\n";
        return 1;
    }
    SAMPLERATE = 55780;
    cerr << "Sample rate " << dec << SAMPLERATE << " Hz\n";

    #ifdef TRACE
    VerilatedVcdC* tfp = new VerilatedVcdC;
    if( trace ) {
        Verilated::traceEverOn(true);
        top->trace(tfp,99);
        tfp->open("/dev/stdout");
    }
    #endif
    // Reset
    top->rst    = 1;
    top->clk    = 0;
    top->din    = 0;
    top->a0     = 0;
    top->wr_n   = 1;
    // cerr << "Reset\n";
    for(int k=0; k<64; k++ )
        sim_time.advance_clock();
    top->rst = 0;

    int last_sample=0, sample=0;
    WaveOutputs waves( wav_filename, SAMPLERATE, false );
    TimeWrites tw;
    // main loop
    // writter.watch( 1, 0 ); // top bank, channel 0
    bool skip_zeros=true;

    while( forever || !sim_time.finish() ) {
        if( sim_time.advance_clock() ) {
            last_sample = sample;
            sample = top->sample;
            if( sample && !last_sample ) waves.write(top);
            if( sim_time.get_ticks() >= tw.next_time ) {
                //tw.report();
                top->a0   = tw.next_a0;
                top->din  = tw.next_din;
                top->wr_n = 0;
                if( !tw.adv() ) break;
                //cerr << '.';
            }
            else top->wr_n = 1;
        }
        #ifdef TRACE
        if( trace && sim_time.get_time()>trace_start_time )
                tfp->dump(sim_time.get_time());
        #endif
    }
finish:
    if( skip_zeros ) {
        cerr << "WARNING: Output wavefile is empty. No sound output was produced.\n";
    }

    if( main_time>1000000000 ) { // sim lasted for seconds
        cerr << "$finish at " << dec << sim_time.get_time_s() << "s = " << sim_time.get_time_ms() << " ms\n";
    } else {
        cerr << "$finish at " << dec << sim_time.get_time_ms() << "ms = " << sim_time.get_time() << " ns\n";
    }
    #ifdef TRACE
    if(trace) tfp->close();
    #endif
    delete top;
}

///////////////////////////////////////////////////////////////////////

WaveOutputs::WaveOutputs( const string& filename, int sample_rate, bool dump_hex ) {
    string base_name;
    auto pos = filename.find_last_of('.');
    if( pos == string::npos ) pos=filename.length();
    base_name = filename.substr( 0, pos  );
    mixed  = new WaveWritter( base_name+".wav", sample_rate, dump_hex );
}

WaveOutputs::~WaveOutputs() {
    delete mixed;  mixed=0;
}


void WaveOutputs::write( class Vtest *top ) {
    int16_t snd[2]; // 0=left, 1=right
    snd[0] = top->xleft;
    snd[1] = top->xright;
    mixed->write(snd);
}
