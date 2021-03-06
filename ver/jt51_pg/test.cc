#include "verilated_vcd_c.h"
#include "Vjt51_pg.h"
#include "opm.h"
#include "tables.h"
#include <cstdio>
#include <cmath>

using namespace std;

struct jt51_st{
    int phinc;
    int kcode;
};

int ref      ( opm_t* opm,                 int kc, int kf, int lfo, int pms, int dt1, int dt2, int mul );
void eval_dut( Vjt51_pg* dut, jt51_st& st, int kc, int kf, int lfo, int pms, int dt1, int dt2, int mul );

extern "C" {
    void OPM_PhaseCalcFNumBlock(opm_t *chip);
    void OPM_PhaseCalcIncrement(opm_t *chip);
}


float phinc2freq( int phinc ) {
    const float CRYSTAL=3.579545e6;
    return ((float)phinc)*CRYSTAL/32/(1<<20)/2.0;
}

float cent( float a, float b ) {
    const float log2 = 0.693147;
    return 1200*log(a/b)/log2;
}

int test_all( Vjt51_pg& dut, opm_t& opm );
void report( const char* sz, bool bad );

int test_dt1( Vjt51_pg& dut, opm_t& opm );
int test_dt2( Vjt51_pg& dut, opm_t& opm );
int test_pms( Vjt51_pg& dut, opm_t& opm );
int test_pg_nolfo( Vjt51_pg& dut, opm_t& opm);

bool check_dt1( int oct, int note, int dt1, float delta );
float expectedHz( int kc, int kf, int lfo, int pms, int dt1, int dt2, int mul );

int main(int argc, char *argv[]) {
    Vjt51_pg dut;
    opm_t opm;

    OPM_Reset(&opm);

    bool run_dt1=true,
         run_dt2=true,
         run_pms=true,
         run_nolfo=false;

    int good=0; // 0 = no errors

    if( run_dt1 ) good += test_dt1( dut, opm );
    if( run_dt2 ) good += test_dt2( dut, opm );
    if( run_pms ) good += test_pms( dut, opm );
    if( run_nolfo ) good += test_pg_nolfo( dut, opm );

    if( good!=0 ) {
        printf("FAIL\n");
    }
    return good;
}

int ref( opm_t* opm, int kc, int kf, int lfo, int pms, int dt1, int dt2, int mul ) {
    opm->ch_kc[0]  = kc;
    opm->ch_kf[0]  = kf;
    opm->ch_pms[0] = pms;
    opm->sl_dt2[0] = dt2;
    opm->lfo_pmd   = 1;
    opm->lfo_pm_lock = lfo;
    opm->sl_dt1[0] = dt1;
    opm->sl_mul[0] = mul;

    opm->cycles    = 32-7;
    OPM_PhaseCalcFNumBlock( opm );
    opm->cycles    = 0;
    OPM_PhaseCalcIncrement( opm );

    return opm->pg_inc[0];
}

void eval_dut( Vjt51_pg* dut, jt51_st& st, int kc, int kf, int lfo, int pms, int dt1, int dt2, int mul ) {
    dut->rst        = 0;
    dut->cen        = 1;
    dut->zero       = 0;
    dut->pm         = lfo;
    dut->pg_rst_III = 0;
    int phinc;
    for( int k=1; k<=32; k++ ) {
        dut->kc_I   = kc ; // k==1 ? kc : 0;
        dut->kf_I   = kf ; // k==1 ? kf : 0;
        dut->mul_VI = mul; // k==6 ? mul : 0;
        dut->dt1_II = dt1; // k==2 ? dt1 : 0;
        dut->dt2_I  = dt2; // k==1 ? dt2 : 0;
        dut->pms_I  = pms; // k==1 ? pms : 0;
        dut->clk=0;
        dut->eval();
        dut->clk=1;
        dut->eval();
        if( k==6 ) st.phinc = dut->phase_step_VII_out;
        if( k==1 ) st.kcode = dut->keycode_I_out;
        //if( dut->phinc_III_out != 0 ) printf("phinc=%d for k=%d\n",dut->phinc_III_out, k);
    }
}
/*
float expectedHz( int kc, int kf, int lfo, int pms, int dt1, int dt2, int mul ) {
    float f0 = 3.579545e6;
}
*/

