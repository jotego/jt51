#include "tables.h"
#include <cmath>
#include <iostream>
using namespace std;

const int M1=1, C1=2, M2=4, C2=8;

signed int tl_tab[TL_RES_LEN];
unsigned int sin_tab[SIN_LEN];
unsigned d1l_tab[16];

int phaseinc_rom[768]={
1299,1300,1301,1302,1303,1304,1305,1306,1308,1309,1310,1311,1313,1314,1315,1316,
1318,1319,1320,1321,1322,1323,1324,1325,1327,1328,1329,1330,1332,1333,1334,1335,
1337,1338,1339,1340,1341,1342,1343,1344,1346,1347,1348,1349,1351,1352,1353,1354,
1356,1357,1358,1359,1361,1362,1363,1364,1366,1367,1368,1369,1371,1372,1373,1374,
1376,1377,1378,1379,1381,1382,1383,1384,1386,1387,1388,1389,1391,1392,1393,1394,
1396,1397,1398,1399,1401,1402,1403,1404,1406,1407,1408,1409,1411,1412,1413,1414,
1416,1417,1418,1419,1421,1422,1423,1424,1426,1427,1429,1430,1431,1432,1434,1435,
1437,1438,1439,1440,1442,1443,1444,1445,1447,1448,1449,1450,1452,1453,1454,1455,
1458,1459,1460,1461,1463,1464,1465,1466,1468,1469,1471,1472,1473,1474,1476,1477,
1479,1480,1481,1482,1484,1485,1486,1487,1489,1490,1492,1493,1494,1495,1497,1498,
1501,1502,1503,1504,1506,1507,1509,1510,1512,1513,1514,1515,1517,1518,1520,1521,
1523,1524,1525,1526,1528,1529,1531,1532,1534,1535,1536,1537,1539,1540,1542,1543,
1545,1546,1547,1548,1550,1551,1553,1554,1556,1557,1558,1559,1561,1562,1564,1565,
1567,1568,1569,1570,1572,1573,1575,1576,1578,1579,1580,1581,1583,1584,1586,1587,
1590,1591,1592,1593,1595,1596,1598,1599,1601,1602,1604,1605,1607,1608,1609,1610,
1613,1614,1615,1616,1618,1619,1621,1622,1624,1625,1627,1628,1630,1631,1632,1633,
1637,1638,1639,1640,1642,1643,1645,1646,1648,1649,1651,1652,1654,1655,1656,1657,
1660,1661,1663,1664,1666,1667,1669,1670,1672,1673,1675,1676,1678,1679,1681,1682,
1685,1686,1688,1689,1691,1692,1694,1695,1697,1698,1700,1701,1703,1704,1706,1707,
1709,1710,1712,1713,1715,1716,1718,1719,1721,1722,1724,1725,1727,1728,1730,1731,
1734,1735,1737,1738,1740,1741,1743,1744,1746,1748,1749,1751,1752,1754,1755,1757,
1759,1760,1762,1763,1765,1766,1768,1769,1771,1773,1774,1776,1777,1779,1780,1782,
1785,1786,1788,1789,1791,1793,1794,1796,1798,1799,1801,1802,1804,1806,1807,1809,
1811,1812,1814,1815,1817,1819,1820,1822,1824,1825,1827,1828,1830,1832,1833,1835,
1837,1838,1840,1841,1843,1845,1846,1848,1850,1851,1853,1854,1856,1858,1859,1861,
1864,1865,1867,1868,1870,1872,1873,1875,1877,1879,1880,1882,1884,1885,1887,1888,
1891,1892,1894,1895,1897,1899,1900,1902,1904,1906,1907,1909,1911,1912,1914,1915,
1918,1919,1921,1923,1925,1926,1928,1930,1932,1933,1935,1937,1939,1940,1942,1944,
1946,1947,1949,1951,1953,1954,1956,1958,1960,1961,1963,1965,1967,1968,1970,1972,
1975,1976,1978,1980,1982,1983,1985,1987,1989,1990,1992,1994,1996,1997,1999,2001,
2003,2004,2006,2008,2010,2011,2013,2015,2017,2019,2021,2022,2024,2026,2028,2029,
2032,2033,2035,2037,2039,2041,2043,2044,2047,2048,2050,2052,2054,2056,2058,2059,
2062,2063,2065,2067,2069,2071,2073,2074,2077,2078,2080,2082,2084,2086,2088,2089,
2092,2093,2095,2097,2099,2101,2103,2104,2107,2108,2110,2112,2114,2116,2118,2119,
2122,2123,2125,2127,2129,2131,2133,2134,2137,2139,2141,2142,2145,2146,2148,2150,
2153,2154,2156,2158,2160,2162,2164,2165,2168,2170,2172,2173,2176,2177,2179,2181,
2185,2186,2188,2190,2192,2194,2196,2197,2200,2202,2204,2205,2208,2209,2211,2213,
2216,2218,2220,2222,2223,2226,2227,2230,2232,2234,2236,2238,2239,2242,2243,2246,
2249,2251,2253,2255,2256,2259,2260,2263,2265,2267,2269,2271,2272,2275,2276,2279,
2281,2283,2285,2287,2288,2291,2292,2295,2297,2299,2301,2303,2304,2307,2308,2311,
2315,2317,2319,2321,2322,2325,2326,2329,2331,2333,2335,2337,2338,2341,2342,2345,
2348,2350,2352,2354,2355,2358,2359,2362,2364,2366,2368,2370,2371,2374,2375,2378,
2382,2384,2386,2388,2389,2392,2393,2396,2398,2400,2402,2404,2407,2410,2411,2414,
2417,2419,2421,2423,2424,2427,2428,2431,2433,2435,2437,2439,2442,2445,2446,2449,
2452,2454,2456,2458,2459,2462,2463,2466,2468,2470,2472,2474,2477,2480,2481,2484,
2488,2490,2492,2494,2495,2498,2499,2502,2504,2506,2508,2510,2513,2516,2517,2520,
2524,2526,2528,2530,2531,2534,2535,2538,2540,2542,2544,2546,2549,2552,2553,2556,
2561,2563,2565,2567,2568,2571,2572,2575,2577,2579,2581,2583,2586,2589,2590,2593
};

