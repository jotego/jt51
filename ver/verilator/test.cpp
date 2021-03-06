#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <list>
#include "Vjt51.h"
#include "verilated_vcd_c.h"
#include "VGMParser.hpp"
#include "feature.hpp"
#include "WaveWritter.hpp"
#include "opm.h"
#include "ref.h"
#include "cmp.h"

  // #include "verilated.h"

using namespace std;

class SimTime {
    vluint64_t main_time, time_limit, fast_forward;
    vluint64_t main_next;
    int verbose_ticks;
    int toggle_cnt, toggle_step;
    int PERIOD, SEMIPERIOD, CLKSTEP;
    class Vjt51 *top;
    opm_t* ref;
public:
    int32_t ref_output[2];
    void set_period( int _period ) {
        PERIOD =_period;
        PERIOD += PERIOD%2; // make it even
        SEMIPERIOD = PERIOD>>1;
        CLKSTEP = SEMIPERIOD>>2;
        toggle_cnt = toggle_step = 4;
        //CLKSTEP = SEMIPERIOD;
    }
    int period() { return PERIOD; }
    SimTime(Vjt51 *_top, opm_t *_ref ) {
        top = _top;
        ref = _ref;
        ref_output[0]=0;
        ref_output[1]=0;
        main_time=0; fast_forward=0; time_limit=0; toggle_cnt=2;
        verbose_ticks = 48000*24/2;
        set_period(132*6);
    }
    void advance_clock() {
        int clk = top->clk;
        top->clk = 1-clk;
        if( clk==1 ) {
            int cenp1 = top->cen_p1;
            top->cen_p1 = 1-cenp1;
            if( cenp1 ) OPM_Clock( ref, ref_output, NULL, NULL, NULL );
        }
    }