// Runs against my own measurements
int test_pg_nolfo( Vjt51_pg& dut, opm_t& opm) {
    bool bad = false;
    jt51_st dut_st;

    int passed=0;
    int octave=3, note=4, kf=0, lfo=0, pms=0, dt1=0, dt2=0, mul=1;
    for( octave=0; octave<8; octave++ )
    for( note=0; note<4; note++ )
    //for( kf=0; kf<64; kf++ )
    for( dt1=0; dt1<4; dt1++ )
    //for( dt2=0; dt2<4; dt2++ )
    //for( mul=0; mul<16; mul++ )
    {
        int kc = (octave<<4) | note;
        int myref = ym_phaseinc( octave, note, kf, dt1, dt2, mul );
        int opminc= ref( &opm, kc, kf, lfo, pms, dt1, dt2, mul );
        eval_dut( &dut, dut_st, kc, kf, lfo, pms, dt1, dt2, mul );
        if( myref != dut_st.phinc ) {
            printf("%d %d %d %d %d %d: myref=%d <> %d=JT51 <> %d=OPM \n", octave, note, kf, dt1, dt2, mul,
                myref, dut_st.phinc, opminc );
            bad=true;
            //goto finish;
        }
    }
    finish:
    report("Measurements", bad);
    return bad;
}

int test_all( Vjt51_pg& dut, opm_t& opm ) {
    jt51_st dut_st;

    int passed=0;
    int kc=3, kf=0, lfo=0x80, pms=1, dt1=0, dt2=0, mul=0;
    for( kc=0; kc<128; kc++ )
    for( kf=0; kf<64; kf++ )
    for( lfo=0; lfo<256; lfo++ )
    for( pms=0; pms<7; pms++ )
    for( dt1=0; dt1<4; dt1++ )
    for( dt2=0; dt2<4; dt2++ )
    for( mul=0; mul<16; mul++ )
    {
        int ref_phinc = ref( &opm, kc, kf, lfo, pms, dt1, dt2, mul );
        eval_dut( &dut, dut_st, kc, kf, lfo, pms, dt1, dt2, mul );
        float ref_freq = phinc2freq( ref_phinc );
        float dut_freq = phinc2freq( dut_st.phinc );
        if( ref_phinc != dut_st.phinc ) {
            printf("%2X %2X // LFO=%2X PMS=%X // %X %X // MUL=%d -> FNUM=%X"
                   " // KCODE=%X <> %X"
                   " // PGINC=%5d <> %5d //  %.1fHz <> %.1fHz\n",
                     kc, kf,  lfo,pms, dt1, dt2, mul,
                     opm.pg_fnum[0],
                     opm.pg_kcode[0], dut_st.kcode,
                     ref_phinc, dut_st.phinc,
                     ref_freq, dut_freq );
            printf("FAIL\n");
            goto finish;
        }
        passed++;
    }
    printf("PASS\n");
    finish:
    return 0;
}

bool check_dt1( int oct, int note, int dt1, float delta ) {
    if( note > 4 ) return true; // ignore these cases
    note&=7;
    dt1&=3;
    float lut[8][4][4] = {
        0, 0, 0.053, 0.107, // oct=0, note=0
        0, 0, 0.053, 0.107, // oct=0, note=1
        0, 0, 0.053, 0.107, // oct=0, note=2
        0, 0, 0.053, 0.107, // oct=0, note=3

        0, 0.053, 0.107, 0.107, // oct=1, note=0
        0, 0.053, 0.107, 0.160, // oct=1, note=1
        0, 0.053, 0.107, 0.160, // oct=1, note=2
        0, 0.053, 0.107, 0.160, // oct=1, note=3

        0, 0.053, 0.107, 0.213, // oct=2, note=0
        0, 0.053, 0.160, 0.213, // oct=2, note=1
        0, 0.053, 0.160, 0.213, // oct=2, note=2
        0, 0.053, 0.160, 0.267, // oct=2, note=3

        0, 0.107, 0.213, 0.267, // oct=3, note=0
        0, 0.107, 0.213, 0.320, // oct=3, note=1
        0, 0.107, 0.213, 0.320, // oct=3, note=2
        0, 0.107, 0.267, 0.373, // oct=3, note=3

        0, 0.107, 0.267, 0.427, // oct=4, note=0
        0, 0.160, 0.320, 0.427, // oct=4, note=1
        0, 0.160, 0.320, 0.480, // oct=4, note=2
        0, 0.160, 0.320, 0.480, // oct=4, note=3

        0, 0.213, 0.427, 0.587, // oct=5, note=0
        0, 0.213, 0.427, 0.640, // oct=5, note=1
        0, 0.213, 0.480, 0.693, // oct=5, note=2
        0, 0.267, 0.533, 0.747, // oct=5, note=3

        0, 0.267, 0.587, 0.853, // oct=6, note=0
        0, 0.320, 0.640, 0.907, // oct=6, note=1
        0, 0.320, 0.693, 1.013, // oct=6, note=2
        0, 0.373, 0.747, 1.067, // oct=6, note=3

        0, 0.427, 0.853, 1.173, // oct=7, note=0
        0, 0.427, 0.907, 1.173, // oct=7, note=1
        0, 0.480, 1.013, 1.173, // oct=7, note=2
        0, 0.533, 1.067, 1.173  // oct=7, note=3
    };
    float r = lut[oct][note][dt1];
    if( delta < (r-0.01) || delta>(r+0.01) )
        return false; // bad
    else
        return true; // good
}