void print_phaseinc_rom() {
    for(int nota=0; nota<16; nota++ )
    for(int kf=0; kf<64; kf++ ) {
    	int kc = (nota<<6) | kf;
        int notax;
		switch( nota ) {
        	case 0: notax = 0; break;
            case 1: notax = 1; break;
            case 2: notax = 2; break;
        	case 3: notax = 2; break;
        	case 4: notax = 3; break;
            case 5: notax = 4; break;
            case 6: notax = 5; break;
        	case 7: notax = 5; break;
        	case 8: notax = 6; break;
            case 9: notax = 7; break;
            case 10: notax = 8; break;
        	case 11: notax = 8; break;
        	case 12: notax = 9; break;
            case 13: notax = 10; break;
            case 14: notax = 11; break;
        	case 15: notax = 11; break;
        }
        int j = (notax<<6)|kf;
        cout << "10'd" << kc << ":\t phinc <= { 12'd" << phaseinc_rom[j] << "\t}; // nota = " << nota << ", KF = " << kf << '\n';
        j++;
    }
}

int pow2man[16];

void haz_pow2man() {
	for( int k=0; k<16; k++ ) {
		double f = ((double)k)/16;
		pow2man[k] = 16*pow( 2.0, f );
	}
}

void print_pow2man() {
	cout << "case( pow2ind )\n";
	for( int k=0; k<16; k++ ) {
		cout << "\t4'd" << k << ": pow2 <= 4'd" << pow2man[k] << ";\n";
	}
	cout << "endcase\n";
}

int reduce_nota(int nota) {
	return (nota>>2)*3 + (nota&3);
}

