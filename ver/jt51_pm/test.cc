#include "verilated_vcd_c.h"
#include "Vjt51_pm.h"
#include <cstdio>

using namespace std;

int32_t OPM_LFOApplyPMS(int32_t lfo, int32_t pms);
int32_t OPM_CalcKCode(int32_t kcf, int32_t lfo, int32_t lfo_sign, int32_t dt);

int ref( int kc, int kf, int lfo, int pms, int dt );

int main(int argc, char *argv[]) {
    Vjt51_pm top;
    int passed=0, kc=0, kf=0, pm=0, pms=0, dt=0;
    for( kc=0; kc<128; kc++ )
    for( kf=0; kf<64; kf++ )
    for( pm=0; pm<256; pm++ )
    for( pms=0; pms<8; pms++ )
    for( dt=0; dt<4; dt++ )
    {
        top.kc = kc;
        top.kf = kf;
        top.pm = pm;
        top.pms = pms;
        top.dt = dt;
        top.eval();
        int kcex = top.kcex;
        int ref_kc = ref( kc, kf, pm, pms, dt);
        if( kcex != ref_kc ) {
            printf("Error: ");
            printf("%2X %2X // %2X %X // %X -> RTL=%X <> %X=C++\n", kc, kf, pm, pms, dt, kcex, ref_kc );
            printf("%d ok\n", passed);
            goto finish;
        }
        passed++;
    }
    printf("PASS\n");
    finish:
    return 0;
}

int ref( int kc, int kf, int lfo, int pms, int dt ) {
    uint32_t kcf = (kc<< 6) + kf;
    int32_t lfo_pm = OPM_LFOApplyPMS(lfo & 127, pms);
    uint32_t kcode = OPM_CalcKCode(kcf, lfo_pm, (lfo & 0x80) != 0 && pms != 0 ? 0 : 1, dt);
    return kcode;
}

int32_t OPM_LFOApplyPMS(int32_t lfo, int32_t pms)
{
    int32_t t, out;
    int32_t top = (lfo >> 4) & 7;
    if (pms != 7)
    {
        top >>= 1;
    }
    t = (top & 6) == 6 || ((top & 3) == 3 && pms >= 6);
    //printf("ref: t=%d\n",t);

    out = top + ((top >> 2) & 1) + t;
    out = out * 2 + ((lfo >> 4) & 1);

    if (pms == 7)
    {
        out >>= 1;
    }
    out &= 15;
    out = (lfo & 15) + out * 16;
    //printf("ref:pre-scaled=%x\n",out);
    switch (pms)
    {
    case 0:
    default:
        out = 0;
        break;
    case 1:
        out = (out >> 5) & 3;
        break;
    case 2:
        out = (out >> 4) & 7;
        break;
    case 3:
        out = (out >> 3) & 15;
        break;
    case 4:
        out = (out >> 2) & 31;
        break;
    case 5:
        out = (out >> 1) & 63;
        break;
    case 6:
        out = (out & 255) << 1;
        break;
    case 7:
        out = (out & 255) << 2;
        break;
    }
    //printf("ref:scaled=%x\n",out);
    return out;
}

int32_t OPM_CalcKCode(int32_t kcf, int32_t lfo, int32_t lfo_sign, int32_t dt)
{
    int32_t t2, t3, b0, b1, b2, b3, w2, w3, w6;
    int32_t overflow1 = 0;
    int32_t overflow2 = 0;
    int32_t negoverflow = 0;
    int32_t sum, cr;
    if (!lfo_sign)
    {
        lfo = ~lfo;
    }
    sum = (kcf & 8191) + (lfo&8191) + (!lfo_sign);
    cr = ((kcf & 255) + (lfo & 255) + (!lfo_sign)) >> 8;
    if (sum & (1 << 13))
    {
        overflow1 = 1;
    }
    sum &= 8191;
    if (lfo_sign && ((((sum >> 6) & 3) == 3) || cr))
    {
        //printf("ref: +64 (was %X)\n",sum);
        sum += 64;
    }
    if (!lfo_sign && !cr)
    {
        //printf("ref: negoverflow\n");
        sum += (-64)&8191;
        negoverflow = 1;
    }
    if (sum & (1 << 13))
    {
        //printf("ref: overflow2 set, sum=%X\n",sum);
        overflow2 = 1;
    }
    sum &= 8191;
    if ((!lfo_sign && !overflow1) || (negoverflow && !overflow2))
    {
        //printf("ref: sum underflow\n");
        sum = 0;
    }
    if (lfo_sign && (overflow1 || overflow2))
    {
        //printf("ref: sum overflow\n");
        sum = 8127;
    }
    //printf("ref: sum=%X\n",sum);

    t2 = sum & 63;
    if (dt == 2)
        t2 += 20;
    if (dt == 2 || dt == 3)
        t2 += 32;

    b0 = (t2 >> 6) & 1;
    b1 = dt == 2;
    b2 = ((sum >> 6) & 1);
    b3 = ((sum >> 7) & 1);


    w2 = (b0 && b1 && b2);
    w3 = (b0 && b3);
    w6 = (b0 && !w2 && !w3) || (b3 && !b0 && b1);

    t2 &= 63;

    t3 = (sum >> 6) + w6 + b1 + (w2 || w3) * 2 + (dt == 3) * 4 + (dt != 0) * 8;
    if (t3 & 128)
    {
        t2 = 63;
        t3 = 126;
    }
    sum = t3 * 64 + t2;
    return sum;
}