int test_dt1( Vjt51_pg& dut, opm_t& opm ) {
    bool bad=false;
    int kf=0, lfo=0, pms=0, dt1=0, dt2=0, mul=1;
    printf("Oct  Note        | DT1=0 | DT1=1 | DT1=2 | DT1=3\n");
    printf("-----------------|-------|-------|-------|------\n");
    for( int oct=0; oct<8; oct++ )
    for( int note=0; note<4; note++ )
    {
        int kc = (oct<<4) | note;
        dt1=0; dt2=0;
        jt51_st dut_st;
        int ref_phinc = ref( &opm, kc, kf, lfo, pms, dt1, dt2, mul );
        eval_dut( &dut, dut_st, kc, kf, lfo, pms, dt1, dt2, mul );
        float ref_base = phinc2freq( ref_phinc );
        float dut_base = phinc2freq( dut_st.phinc );

        printf("%d   %2d (%4.0f Hz) ", oct, note, dut_base );
        for( dt1=0; dt1<4; dt1++ ) {
            int ref_phinc = ref( &opm, kc, kf, lfo, pms, dt1, dt2, mul );
            eval_dut( &dut, dut_st, kc, kf, lfo, pms, dt1, dt2, mul );
            float ref_freq = phinc2freq( ref_phinc );
            float dut_freq = phinc2freq( dut_st.phinc );
            /*
            if( ref_phinc != dut_st.phinc ) {
                printf("%2X %2X // LFO=%2X PMS=%X // %X %X // MUL=%d -> FNUM=%X"
                       " // KCODE=%X <> %X"
                       " // PGINC=%5d <> %5d //  %.1fHz <> %.1fHz\n",
                         kc, kf,  lfo,pms, dt1, dt2, mul,
                         opm.pg_fnum[0],
                         opm.pg_kcode[0], dut_st.kcode,
                         ref_phinc, dut_st.phinc,
                         ref_freq, dut_freq );
                printf("FAIL\n");
                goto finish;
            }*/
            // print table
            printf("| %.3f", dut_freq-dut_base );
            if( check_dt1( oct, note, dt1, dut_freq-dut_base) )
                printf(" ");
            else {
                printf("*");
                bad=true;
            }
        }
        printf("\n");
    }
    finish:
    report("DT1",bad);
    return 0;
}