void show_kc( int kc ) {
	int o = kc>>10;
	int n = (kc>>6)&15;
	int kf= kc&63;
	cout << o << "-" << n << "-" << kf << '\t';
}

int kc_alt(int kc, int dt2, bool oct_con15 ) {
//	show_kc(kc);
	int n = (kc>>6)&15;

    if( (n&3)==3 ) kc+=64;
	int o = kc>>10;
    //if( oct_con15 ) o = kc>>10; // se guarda el salto de octava si era la nota 15
    int corto=kc&0x3ff;
    int resta = (kc>>8)&3;
    corto -= 64*resta;
    switch( dt2 ) {
    	case 1: corto+=384; break;
        case 2: corto+=500; break;
        case 3: corto+=608; break;
    }
    int extra = 0;
    if( corto>767 ) {extra++; corto-=768;}
    // vuelve al formato original
    switch( (corto>>6) ) {
     	case 0:
        case 1:
        case 2: break;
    	case 3:
        case 4:
        case 5: corto += 64; break;
        case 6:
        case 7:
        case 8: corto += 128; break;
        case 9:
        case 10:
        case 11: corto += 192; break;
        default: cout << "!!";
    }
    int alt = ((o+extra)<<10) | corto;
    if( alt > 8191 ) alt = 8191;
//	show_kc( alt );	cout << endl;
    return alt;
}

int calc_dt1(int kc, int dt1, int dt2) {
	int dt1_finc=0;
	int dt1_lsb = dt1&3;
//	cout << "DT1 = " << dt1 << " DT1 LSB = " << dt1_lsb << endl;
	if( dt1_lsb ) {
		int signo = dt1&4 ? -1 : 1;
        int key = kc_alt( kc, dt2, false );
//        if( (nota&3)==3) key+=64;
       /* switch( nota ) {
        	case 3: key+=64;
            case 7: key+=64*30;
        }*/
		int base = 0;
		switch(dt1_lsb) {
			case 1: base = -1*(1<<10); break;
			case 2: base = 1<<10; break;
			case 3: base = 1<<11; break;
		}
//		cout << "key=" << key << "\t base=" << base << endl;
		key += base;
		key &= ~0xff; // los ultimos 8 bits a cero
//		cout << "----------------------------\n";
//		cout << "key=" << key << endl;
//		if(key>=0)
		{
			int ind = (key>>7)&0xf;
			int exp = (key>>11)&7;
//			if( ind&1 ) cout << "El indice es impar!\n";
//			cout << "ind=" << dec << ind << "\t exp=" << exp << "\t tabla=" << pow2man[ind] << dec << endl;
			if( exp>5 )
				dt1_finc=0;
			else
				dt1_finc = (pow2man[ind]<<exp)>>4;
//			cout << "dt1_finc=" << dt1_finc << endl;
			if( dt1_finc>8 && dt1_lsb==1 ) dt1_finc=8;
			if( dt1_finc>16 && dt1_lsb==2 ) dt1_finc=16;
			if( dt1_finc>22 && dt1_lsb==3 ) dt1_finc=22;
//			cout << "dt1_finc=" << dt1_finc << endl;
			dt1_finc = signo*dt1_finc;
		}
	}
	return dt1_finc;
}

void print_dt1_table(int dt1, int dt2) {
	int ult=-1;
	for( int octave=0;octave<8; octave++ )
	for( int note=0; note<16; note++ ) {
		if ((note&3)==3) continue;
		for( int kf=0; kf<64; kf++ ) {
			int este = calc_dt1( (octave*16+note)*64+kf, dt1, dt2);
			if( este != ult ) {
				cout << octave <<'\t'<< note <<'\t'<< kf <<"\t(0x";
				cout << hex << (octave*16+note)*64+kf << ")\t" << dec;
				cout << este << endl;
				ult=este;
			}
		}
	}
}

