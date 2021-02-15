#include "verilated_vcd_c.h"
#include "Vjt51_pg.h"
#include "opm.h"
#include <cstdio>

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
    return ((float)phinc)*CRYSTAL/32/(1<<20);
}

int main(int argc, char *argv[]) {
    Vjt51_pg dut;
    opm_t opm;
    jt51_st dut_st;

    OPM_Reset(&opm);

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