int test_dt2( Vjt51_pg& dut, opm_t& opm ) {
    bool bad=false;
    int kf=0, lfo=0, pms=0, dt1=0, dt2=0, mul=1, oct=4;
    printf("Oct  Note        | DT2=0 | DT2=1 | DT2=2 | DT2=3\n");
    printf("-----------------|-------|-------|-------|------\n");
    //for( int oct=7; oct<8; oct++ )
    for( int note=0; note<15; note++ )
    {
        int kc = (oct<<4) | note;
        dt1=0; dt2=0;
        jt51_st dut_st;
        int ref_phinc = ref( &opm, kc, kf, lfo, pms, dt1, dt2, mul );
        eval_dut( &dut, dut_st, kc, kf, lfo, pms, dt1, dt2, mul );
        float ref_base = phinc2freq( ref_phinc );
        float dut_base = phinc2freq( dut_st.phinc );

        printf("%d   %2d (%4.0f Hz) ", oct, note, dut_base );
        for( dt2=0; dt2<4; dt2++ ) {
            float dt_exp[4] = { 1.0, 1.41, 1.57, 1.73 };

            int ref_phinc = ref( &opm, kc, kf, lfo, pms, dt1, dt2, mul );
            eval_dut( &dut, dut_st, kc, kf, lfo, pms, dt1, dt2, mul );
            float ref_freq = phinc2freq( ref_phinc );
            float dut_freq = phinc2freq( dut_st.phinc );
            if( ref_phinc != dut_st.phinc ) {
                printf("%2X %2X // LFO=%2X PMS=%X // %X %X // MUL=%d -> FNUM=%X"
                       " // KCODE=%X <> %X"
                       " // PGINC=%5d <> %5d //  %.1fHz <> %.1fHz\n",
                         kc, kf,  lfo,pms, dt1, dt2, mul,
                         opm.pg_fnum[0],
                         opm.pg_kcode[0], dut_st.kcode,
                         ref_phinc, dut_st.phinc,
                         ref_freq, dut_freq );
                printf("FAIL\n");
                goto finish;
            }
            // print table
            float rel_dt = dut_freq/dut_base;
            printf("| %.3f", rel_dt );
            if( rel_dt > (dt_exp[dt2]+0.02) || rel_dt < (dt_exp[dt2]-0.02) ) {
                printf("*");
                bad=true;
            }
            else {
                printf(" ");
            }

        }
        printf("\n");
    }
    finish:
    report("DT2",bad);
    return 0;
}

void report( const char* sz, bool bad ) {
    if( bad )
        printf("%s FAIL\n\n", sz);
    else
        printf("%s PASS\n\n", sz);
}

int test_pms( Vjt51_pg& dut, opm_t& opm ) {
    bool bad=false;
    int kf=0, lfo=128, pms=0, dt1=0, dt2=0, mul=1;
    printf("LFO (note)    | PMS=0  | PMS=1  | PMS=2  | PMS=3  | PMS=4  | PMS=5  | PMS=6  | PMS=7 \n");
    printf("--------------|--------|--------|--------|--------|--------|--------|--------|-------\n");
    int oct=4, note=10;
    int lfo_lut[]={0, 31, 63, 127,160,210,255 };
    for( oct=0; oct<8; oct+=1 )
    for( note=0; note<16; note++ )
    for( lfo=0; lfo<256; lfo++ )
    //for( mul=0; mul<16; mul++ )
    //for( int lfok=0; lfok<7; lfok++ )
    {
        if( note&3 == 3 ) continue;
        //lfo=lfo_lut[lfok];
        int kc = (oct<<4) | note;
        dt1=0; dt2=0;
        jt51_st dut_st;
        int ref_phinc = ref( &opm, kc, kf, lfo, pms, dt1, dt2, mul );
        eval_dut( &dut, dut_st, kc, kf, lfo, pms, dt1, dt2, mul );
        float ref_base = phinc2freq( ref_phinc );
        float dut_base = phinc2freq( dut_st.phinc );

        printf("%3d (%4.0f Hz) ", lfo, dut_base );
        for( pms=0; pms<8; pms++ ) {
            float dt_exp[4] = { 1.0, 1.41, 1.57, 1.73 };

            int ref_phinc = ref( &opm, kc, kf, lfo, pms, dt1, dt2, mul );
            eval_dut( &dut, dut_st, kc, kf, lfo, pms, dt1, dt2, mul );
            float ref_freq = phinc2freq( ref_phinc );
            float dut_freq = phinc2freq( dut_st.phinc );
            // print table
            float delta_cent = cent( dut_freq, dut_base );
            float ref_cent = cent( ref_freq, ref_base );
            float error_freq = dut_freq-ref_freq;
            printf("|%6.0f ", delta_cent );
            //printf("|%6.0f ", error_freq);
            if( fabs(error_freq)>0  ) {
                printf("*");
                printf("\ndelta = %6.0f <> %6.0f\n", delta_cent, ref_cent);
                printf("\nBase freq = %.1f <> %f\n", dut_base, ref_base);
                printf("Mod  freq = %.1f <> %f\n", dut_freq, ref_freq);
                bad=true;
                goto finish;
            }
            else {
                printf(" ");
            }

        }
        printf("\n");
    }
    finish:
    report("PMS",bad);
    return 0;
}