int ym_phaseinc( int octave, int note, int kf, int dt1, int dt2, int mul ) {
	int notex=note;
	int kc_base = (((octave<<4)|note)<<6)|kf;
	switch(note) {
		case 3:  notex=3; break;
		case 4:  notex=3; break;
		case 5:  notex=4; break;
		case 6:  notex=5; break;
		case 7:  notex=6; break;
		case 8:  notex=6; break;
		case 9:  notex=7; break;
		case 10: notex=8; break;
		case 11: notex=9; break;
		case 12: notex=9; break;
		case 13: notex=10; break;
		case 14: notex=11; break;
		case 15: { notex=0; octave++; break;}
	}
	unsigned i = (notex<<6) + kf;
	switch( dt2 ) {
		case 1: i += 384; break;
		case 2: i += 500; break; // 445 - 450 max
		case 3: i += 608; break;
	}
	int f=0;
//	cout << "indice LUT incrementos = " << i << endl;
	if( i>=768 ) { i-=768; f=1; }
//	cout << "indice LUT incrementos = " << i << endl;
	int finc = phaseinc_rom[i]<<f;
//	cout << "finc = " << finc << endl;
	if( octave<2 ) finc >>= (2-octave);
	if( octave>2 ) finc <<= (octave-2);
//	cout << "finc = " << finc << endl;
	if( finc>82976 ) finc=82976;
//	cout << "finc = " << finc << endl;
	int dt1_finc=calc_dt1( kc_base , dt1, dt2);
//	cout << "pre MUL, DT1_finc=" << dt1_finc << endl;
	finc +=dt1_finc;
	if( mul==0 ) finc>>=1; else finc*=mul;

  	//int mascara = (1<<20)-1;
  	//cout << "Mascara = " << mascara << endl;
  	//finc &= mascara; // 20 bits
  	if( finc >= 1024*1024 ) finc-=1024*1024;
	return finc;
}

// reduce la resolucon de un numero al pasarlo por el filtro
// de coma flotante del YM
int ym_flotante( int lin ) {
  int man=lin,exp=1;
  while( man>511 || man<-512) {
    man >>= 1;
    exp++;
  }
  return man << (exp-1);
}

YMval ym_sin( int phase, int tl, int eg ) {
  YMval ymv;
  phase &= 0x3FF;
  // uso solo los primeros 256 valores de la tabla
  int phase4 = phase&0xff;
  if (phase&0x100) phase4=(~phase4)&0xff;
  int logval = sin_tab[phase4]; // 13 bits + 1 de signo
  // quita el bit de signo
  //int sign = logval&1;
  int sign = phase>>9;
  tl <<= 5; // *32
  logval>>=1;
  // atenua
  logval += tl;
  logval += eg;
  // lo pasa a lineal
  int lin = tl_tab[ logval&0xFF ];
  for( int k=(logval>>8); k; k-- ) lin>>=1;
  if( sign ) lin *= -1;
  // convierte a formato exponencial
  int man=lin,exp=1;
  while( man>511 || man<-512) {
    man >>= 1;
    exp++;
  }
  ymv.exact = lin;
  ymv.man   = man;
  ymv.exp   = exp;
  ymv.lin   = man << (exp-1);
  return ymv;
}


/*

El canal se calcula en el orden: C1 C2 M1 M2 (suma) C1 C2 M1 M2 (suma)  C1 C2 M1 M2 (suma)
pero el keyon entra siempre en este orden M2 C1 C2 M1, o sea que pilla a M2 una muestra antes
que al resto de operadores.


*/

int fase_desfase( int fase, int desfase ) {
	return	fase + (desfase>>1);
}


