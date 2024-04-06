#include "opm.h"
#include "vcdwr.h"
#include <cstring>
#include <cstdio>

class Ref {
    VCDwr wr;
    template <typename A> void dump_array(const char *name, A* signals, int width );
public:
    opm_t opm;
    Ref();
    void dump( uint64_t time );
};

#define DUMP_VCD(a, w) wr.define( #a, &opm.a, w );
#define DUMP_VCD_NAME(a, b, w) wr.define( #a, &opm.b, w );

Ref::Ref() : wr("ref.vcd", "opm") {
    DUMP_VCD(ic, 1)

    DUMP_VCD(cycles, 5)

    wr.add_scope("noise");
    wr.add_scope("LFSR");
        DUMP_VCD(noise_lfsr, 16)
        DUMP_VCD(noise_temp, 1)
        DUMP_VCD(noise_update, 1)
        DUMP_VCD(noise_timer_of, 1)
        DUMP_VCD(noise_timer, 5)
    wr.close_scope();

    wr.add_scope("MMR");
        DUMP_VCD(ch_pms[0],3)
        DUMP_VCD(ch_pms[1],3)
        DUMP_VCD(ch_pms[2],3)
        DUMP_VCD(ch_pms[3],3)
        DUMP_VCD(ch_pms[4],3)
        DUMP_VCD(ch_pms[5],3)
        DUMP_VCD(ch_pms[6],3)
        DUMP_VCD(ch_pms[7],3)
    wr.close_scope();

    wr.add_scope("waveform");
        DUMP_VCD(nc_active, 1)
        DUMP_VCD(nc_active_lock, 1)
        DUMP_VCD(nc_out, 18)
        DUMP_VCD(nc_sign, 1)
        DUMP_VCD(nc_sign_lock, 1)
        DUMP_VCD(nc_sign_lock2, 1)
        DUMP_VCD(noise_en, 1)
        DUMP_VCD(noise_freq, 5)
    wr.close_scope();
    wr.close_scope(); // noise

    // mixer
    DUMP_VCD(op_mix, 18)

    // LFO
    wr.add_scope("LFO");
    DUMP_VCD(lfo_val, 16)
    DUMP_VCD(lfo_trig_sign, 1)
    DUMP_VCD(lfo_saw_sign, 1)
    DUMP_VCD(lfo_wave, 2)
    DUMP_VCD(lfo_out1, 7)
    DUMP_VCD(lfo_out2, 16)
    DUMP_VCD(lfo_out2_b, 16)
    DUMP_VCD(lfo_pm_lock, 8)
    DUMP_VCD(lfo_am_lock, 8)

    DUMP_VCD(lfo_frq_update, 1)
    DUMP_VCD(lfo_freq_hi, 4)
    DUMP_VCD(lfo_freq_lo, 4)
    DUMP_VCD(lfo_bit_counter, 4)
    DUMP_VCD(lfo_val_carry, 1)
    DUMP_VCD(lfo_clock, 1)
    DUMP_VCD(lfo_clock_lock, 1)
    DUMP_VCD(lfo_counter1, 4)
    DUMP_VCD(lfo_counter1_of1, 1)
    DUMP_VCD(lfo_counter1_of2, 1)
    DUMP_VCD(lfo_counter2, 16)
    DUMP_VCD(lfo_counter2_load, 1)
    DUMP_VCD(lfo_counter2_of, 1)
    DUMP_VCD(lfo_counter2_of_lock, 1)
    DUMP_VCD(lfo_counter2_of_lock2, 1)
    DUMP_VCD(lfo_counter3, 4)
    DUMP_VCD(lfo_counter3_step, 1)
    DUMP_VCD(lfo_counter3_clock, 1)
    DUMP_VCD(lfo_mult_carry, 1)
    wr.close_scope();

    DUMP_VCD_NAME(dac_output_l, dac_output[0], 16)
    DUMP_VCD_NAME(dac_output_r, dac_output[1], 16)

    // PG
    wr.add_scope("pg");
        dump_array( "fnum", opm.pg_fnum, 12 );
        dump_array( "kcode", opm.pg_kcode, 5 );
        dump_array( "phase", opm.pg_phase, 20 );
        dump_array( "phinc", opm.pg_inc, 20 );
        dump_array( "ph_rst", opm.pg_reset, 1 );
        DUMP_VCD  ( pg_serial, 1)
    wr.close_scope();

    // DUMP_VCD(eg_st_37, 2)
    // DUMP_VCD(eg_rate_31, 7)
    wr.add_scope("eg");
        DUMP_VCD(eg_serial_bit, 1)
        DUMP_VCD(eg_serial, 10)
        DUMP_VCD(eg_level[000], 10);
        DUMP_VCD(eg_level[001], 10);
        DUMP_VCD(eg_level[002], 10);
        DUMP_VCD(eg_level[003], 10);
        DUMP_VCD(eg_level[004], 10);
        DUMP_VCD(eg_level[005], 10);
        DUMP_VCD(eg_level[006], 10);
        DUMP_VCD(eg_level[007], 10);
        DUMP_VCD(eg_level[010], 10);
        DUMP_VCD(eg_level[011], 10);
        DUMP_VCD(eg_level[012], 10);
        DUMP_VCD(eg_level[013], 10);
        DUMP_VCD(eg_level[014], 10);
        DUMP_VCD(eg_level[015], 10);
        DUMP_VCD(eg_level[016], 10);
        DUMP_VCD(eg_level[017], 10);
        DUMP_VCD(eg_level[020], 10);
        DUMP_VCD(eg_level[021], 10);
        DUMP_VCD(eg_level[022], 10);
        DUMP_VCD(eg_level[023], 10);
        DUMP_VCD(eg_level[024], 10);
        DUMP_VCD(eg_level[025], 10);
        DUMP_VCD(eg_level[026], 10);
        DUMP_VCD(eg_level[027], 10);
        DUMP_VCD(eg_level[030], 10);
        DUMP_VCD(eg_level[031], 10);
        DUMP_VCD(eg_level[032], 10);
        DUMP_VCD(eg_level[033], 10);
        DUMP_VCD(eg_level[034], 10);
        DUMP_VCD(eg_level[035], 10);
        DUMP_VCD(eg_level[036], 10);
        DUMP_VCD(eg_level[037], 10);
    wr.close_scope();

    wr.add_scope("acc");
        DUMP_VCD(mix[0],16)
    wr.close_scope();

    wr.add_scope("op");
        DUMP_VCD(op_out[0],16)
    wr.close_scope();

    wr.add_scope("kon");
        DUMP_VCD(kon_csm,1)
        DUMP_VCD(kon_csm_lock,1)
        DUMP_VCD(kon[0],1)
        DUMP_VCD(kon[1],1)
        DUMP_VCD(kon[2],1)
        DUMP_VCD(kon[3],1)
        DUMP_VCD(kon[4],1)
        DUMP_VCD(kon[5],1)
        DUMP_VCD(kon[6],1)
        DUMP_VCD(kon[7],1)
    wr.close_scope();
}

void Ref::dump( uint64_t time ) {
    wr.set_time( time );
}

template <typename A>void Ref::dump_array(const char *name, A* signals, int width ) {
    char *sz = new char[ std::strlen(name)+10 ];
    for( int k=0; k<32; k++ ) {
        sprintf( sz, "%s_%02o",name,k);
        wr.define( sz, &signals[k], width );
    }
    delete[] sz;
}