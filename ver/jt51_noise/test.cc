#include "Vjt51_noise.h"
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
    Vjt51_noise rtl;
    DUT();
    void clock(int n, bool dump=true);
    void record_period();
    float period(int mod=0);
    void reset_period();
};

int main() {
    Verilated::traceEverOn(true);
    DUT dut;
    // frequency
    for( int nfrq=0; nfrq<32; nfrq++ ) {
        dut.rtl.nfrq=nfrq;
        dut.clock(10'000);
    }
    // amplitude
    dut.rtl.nfrq=16;
    for( int eg=0; eg<1024; eg++ ) {
        dut.rtl.eg = eg;
        dut.clock(320);
    }
    return 0;
}


DUT::DUT() : edges(0), cycles(0) {
    rtl.trace(&tracer,99);
    tracer.open("test.vcd");
    rtl.cen=1;

    rtl.nfrq    = 0;
    rtl.eg      = 0;
    rtl.op31_no = 0;
    rtl.half    = 0;

    rtl.rst = 1;
    clock(4);
    rtl.rst = 0;

    reset_period();
}

void DUT::clock(int n, bool dump) {
    for(int k=0; k<(n<<1); k++ ) {
        rtl.clk    = k&1;
        rtl.half = (cycles&0xf)==0x0;
        rtl.op31_no = (cycles&0x1f)==0x1f;
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

}

float DUT::period( int mod ) {
    return 0;
}

void DUT::reset_period() {

}