/*
	Calculo al estilo de YM2612, operadores en orden M1, M2, C1, C2

	Para TL grandes (mucha atenuacion) hay pequeñas diferencias con los
	resultados calculando de la otra forma.
	Este método usa menos memoria para almacenar resultados intermedios,
	solo int[3+n] en total, mientras que canal_tubo usa int[16]. Digo
	que usa 3+n porque hay 3 explícitos pero luego un retraso de n etapas
	en el pipeline que es la forma de tener accesible el resultado de
	M2 para C2. Ese n para YM2612 es de 6, y para YM2151 debería ser 8.
	O sea que en total se usarían 11 registros en vez de 16. Algo ahorra.

	El keyon ocurre justo en el calculo de M2 y la suma del resultado
	justo tras el calculo de M2. Eso evita tener que hacer ajustes con
	el tiempo en que los operadores empiezan. Todos empiezan en origen
	a la vez, solo que la señal pilla el pipeline para cuando se esta
	calculando M2. La suma de la salida no esta sincronizada y sencillamente
	coincide con el fin de M2. No creo que nada de esto sea por diseño,
	coincide y punto. Es la explicación más sencilla.

*/

void canal_2612( int con, int op_mask, int tl, int fl, int* out, int len, bool exact, bool verbose ) {
	int cnt_m1=-1, cnt_m2=0, cnt_c1=0, cnt_c2=0;
	YMval out_m1, out_c1, out_m2, out_c2;
	if(verbose) cout << "M1\t\t\tM2\t\t\tC1\t\t\tC2\t\t\tOutput\n";
    struct shr {
	    int reg[3];
	    shr() { for(int k=0; k<3; k++) reg[k]=0; }
	    void operator<<(int a) {
		    for(int k=2; k>0; k--) reg[k]=reg[k-1];
		    reg[0]=a;
	    }
    }sh;
    int sh_alt=0;
	int paso=0;
	while( paso<len ) {
		int desfase_c1=0, desfase_m2=0, desfase_c2=0;

        /////////// M1
		if( (op_mask & M1) && (cnt_m1>=0) ) {
			int phase = (cnt_m1<<10);
			////if( fl ) phase += ( (old[1]+old[0])<<fl);
			if( fl ) phase += ( (sh.reg[0]+sh.reg[1])<<fl);
			out_m1 = ym_sin( phase>>10, tl, 0 );
			if(verbose) cout << cnt_m1 << '\t' << (phase>>10) << '\t' << out_m1.exact << '\t';
		}
		else if(verbose) cout << "\t\t\t";
        sh << out_m1.exact;
        cnt_m1++;

        //////////// M2
		switch( con ) {
			case 0: desfase_m2 = sh_alt; break;
			case 1: desfase_m2 = sh.reg[1]+sh_alt; break;
			case 2: desfase_m2 = sh_alt; break;
			case 5: desfase_m2 = sh.reg[1]; break;
			default:desfase_m2 = 0;
		}
		if( cnt_m2>=0 && (op_mask & M2) ) {
			int phase = fase_desfase(cnt_m2, desfase_m2);
			out_m2 = ym_sin( phase, tl, 0 );
			if(verbose) cout << cnt_m2 << '\t' << desfase_m2 << '\t' << out_m2.exact << '\t';
		}
		else if(verbose) cout << "\t\t\t";
		cnt_m2++;

		int t = 0;
		switch( con ) {
			case 4:
				t = out_c1.exact + out_c2.exact;
				break;
			case 5:
			case 6:
				t = out_m2.exact + out_c1.exact + out_c2.exact;
				break;
			case 7:
				t = out_m1.exact + out_m2.exact + out_c1.exact + out_c2.exact;
				break;
			default: t = out_c2.exact; break;
		}

		//////////// C1
		switch( con ) {
			case 0: desfase_c1 = sh.reg[0]; break;
			case 3: desfase_c1 = sh.reg[0]; break;
			case 4: desfase_c1 = sh.reg[0]; break;
			case 5: desfase_c1 = sh.reg[0]; break;
			case 6: desfase_c1 = sh.reg[0]; break;
			default: desfase_c1 = 0;
		}
		if( (op_mask & C1) && (cnt_c1>=0) ) {
			int phase = fase_desfase(cnt_c1,desfase_c1);
			out_c1 = ym_sin( phase, tl, 0 );
			if(verbose) cout << cnt_c1 << '\t' << desfase_c1 << '\t' << out_c1.exact << '\t';
		}
		else if(verbose) cout << "\t\t\t";
		cnt_c1++;

        //////////// C2
		switch( con ) {
			case 0: desfase_c2 = out_m2.exact; break;
			case 1: desfase_c2 = out_m2.exact; break;
			case 2: desfase_c2 = sh.reg[0]+ out_m2.exact; break;
			case 3: desfase_c2 = sh_alt+out_m2.exact; break;
			case 4: desfase_c2 = out_m2.exact; break;
			case 5: desfase_c2 = sh.reg[0]; break;
			default:desfase_c2 = 0;
		}
		if( (op_mask & C2) && (cnt_c2>=0) ) {
			int phase = fase_desfase(cnt_c2, desfase_c2);
			out_c2 = ym_sin( phase, tl, 0 );
			if(verbose) cout << cnt_c2 << '\t' << desfase_c2 << '\t' << out_c2.exact << '\t';
		}
		else if(verbose) cout << "\t\t\t";
		cnt_c2++;

		sh_alt = out_c1.exact; // aunque solo se usa si con<=3
		out[paso++]= exact ? t : ym_flotante(t);
		if(verbose) cout << t << '\n';
	}
	if(verbose) cout << "\n";
}

