#include "Vjt51_lfo.h"
#include "verilated_vcd_c.h"
#include <fstream>
#include <iostream>

using namespace std;

const int PERIOD=280;

class DUT{
    vluint64_t edges, t0_am, t1_am, t0_pm, t1_pm;
    VerilatedVcdC tracer;
    int cycles;
public:
    Vjt51_lfo rtl;
    unsigned last_am, max_am;
    int last_pm;
    int am_cycles, pm_cycles;
    DUT();
    void clock(int n, bool dump=true);
    void update(bool dump=true);
    void record_period();
    float period(int mod=0);
    void reset_period();
};

void am_freq_test(DUT &dut, int amd, int freq_min);
void pm_freq_test(DUT &dut, int pmd, int freq_min);
void am_amp_test(DUT &dut);
void am_wave_test(DUT &dut);

int main() {
    Verilated::traceEverOn(true);
    DUT dut;
    printf("PM test for waveforms 0,1 and 2\n");
    pm_freq_test(dut, 63, 0x80);
    printf("AM test for waveforms 0,1 and 2\n");
    am_freq_test(dut, 63, 0x80);
    //am_wave_test(dut);
    return 0;
}

void am_wave_test(DUT &dut) {
    dut.rtl.lfo_freq=0xff;
    dut.rtl.lfo_amd=127;
    dut.update();
    for( int lfo_w=0; lfo_w<4; lfo_w++ ) {
        dut.rtl.lfo_w=lfo_w;
        dut.clock(100'000,true);
        //printf("%3d -> %3u", lfo_amd, dut.max_am );
        //if( dut.max_am > last_am )
        //    printf(" * ");
        //putchar('\n');
        //last_am = dut.max_am;
    }
}

void am_amp_test(DUT &dut) {
    dut.rtl.lfo_freq=0xff;
    dut.update();
    unsigned last_am=255;
    for( int lfo_amd=0x7f; lfo_amd>=0; lfo_amd-- ) {
        dut.rtl.lfo_amd=lfo_amd;
        dut.clock(100'000,false);
        printf("%3d -> %3u", lfo_amd, dut.max_am );
        if( dut.max_am > last_am )
            printf(" * ");
        putchar('\n');
        last_am = dut.max_am;
    }
}

void pm_freq_test(DUT &dut, int pmd, int freq_min) {
    dut.rtl.lfo_pmd=pmd;
    float last_period[3]={55,55,55};
    bool dump=false;
    for( int freq=0xff; freq>=freq_min; freq-- ) {
        dut.reset_period();
        dut.rtl.lfo_freq=freq;
        dut.update(dump);
        printf("$%2X ", dut.rtl.lfo_freq );
        for( int w=0; w<3; w++ ) {
            dut.rtl.lfo_w=w;
            dut.reset_period();
            for( int timeout=2000; timeout && dut.pm_cycles<3; timeout--)
                dut.clock(500'000,dump);
            float period = dut.period(1);
            printf("%3.2f Hz", period );
            if( period > last_period[w] ) {
                printf(" * ");
                if(!dump ) {
                    // repeat dumping the signals
                    freq_min = freq;
                    freq+=2;
                    dump=true;
                }
            }
            if(w==2)
                putchar('\n');
            else
                printf(", ");
            last_period[w] = period;
        }
        // if( dut.period() < 0 ) {
        //     dut.clock( 50'000'000, true );
        //     printf("$%2X %3.2f Hz*\n", dut.rtl.lfo_freq, dut.period());
        //     break;
        // }
    }
}

void am_freq_test(DUT &dut, int amd, int freq_min) {
    dut.rtl.lfo_amd=amd;
    float last_period[3]={55,55,55};
    bool dump=false;
    for( int freq=0xff; freq>=freq_min; freq-- ) {
    //for( int freq=0xf8; freq<=0xff; freq+=1 ) {
        dut.reset_period();
        dut.rtl.lfo_freq=freq;
        dut.update(dump);
        printf("$%2X ", dut.rtl.lfo_freq );
        for( int w=0; w<3; w++ ) {
            dut.rtl.lfo_w=w;
            dut.reset_period();
            for( int timeout=2000; timeout && dut.am_cycles<3; timeout--)
                dut.clock(500'000,dump);
            float period = dut.period();
            printf("%3.2f Hz", period );
            if( period > last_period[w] ) {
                printf(" * ");
                if(!dump ) {
                    // repeat dumping the signals
                    freq_min = freq;
                    freq+=2;
                    dump=true;
                }
            }
            if(w==2)
                putchar('\n');
            else
                printf(", ");
            last_period[w] = period;
        }
        // if( dut.period() < 0 ) {
        //     dut.clock( 50'000'000, true );
        //     printf("$%2X %3.2f Hz*\n", dut.rtl.lfo_freq, dut.period());
        //     break;
        // }
    }
}

DUT::DUT() : edges(0), cycles(0) {
    rtl.trace(&tracer,99);
    tracer.open("test.vcd");
    rtl.cen=1;

    rtl.lfo_freq  = 0;
    rtl.lfo_amd   = 0;
    rtl.lfo_pmd   = 0;
    rtl.lfo_w     = 0;
    rtl.lfo_up    = 0;
    rtl.noise     = 0;
    rtl.test      = 0;

    rtl.rst = 1;
    clock(4);
    rtl.rst = 0;

    reset_period();
}

void DUT::update(bool dump) {
    rtl.lfo_up=1;
    clock(1, dump);
    rtl.lfo_up=0;
    clock(1, dump);
}

void DUT::clock(int n, bool dump) {
    for(int k=0; k<(n<<1); k++ ) {
        rtl.clk    = k&1;
        rtl.cycles = cycles;
        rtl.eval();
        if( dump ) tracer.dump(edges*PERIOD);
        edges++; // 3.57 MHz / 2, to take into account the cen_p1, which is set to 1
        if( rtl.clk==1 && rtl.rst==0 ) {
            cycles++;
        }
        record_period();
    }
}

void DUT::record_period() {
    unsigned cur_am = (unsigned)rtl.am;
    int cur_pm = rtl.pm;

    if( last_am == 0 && cur_am>last_am ) {
        t0_am = t1_am;
        t1_am = edges;
        am_cycles++;
        max_am = cur_am;
        //cout << "t0_am=" << t0_am << " t1_am=" << t1_am << " am=" << (unsigned)rtl.am << '\n';
    }

    if( (last_pm&0x80) != (cur_pm&0x80) && (cur_pm&0x80)==0x80) {
        t0_pm = t1_pm;
        t1_pm = edges;
        pm_cycles++;
    }

    last_am = cur_am;
    last_pm = cur_pm;
}

float DUT::period( int mod ) {
    float p = (mod==0 ? (t1_am-t0_am) : (t1_pm-t0_pm))>>1;
    p=p*2e-9*PERIOD;
    return p==0 ? 0 : 1/p;
}

void DUT::reset_period() {
    t0_am = t1_am = edges;
    t0_pm = t1_pm = edges;
    am_cycles = pm_cycles = 0;
    last_am = rtl.am;
    last_pm = rtl.pm;
}