#include "Vjt51_lfo.h"
#include "verilated_vcd_c.h"
#include <fstream>
#include <iostream>

using namespace std;

const int PERIOD=280;

class DUT{
    vluint64_t edges, t0, t1;
    VerilatedVcdC tracer;
    int cycles;
    unsigned last_am;
public:
    Vjt51_lfo rtl;
    int am_cycles;
    DUT();
    void clock(int n, bool dump=true);
    void update();
    void record_period();
    float period();
    void reset_period();
};

void am_freq_test(DUT &dut);

int main() {
    Verilated::traceEverOn(true);
    DUT dut;
    am_freq_test(dut);
    return 0;
}

void am_freq_test(DUT &dut) {
    dut.rtl.lfo_amd=127;
    float last_period=0;
    for( int freq=0xff; freq>=0; freq-- ) {
    //for( int freq=0xf8; freq<=0xff; freq+=1 ) {
        dut.reset_period();
        dut.rtl.lfo_freq=freq;
        dut.update();
        for( int timeout=2000; timeout && dut.am_cycles<2; timeout--)
            dut.clock(500'000,false);
        float period = dut.period();
        printf("$%2X %3.2f Hz", dut.rtl.lfo_freq, period );
        if( period > last_period )
            printf(" * ");
        putchar('\n');
        last_period = period;
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

void DUT::update() {
    rtl.lfo_up=1;
    clock(1);
    rtl.lfo_up=0;
    clock(1);
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
    if( cur_am>last_am ) {
        t0 = t1;
        t1 = edges;
        am_cycles++;
        //cout << "t0=" << t0 << " t1=" << t1 << " am=" << (unsigned)rtl.am << '\n';
    }
    last_am = cur_am;
}

float DUT::period() {
    float p = (t1-t0)>>1;
    p=p*2e-9*PERIOD;
    return p==0 ? 0 : 1/p;
}

void DUT::reset_period() {
    t0 = t1 = edges;
    am_cycles = 0;
    last_am = rtl.am;
}