void canal_tubo( int con, int op_mask, int tl, int fl, int* out, int len, bool exact, bool verbose ) {
	int cnt_m1=-2, cnt_m2=-1, cnt_c1=-2, cnt_c2=-2;
	YMval out_m1, out_c1, out_m2, out_c2;
	if(verbose) cout << "M1\t\t\tM2\t\t\tC1\t\t\tC2\t\t\tOutput\n";
    struct shr {
	    int reg[16];
	    shr() { for(int k=0; k<16; k++) reg[k]=0; }
	    void operator<<(int a) {
		    for(int k=15; k>0; k--) reg[k]=reg[k-1];
		    reg[0]=a;
	    }
    }sh;

	while( cnt_m2<len-1 ) {
		int desfase_c1=0, desfase_m2=0, desfase_c2=0;

		//////////// C1
		switch( con ) {
			case 0:
			case 3:
			case 4:
			case 5:
			case 6:  desfase_c1 = sh.reg[1]/*M1*/; break; // desfase_c1 = old[0];
			default: desfase_c1 = 0;
		}
		if( (op_mask & C1) && (++cnt_c1>=0) ) {
			int phase = fase_desfase(cnt_c1,desfase_c1);
			out_c1 = ym_sin( phase, tl, 0 );
			if(verbose) cout << cnt_c1 << '\t' << desfase_c1 << '\t' << out_c1.exact << '\t';
		}
		else if(verbose) cout << "\t\t\t";
		sh << out_c1.exact;

        //////////// C2
		switch( con ) {
			case 0:
			case 1:
			case 4: desfase_c2 = sh.reg[1] /*M2*/; break;
			case 2: desfase_c2 = sh.reg[1]/*M2*/+sh.reg[2]/*M1*/; break;
			case 3: desfase_c2 = sh.reg[4] /* C1 */+ sh.reg[1] /* M2 */; break;//old_c1+out_m2.exact; break;
			case 5: desfase_c2 = sh.reg[2]/*M1*/;	break;
			default:desfase_c2 = 0;
		}
		if( (op_mask & C2) && (++cnt_c2>=0) ) {
			int phase = fase_desfase(cnt_c2, desfase_c2);
			out_c2 = ym_sin( phase, tl, 0 );
			if(verbose) cout << cnt_c2 << '\t' << desfase_c2 << '\t' << out_c2.exact << '\t';
		}
		else if(verbose) cout << "\t\t\t";
		sh << out_c2.exact;

        /////////// M1
		if( (op_mask & M1) && (++cnt_m1>=0) ) {
			int phase = (cnt_m1<<10);
			////if( fl ) phase += ( (old[1]+old[0])<<fl);
			if( fl ) phase += ( (sh.reg[3]+sh.reg[7])<<fl);
			out_m1 = ym_sin( phase>>10, tl, 0 );
			if(verbose) cout << cnt_m1 << '\t' << (phase>>10) << '\t' << out_m1.exact << '\t';
		}
		else if(verbose) cout << "\t\t\t";
        sh << out_m1.exact;
        //////////// M2
		switch( con ) {
			case 0: desfase_m2 = sh.reg[2]; /*out_c1.exact;*/	break;
			case 1: desfase_m2 = sh.reg[2]/*C1*/+sh.reg[4];/*old[1]*/; break;
			case 2: desfase_m2 = sh.reg[2];/* old_c1;*/	break;
			case 5: desfase_m2 = sh.reg[4] /*M1*/; break;
			default:desfase_m2 = 0;
		}
		cnt_m2++;
		if( op_mask & M2 ) {
			int phase = fase_desfase(cnt_m2, desfase_m2);
			out_m2 = ym_sin( phase, tl, 0 );
			if(verbose) cout << cnt_m2 << '\t' << desfase_m2 << '\t' << out_m2.exact << '\t';
		}
		else if(verbose) cout << "\t\t\t";
        sh << out_m2.exact;

		int t = 0;
		switch( con ) {
			case 4:
				t = out_c1.exact + out_c2.exact;
				break;
			case 5:
			case 6:
				t = out_m2.exact + out_c1.exact + out_c2.exact;
				break;
			case 7:
				t = out_m1.exact + out_m2.exact + out_c1.exact + out_c2.exact;
				break;
			default: t = out_c2.exact; break;
		}
		out[cnt_m2]= exact ? t : ym_flotante(t);
		if(verbose) cout << t << '\n';
	}
	if(verbose) cout << "\n";
}


