#ifndef __JT51_TABLES_H
#define __JT51_TABLES_H

#define PI 3.14159265358979323846

#define ENV_BITS		10
#define ENV_LEN			(1<<ENV_BITS)
#define ENV_STEP		(128.0/ENV_LEN)

#define SIN_BITS		10
#define SIN_LEN			(1<<SIN_BITS)
#define SIN_MASK		(SIN_LEN-1)

#define TL_RES_LEN		(256) /* 8 bits addressing (real chip) */

/*	TL_TAB_LEN is calculated as:
*	13 - sinus amplitude bits     (Y axis)
*	2  - sinus sign bit           (Y axis)
*	TL_RES_LEN - sinus resolution (X axis)
*/
extern signed int tl_tab[TL_RES_LEN];
extern const int M1, C1, M2, C2;

#define ENV_QUIET		(TL_TAB_LEN>>3)

/* sin waveform table in 'decibel' scale */
extern unsigned int sin_tab[SIN_LEN];
extern int phaseinc_rom[768];


/* translate from D1L to volume index (16 D1L levels) */
//extern unsigned d1l_tab[16];

struct YMval{
  int lin, exact, man, exp;
  YMval() { lin=exact=man=0; exp=1; }
};

int ym_flotante( int lin );
void canal( int con, int op_mask, int tl, int fl, int* out, int len, bool exact=false );
void canal_tubo( int con, int op_mask, int tl, int fl, int* out, int len, bool exact=false, bool verbose=true );
void canal_2612( int con, int op_mask, int tl, int fl, int* out, int len, bool exact=false, bool verbose=true );

void init_tables(void);
void print_sine_table();
int reduce_nota(int nota);
void print_dt1_table(int dt1, int dt2);
void print_pow2man();
YMval ym_sin( int phase, int tl, int eg );
int ym_phaseinc( int octave, int note, int kf, int dt1, int dt2, int mul );
int indice( int finc, unsigned mul );
//void vuelca_dt1_tl();
void print_phaseinc_rom();

int kc_alt(int kc, int dt2, bool );
#endif
