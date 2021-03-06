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

    // EG
    /*
    DUMP_VCD(eg_level00, 10);
    DUMP_VCD(eg_level01, 10);
    DUMP_VCD(eg_level02, 10);
    DUMP_VCD(eg_level03, 10);
    DUMP_VCD(eg_level04, 10);
    DUMP_VCD(eg_level05, 10);
    DUMP_VCD(eg_level06, 10);
    DUMP_VCD(eg_level07, 10);
    DUMP_VCD(eg_level10, 10);
    DUMP_VCD(eg_level11, 10);
    DUMP_VCD(eg_level12, 10);
    DUMP_VCD(eg_level13, 10);
    DUMP_VCD(eg_level14, 10);
    DUMP_VCD(eg_level15, 10);
    DUMP_VCD(eg_level16, 10);
    DUMP_VCD(eg_level17, 10);
    DUMP_VCD(eg_level20, 10);
    DUMP_VCD(eg_level21, 10);
    DUMP_VCD(eg_level22, 10);
    DUMP_VCD(eg_level23, 10);
    DUMP_VCD(eg_level24, 10);
    DUMP_VCD(eg_level25, 10);
    DUMP_VCD(eg_level26, 10);
    DUMP_VCD(eg_level27, 10);
    DUMP_VCD(eg_level30, 10);
    DUMP_VCD(eg_level31, 10);
    DUMP_VCD(eg_level32, 10);
    DUMP_VCD(eg_level33, 10);
    DUMP_VCD(eg_level34, 10);
    DUMP_VCD(eg_level35, 10);
    DUMP_VCD(eg_level36, 10);
    DUMP_VCD(eg_level37, 10);
*/
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
    wr.close_scope();

    wr.add_scope("kon");
        DUMP_VCD(kon_csm,1)
        DUMP_VCD(kon_csm_lock,1)
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