void canal( int con, int op_mask, int tl, int fl, int* out, int len, bool exact ) {
	int old[2] = { 0,0 };
	int old_m2 = 0, old_c1 = 0;
	int cnt_m1=-1, cnt_m2=0, cnt_c1=-1, cnt_c2=-1;
	if( con<=2 ) cnt_c1=0;
	if( con==0 ) cnt_m1=0;
	while( cnt_m2<len ) {
		int desfase_c1=0, desfase_m2=0, desfase_c2=0;
		switch( con ) {
			case 0:
				desfase_c1 = ( old[0]<<9 )&0xfffff;
				desfase_m2 = ( old_c1<<9 )&0xfffff;
				desfase_c2 = ( old_m2<<9 )&0xfffff;
				break;
			case 1:
				desfase_m2 = ( (old_c1+old[0])<<9 )&0xfffff;
				desfase_c2 = ( (old_m2)<<9 )&0xfffff;
				break;
			case 2:
				desfase_m2 = ( old_c1<<9 )&0xfffff;
				desfase_c2 = ( (old_m2+old[0])<<9 )&0xfffff;
				break;
			case 3: desfase_c1 = ( old[0]<<9 )&0xfffff;
					desfase_c2 = ( (old_c1+old_m2)<<9 )&0xfffff;
					break;
			case 4: desfase_c1 = ( old[0]<<9 )&0xfffff;
					desfase_c2 = ( old_m2<<9 )&0xfffff;
					break;
			case 5: desfase_c1 = ( old[0]<<9 )&0xfffff;
					desfase_m2 = desfase_c1;
					desfase_c2 = desfase_c1;
					break;
			case 6: desfase_c1 = ( old[0]<<9 )&0xfffff; break;
		}
		YMval out_m1, out_c1, out_m2, out_c2;
		if( (op_mask & M1) && (cnt_m1>=0) ) {
			int phase = (cnt_m1<<10);
			if( fl ) phase += ( (old[1]+old[0])<<fl);
			old[1] = old[0];
			out_m1 = ym_sin( phase>>10, tl, 0 );
			old[0] = out_m1.exact;
		}
		if( (op_mask & C1) && (cnt_c1>=0) ) {
			int phase = ((cnt_c1<<10) + desfase_c1)>>10;
			out_c1 = ym_sin( phase, tl, 0 );
			old_c1 = out_c1.exact;
		}
		if( op_mask & M2 ) {
			int phase = ((cnt_m2<<10) + desfase_m2)>>10;
			out_m2 = ym_sin( phase, tl, 0 );
			old_m2 = out_m2.exact;
		}
		if( (op_mask & C2) && (cnt_c2>=0) ) {
			int phase = ((cnt_c2<<10) + desfase_c2)>>10;
			out_c2 = ym_sin( phase, tl, 0 );
		}

		int t = 0;
		switch( con ) {
			case 4:
				t = out_c1.exact + out_c2.exact;
				break;
			case 5:
			case 6:
				t = out_m2.exact + out_c1.exact + out_c2.exact;
				break;
			case 7:
				t = out_m1.exact + out_m2.exact + out_c1.exact + out_c2.exact;
				break;
			default: t = out_c2.exact; break;
		}
		out[cnt_m2]= exact ? t : ym_flotante(t);
		cnt_m2++; cnt_m1++; cnt_c2++; cnt_c1++;
	}
}