    void set_time_limit(vluint64_t t) { time_limit=t; }
    bool limited() { return time_limit!=0; }
    vluint64_t get_time_limit() { return time_limit; }
    vluint64_t get_time() { return main_time; }
    int get_time_s() { return main_time/1000'000'000; }
    int get_time_ms() { return main_time/1000'000; }
    bool next_quarter() {
        bool adv=false;
        main_time += CLKSTEP;
        if ( !--toggle_cnt ) {
            toggle_cnt=toggle_step;
            advance_clock();
            adv = true;
        }
        top->eval();
        return adv;
    }
    bool finish() { return main_time > time_limit && limited(); }
};

vluint64_t main_time = 0;      // Current simulation time
// This is a 64-bit integer to reduce wrap over issues and
// allow modulus.  You can also use a double, if you wish.

double sc_time_stamp () {      // Called by $time in Verilog
   return main_time;           // converts to double, to match
                               // what SystemC does
}

class CmdWritter {
    int addr, cmd, val;
    Vjt51 *top;
    opm_t *ref;
    bool done;
    int last_clk;
    int state;
    int watch_addr, watch_ch;
    list<FeatureUse>features;
    struct Block_def{ int cmd_mask, cmd, blk_addr;
        int (*filter)(int);
    };
    list<Block_def>blocks;
    // map<int>YMReg mirror;
public:
    CmdWritter( Vjt51* _top, opm_t* _ref );
    void Write( int _addr, int _cmd, int _val );
    void block( int cmd_mask, int cmd, int (*filter)(int), int blk_addr=1 ) {
        Block_def aux;
        aux.cmd_mask = cmd_mask;
        aux.cmd = cmd;
        aux.filter = filter;
        aux.blk_addr = blk_addr;
        cerr << "Added block to " << hex << cmd_mask << " - " << cmd << "/ ADDR=" << blk_addr << '\n';
        blocks.push_back( aux );
    };
    void watch( int addr, int ch ) { watch_addr=addr; watch_ch=ch; }
    void Eval();
    bool Done() { return done; }
    void report_usage();
};


struct YMcmd { int addr; int cmd; int val; };

class WaveOutputs {
    class WaveWritter* mixed;
public:
    WaveOutputs( const string& filename, int sample_rate, bool dump_hex );
    ~WaveOutputs();
    void write( class Vjt51 *top );
    void write( int32_t *val );
};

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


void WaveOutputs::write( class Vjt51 *top ) {
    int16_t snd[2]; // 0=left, 1=right
    snd[0] = top->xleft;
    snd[1] = top->xright;
    mixed->write(snd);
}

void WaveOutputs::write( int32_t* val ) {
    int16_t snd[2];
    snd[0] = val[0];
    snd[1] = val[1];
    mixed->write(snd);
}

const int OPM_SIGNAL=1000'000;

int main(int argc, char** argv, char** env) {
    Verilated::commandArgs(argc, argv);

    Vjt51* top = new Vjt51;
    Ref ref; // Reference design

    CmdWritter writter(top, &ref.opm);
    bool trace = false, trace_ref=false, slow=false;
    RipParser *gym;
    bool forever=true, dump_hex=false, decode_pcm=true;
    char *gym_filename;
    SimTime sim_time(top, &ref.opm);
    Cmp cmp( &ref.opm, top );
    int SAMPLERATE=0;
    vluint64_t SAMPLING_PERIOD=0, trace_start_time=0;
    string wav_filename;

    for( int k=1; k<argc; k++ ) {
        if( string(argv[k])=="-trace" ) { trace=true; continue; }
        if( string(argv[k])=="-trace-ref" ) { trace_ref=true; continue; }
        if( string(argv[k])=="-trace_start" ) {
            int aux;
            sscanf(argv[++k],"%d",&aux);
            cerr << "Trace will start at time " << aux << "ms\n";
            trace_start_time = aux;
            trace_start_time *= 1000'000;
            trace=true;
            continue;
        }
        if( string(argv[k])=="-slow" )  { slow=true;  continue; }
        if( string(argv[k])=="-hex" )  { dump_hex=true;  continue; }
        if( string(argv[k])=="-gym" ) {
            gym_filename = argv[++k];
            gym = ParserFactory( gym_filename, sim_time.period() );
            if( gym==NULL ) return 1;
            continue;
        }
        if( string(argv[k])=="-o" ) {
            if( ++k == argc ) { cerr << "ERROR: expecting filename after -o\n"; return 1; }
            wav_filename = string(argv[k]);
            continue;
        }
        if( string(argv[k])=="-time" ) {
            int aux;
            sscanf(argv[++k],"%d",&aux);
            vluint64_t time_limit = aux;
            time_limit *= 1000'000;
            forever=false;
            cerr << "Simulate until " << time_limit/1000'000 << "ms\n";
            sim_time.set_time_limit( time_limit );
            continue;
        }
        if( string(argv[k])=="-nodecode" ) {
            decode_pcm=false;
            continue;
        }
        if( string(argv[k])=="-nopms" ) {
            writter.block( 0xF8, 0x38, [](int v){return v&0xf;} );
            continue;
        }
        if( string(argv[k])=="-noams" ) {
            writter.block( 0xF8, 0x38, [](int v){return v&0xf0;} );
            continue;
        }
        if( string(argv[k])=="-nodt1" ) {
            writter.block( 0xE0, 0x40, [](int v){return v&0x0f;} );
            continue;
        }
        if( string(argv[k])=="-nodt2" ) {
            writter.block( 0xE0, 0xC0, [](int v){return v&0x1f;} );
            continue;
        }
        if( string(argv[k])=="-mute") {
            int ch;
            if( sscanf(argv[++k],"%d",&ch) != 1 ) {
                cerr << "ERROR: needs channel number after -mute\n";
                return 1;
            }
            if( ch<0 || ch>7 ) {
                cerr << "ERROR: muted channel must be within 0-7 range\n";
                return 1;
            }
            cerr << "Channel " << ch << " muted\n";
            switch(ch) {
                case 0: writter.block( 0xFF, 0x8, [](int v)->int{ return ((v&7)==0)? (v&7) : v;} ); break;
                case 1: writter.block( 0xFF, 0x8, [](int v)->int{ return ((v&7)==1)? (v&7) : v;} ); break;
                case 2: writter.block( 0xFF, 0x8, [](int v)->int{ return ((v&7)==2)? (v&7) : v;} ); break;
                case 3: writter.block( 0xFF, 0x8, [](int v)->int{ return ((v&7)==3)? (v&7) : v;} ); break;
                case 4: writter.block( 0xFF, 0x8, [](int v)->int{ return ((v&7)==4)? (v&7) : v;} ); break;
                case 5: writter.block( 0xFF, 0x8, [](int v)->int{ return ((v&7)==5)? (v&7) : v;} ); break;
                case 6: writter.block( 0xFF, 0x8, [](int v)->int{ return ((v&7)==6)? (v&7) : v;} ); break;
                case 7: writter.block( 0xFF, 0x8, [](int v)->int{ return ((v&7)==7)? (v&7) : v;} ); break;
            }
            continue;
        }
        if( string(argv[k])=="-only") {
            int ch;
            if( sscanf(argv[++k],"%d",&ch) != 1 ) {
                cerr << "ERROR: needs channel number after -only\n";
                return 1;
            }
            if( ch<0 || ch>5 ) {
                cerr << "ERROR: channel must be within 0-5 range\n";
                return 1;
            }
            cerr << "Only channel " << ch << " will be played\n";
            for( int k=0; k<6; k++ ) {
                if( k==ch ) continue;
                switch(k) {
                    case 0: writter.block( 0xFF, 0x28, [](int v)->int{ return (v&0xf)==0? 0 : v;} ); break;
                    case 1: writter.block( 0xFF, 0x28, [](int v)->int{ return (v&0xf)==1? 0 : v;} ); break;
                    case 2: writter.block( 0xFF, 0x28, [](int v)->int{ return (v&0xf)==2? 0 : v;} ); break;
                    case 3: writter.block( 0xFF, 0x28, [](int v)->int{ return (v&0xf)==4? 0 : v;} ); break;
                    case 4: writter.block( 0xFF, 0x28, [](int v)->int{ return (v&0xf)==5? 0 : v;} ); break;
                    case 5: writter.block( 0xFF, 0x28, [](int v)->int{ return (v&0xf)==6? 0 : v;} ); break;
                }
            }
            continue;
        }
        cerr << "ERROR: Unknown argument " << argv[k] << "\n";
        return 1;
    }

    // determines the chip type
    switch( gym->chip() ) {
        case RipParser::ym2203: cerr << "YM2203 tune.\n"; return 1;
        case RipParser::ym2612: cerr << "YM2612 tune.\n"; return 1;
        case RipParser::ym2610: cerr << "YM2610 tune.\n"; return 1;
        case RipParser::ym2151: cerr << "YM2151 tune.\n"; break;
        default: cerr << "ERROR: Unknown chip (" << gym->chip() << ") in VGM file\n"; return 1;
    }

    if( gym->period() != 0 ) {
        int period = gym->period();
        cerr << "Setting PERIOD to " << dec << period << " ns\n";
        sim_time.set_period( period );
    }
    SAMPLING_PERIOD = sim_time.period() * 32; // 32 operators
    SAMPLERATE = 1.0/(SAMPLING_PERIOD*1e-9);
    cerr << "Sample rate " << dec << SAMPLERATE << " Hz. Sampling period " << SAMPLING_PERIOD << "ns\n";

    if( gym->length() != 0 && !sim_time.limited() ) sim_time.set_time_limit( gym->length() );

    VerilatedVcdC* tfp = new VerilatedVcdC;
    if( trace ) {
        Verilated::traceEverOn(true);
        top->trace(tfp,99);
        tfp->open("/dev/stdout");
    }
    // Reset
    memset(&ref.opm, 0, sizeof(opm_t));
    OPM_SetIC( &ref.opm, 1);
    top->rst    = 1;
    top->clk    = 0;
    top->cen    = 1;
    top->cen_p1 = 1;
    top->din    = 0;
    top->a0     = 0;
    top->cs_n   = 0;
    top->wr_n   = 1;
    // cerr << "Reset\n";
    while( sim_time.get_time() < 256*sim_time.period() ) {
        sim_time.next_quarter();
        if(trace && trace_start_time==0 ) {
            tfp->dump(main_time);
            if( trace_ref ) ref.dump(main_time);
        }
    }
    OPM_SetIC( &ref.opm, 0);
    top->rst   = 0;
    int last_a = 0;
    enum { WRITE_REG, WRITE_VAL, WAIT_FINISH } state;
    state = WRITE_REG;

    vluint64_t timeout=0;
    const int check_step = 200;
    int next_check=check_step;
    int reg, val;
    bool fail=true;
    // cerr << "Main loop\n";
    vluint64_t wait=0;
    int last_sample=0;
    WaveOutputs waves( wav_filename, SAMPLERATE, dump_hex );
    WaveOutputs waves_ref( "ref_"+wav_filename, SAMPLERATE, false );
    // forced values
    list<YMcmd> forced_values;
    // main loop
    // writter.watch( 1, 0 ); // top bank, channel 0
    //bool skip_zeros=true;
    bool skip_zeros=false;
    vluint64_t adjust_sum=0;
    int next_verbosity = 200;
    vluint64_t next_sample=0;
    while( forever || !sim_time.finish() ) {
        if( sim_time.next_quarter() ) {
            // int dout = top->dout;
            if( sim_time.get_time() > next_sample ) {
                int16_t snd[2];
                snd[0] = top->xleft;
                snd[1] = top->xright;
                // skip initial set of zero's
                if( !skip_zeros || snd[0]!=0 || snd[1]!=0 ) {
                    skip_zeros=false;
                    waves.write( top );
                    waves_ref.write( sim_time.ref_output );
                }
                //if( !cmp.equal() ) {
                //    printf("Reference and RTL output diverged at time \n");
                //}
                next_sample += SAMPLING_PERIOD;
            }
            last_sample = top->sample;
            writter.Eval();

            if( timeout!=0 && sim_time.get_time()>timeout ) {
                cerr << "Timeout waiting for BUSY to clear\n";
                cerr << "writter.done == " << writter.Done() << '\n';
                goto finish;
            }
            if( sim_time.get_time() < wait ) continue;
            if( !writter.Done() ) continue;

            if( !forced_values.empty() ) {
                const YMcmd &c = forced_values.front();
                cerr << "Forced value\n";
                writter.Write( c.addr, c.cmd, c.val );
                forced_values.pop_front();
                continue;
            }

            int action;
            action = gym->parse();
            switch( action ) {
                default:
                    if( !sim_time.finish() ) {
                        cerr << "go on\n";
                        continue;
                    }
                    goto finish;
                case RipParser::cmd_write:
                    // if( /*(gym->cmd&(char)0xfc)==(char)0xb4 ||*/
                    // /*(gym->addr==0 && gym->cmd>=(char)0x30) || */
                    // ((gym->cmd&(char)0xf0)==(char)0x90)) {
                    //   cerr << "Skipping write to " << hex << (gym->cmd&0xff) << " register\n" ;
                    //  break; // do not write to RL register
                    // }
                    // cerr << "CMD = " << hex << ((int)gym->cmd&0xff) << '\n';
                    writter.Write( gym->addr, gym->cmd, gym->val );
                    timeout = sim_time.get_time() + sim_time.period()*6*100;
                    break; // parse register
                case RipParser::cmd_wait:
                    // cerr << "Waiting\n";
                    wait=gym->wait;
                    // cerr << "Wait for " << dec << wait << "ns (" << wait/1000000 << " ms)\n";
                    // if(trace) wait/=3;
                    wait+=sim_time.get_time();
                    timeout=0;
                    break;// wait 16.7ms
                case RipParser::cmd_finish: // reached end of file
                    goto finish;
                case RipParser::cmd_error: // unsupported command
                    goto finish;
            }
        }
        if( sim_time.get_time()>trace_start_time ) {
            auto t = sim_time.get_time();
            if( trace ) tfp->dump(t);
            if( trace_ref ) ref.dump(sim_time.get_time());
        }
    }
finish:
    writter.report_usage();
    if( skip_zeros ) {
        cerr << "WARNING: Output wavefile is empty. No sound output was produced.\n";
    }

    if( main_time>1000000000 ) { // sim lasted for seconds
        cerr << "$finish at " << dec << sim_time.get_time_s() << "s = " << sim_time.get_time_ms() << " ms\n";
    } else {
        cerr << "$finish at " << dec << sim_time.get_time_ms() << "ms = " << sim_time.get_time() << " ns\n";
    }
    if(trace) tfp->close();
    delete gym;
    delete top;
 }



void CmdWritter::report_usage() {
    cerr << "Features used: \t";
    for( const auto& k : features )
        if(k.is_used()) cerr << k.name() << ' ';
    cerr << '\n';
}

CmdWritter::CmdWritter( Vjt51* _top, opm_t *_ref ) {
    top  = _top;
    ref  = _ref;
    last_clk = 0;
    done = true;
    features.push_back( FeatureUse("DT",    0xE0, 0x40, 0x70, [](char v)->bool{return v!=0;} ));
    features.push_back( FeatureUse("MULT",  0xE0, 0x40, 0x0F, [](char v)->bool{return v!=1;} ));
    //features.push_back( FeatureUse("TL",    0xE0, 0x60, 0x7F, [](char v)->bool{return v!=0;} ));
    //features.push_back( FeatureUse("AR",    0xE0, 0x80, 0x1F, [](char v)->bool{return v!=0;} ));
    features.push_back( FeatureUse("KS",    0xE0, 0x80, 0xE0, [](char v)->bool{return v!=0;} ));
    features.push_back( FeatureUse("AMS-EN",0xE0, 0xA0, 0x80, [](char v)->bool{return v!=0;} ));
    //features.push_back( FeatureUse("D1R",   0xE0, 0xA0, 0x1F, [](char v)->bool{return v!=0;} ));
    features.push_back( FeatureUse("DT2",   0xE0, 0xC0, 0xE0, [](char v)->bool{return v!=0;} ));
    features.push_back( FeatureUse("D1L",   0xE0, 0xE0, 0xF0, [](char v)->bool{return v!=0;} ));
    //features.push_back( FeatureUse("RR",    0xE0, 0xE0, 0x0F, [](char v)->bool{return v!=0;} ));
    features.push_back( FeatureUse("FB",    0xF8, 0x20, 0x38, [](char v)->bool{return v!=0;} ));
    features.push_back( FeatureUse("CON",   0xF8, 0x20, 0x07, [](char v)->bool{return v!=0;} ));
    //features.push_back( FeatureUse("RL",    0xF8, 0x20, 0x30, [](char v)->bool{return v!=0;} ));
    features.push_back( FeatureUse("KC",    0xF8, 0x28, 0x7F, [](char v)->bool{return v!=0;} ));
    features.push_back( FeatureUse("KF",    0xF8, 0x30, 0xFC, [](char v)->bool{return v!=0;} ));
    features.push_back( FeatureUse("AMS",   0xF8, 0x38, 0x03, [](char v)->bool{return v!=0;} ));
    features.push_back( FeatureUse("PMS",   0xF8, 0x38, 0x70, [](char v)->bool{return v!=0;} ));
    features.push_back( FeatureUse("TEST",  0xFF, 0x01, 0xFF, [](char v)->bool{return v!=0;} ));
    features.push_back( FeatureUse("NE",    0xFF, 0x0F, 0x80, [](char v)->bool{return v!=0;} ));
    features.push_back( FeatureUse("NFRQ",  0xFF, 0x0F, 0x1F, [](char v)->bool{return v!=0;} ));
    features.push_back( FeatureUse("LFRQ",  0xFF, 0x18, 0xFF, [](char v)->bool{return v!=0;} ));
    features.push_back( FeatureUse("PMD",   0xFF, 0x19, 0x80, [](char v)->bool{return v!=0;} ));
    features.push_back( FeatureUse("AMD",   0xFF, 0x19, 0x80, [](char v)->bool{return v==0;} ));
    features.push_back( FeatureUse("W",     0xFF, 0x1B, 0x03, [](char v)->bool{return v==0;} ));
    watch_ch = -1;
    //add_op_mirror( 0x30, "DT", 0x70, 2, )
}

void CmdWritter::Write( int _addr, int _cmd, int _val ) {
    // cerr << "Writter command\n";
    for( auto&k : blocks ) {
        int aux = _cmd;
        aux &= k.cmd_mask;
        if( aux == k.cmd ) {
            int old=_val;
            _val = k.filter(old);
            if( old!=_val )
                printf("Blocked %X/ %X -> %X\n", aux, old&0xff, _val);
        }
    }
    addr = _addr;
    cmd  = _cmd;
    val  = _val;
    done = false;
    state = 0;
    if( cmd>=(char)0x30 && (cmd&0x7)==watch_ch )
        cerr << watch_ch << " CMD = " << hex << (cmd&0xff) << " VAL = " << (val&0xff) << '\n';
    for( auto& k : features )
        k.check( cmd, val );
    // cerr << addr << '\t' << hex << "0x" << ((unsigned)cmd&0xff);
    // cerr  << '\t' << ((unsigned)val&0xff) << '\n' << dec;
}

void CmdWritter::Eval() {
    // cerr << "Writter eval " << state << "\n";
    static int wait=0;
    int clk = top->clk;

    if( (clk==0) && (last_clk != clk) ) {
        switch( state ) {
            case 0:
                top->a0  = 0;
                top->din = cmd;
                top->wr_n = 0;

                OPM_Write( ref, 0, cmd);
                state=10;
                break;
            case 10:
                top->wr_n = 1;
                state = 11;
                wait=0;
                break;
            case 11:
                wait++;
                if( wait==8 ) {
                    state = 20;
                }
                break;
            case 20:
                top->a0  = 1;
                top->din = val;
                top->wr_n = 0;
                state = 30;
                OPM_Write( ref, 1, val);
                break;
            case 30:
                top->wr_n = 1;
                state   =40;
                top->a0 = 0; // read busy signal
                break;
            case 40:
                if( (((int)top->dout) &0x80 ) == 0 && !ref->write_busy) {
                    done = true;
                    state=50;
                }
                break;
            default: break;
        }
    }
    last_clk = clk;
}