void print_sine_table() {
	for( int j=0; j<1024; j++ ) {
		YMval ymv = ym_sin( j, 0, 0 );
		cout << j << ' ' << ymv.exact << ' ' << ymv.lin << '\n';
	}
}

void init_tables(void)
{
	haz_pow2man();

	signed int i,x,n;
	double o,m;

	for (x=0; x<TL_RES_LEN; x++)
	{
		m = (1<<16) / pow(2, (x+1) * (ENV_STEP/4.0) / 8.0);
		m = floor(m);

		/* we never reach (1<<16) here due to the (x+1) */
		/* result fits within 16 bits at maximum */

		n = (int)m;		/* 16 bits here */
		n >>= 4;		/* 12 bits here */
		if (n&1)		/* round to closest */
			n = (n>>1)+1;
		else
			n = n>>1;
						/* 11 bits here (rounded) */
		n <<= 2;		/* 13 bits here (as in real chip) */
		tl_tab[ x ] = n;
	}

	for (i=0; i<SIN_LEN; i++)
	{
		/* non-standard sinus */
		m = sin( ((i*2)+1) * PI / SIN_LEN ); /* verified on the real chip */

		/* we never reach zero here due to ((i*2)+1) */

		if (m>0.0)
			o = 8*log(1.0/m)/log(2);	/* convert to 'decibels' */
		else
			o = 8*log(-1.0/m)/log(2);	/* convert to 'decibels' */

		o = o / (ENV_STEP/4);

		n = (int)(2.0*o);
		if (n&1)						/* round to closest */
			n = (n>>1)+1;
		else
			n = n>>1;

		sin_tab[ i ] = n*2 + (m>=0.0? 0: 1 );
	}

	/* calculate d1l_tab table */
	for (i=0; i<16; i++)
	{
		m = (i!=15 ? i : i+16) * (4.0/ENV_STEP);   /* every 3 'dB' except for all bits = 1 = 45+48 'dB' */
		d1l_tab[i] = m;
	}
}

int indice( int finc, unsigned mul ) {
	if( mul==0 ) finc<<=1; else finc/=mul;
	while( finc<1299 ) finc<<=1; // octava 0
	while( finc>2593 && (finc%2==0) ) finc>>=1; // reduce octavas
	if( finc>2593 ) { return -1; }
//	cout << "Busco finc="<<finc<<endl;
	int k;
	for( k=0; k<768; k++ )
		if( phaseinc_rom[k]==finc ) break;
	if( phaseinc_rom[k]==finc ) return k; else return -1;
}
