#include "Vjt51.h"
#include "verilated.h"
#ifdef TRACE
#include "verilated_fst_c.h"
#endif
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Only public pins are observed. No hierarchy, reference emulator or EG taps.
class Chip {
    VerilatedContext context;
    Vjt51 top{&context};
#ifdef TRACE
    VerilatedFstC trace;
#endif
    uint64_t clocks=0;
    unsigned decimation=1, native_phase=0;
    bool recording=false;
public:
    std::vector<int> left,right;
    Chip() {
        top.clk=0; top.rst=1; top.cen=1; top.cen_p1=0;
        top.cs_n=0; top.wr_n=1; top.a0=0; top.din=0;
#ifdef TRACE
        context.traceEverOn(true); top.trace(&trace,99); trace.open("test.fst");
#endif
        step(256); top.rst=0; step(256);
    }
    ~Chip() { top.final();
#ifdef TRACE
        trace.close();
#endif
    }
    void step(uint64_t count) {
        while(count--) {
            top.clk=0; top.cen_p1=clocks&1; top.eval();
            const bool sample=top.sample;
            context.timeInc(139665);
#ifdef TRACE
            trace.dump(context.time());
#endif
            top.clk=1; top.eval(); context.timeInc(139665); ++clocks;
#ifdef TRACE
            trace.dump(context.time());
#endif
            if(recording && sample) {
                if(native_phase==0) {left.push_back(int16_t(top.left));right.push_back(int16_t(top.right));}
                native_phase=(native_phase+1)%decimation;
            }
        }
    }
    unsigned status() { return top.dout&3; }
    void wait_busy() {unsigned n=0; while(top.dout&128) {if(++n>4096) throw std::runtime_error("busy timeout");step(1);}}
    void write(unsigned reg,unsigned val) {
        wait_busy(); top.a0=0;top.din=reg;top.wr_n=0;step(2);
        top.wr_n=1;step(10);top.a0=1;top.din=val;top.wr_n=0;step(2);
        top.wr_n=1;step(2);wait_busy();
    }
    void voice(unsigned ar=31,unsigned ks=0,unsigned d1=0,unsigned d2=0,unsigned dl=0,unsigned rr=15,unsigned kc=0x4a,unsigned ch=0,unsigned opoffset=24) {
        write(1,0);write(0x14,0x30);write(0xf,0);write(0x18,0);write(0x19,0);write(0x19,0x80);write(0x1b,0);
        for(unsigned c=0;c<8;++c) {write(8,c);write(0x20+c,c==ch?0xc7:7);write(0x28+c,kc);write(0x30+c,0);write(0x38+c,0);}
        for(unsigned s=0;s<32;++s) {
            const bool target=s==opoffset+ch;
            write(0x40+s,target?8:1);write(0x60+s,target?0:127);
            write(0x80+s,target?(ks<<6)|ar:0);write(0xa0+s,target?d1:0);
            write(0xc0+s,target?d2:0);write(0xe0+s,target?(dl<<4)|rr:15);
        }
        step(4096);
    }
    void begin(unsigned stride=1) { left.clear();right.clear();decimation=stride;native_phase=0;recording=true; }
    void frames(unsigned n) { const auto goal=left.size()+n;while(left.size()<goal) step(1); }
    void end() {recording=false;}
};
static double rms(const std::vector<int>& v,size_t a,size_t b) {double x=0;for(size_t i=a;i<b;++i)x+=double(v.at(i))*v.at(i);return std::sqrt(x/(b-a));}
static int peak(const std::vector<int>& v,size_t a,size_t b) {int p=0;for(size_t i=a;i<b;++i)p=std::max(p,std::abs(v.at(i)));return p;}
static bool bipolar(const std::vector<int>& v) {return *std::min_element(v.begin(),v.end())<0 && *std::max_element(v.begin(),v.end())>0;}
static std::vector<double> projected(const std::vector<int>& v,unsigned window=32,unsigned hop=4) {
    // Fixed carrier public phase increment measured independently:65984/2^20.
    const double w=2*std::acos(-1.0)*65984/1048576;
    std::vector<double> out;
    for(size_t a=0;a+window<=v.size();a+=hop) {
        double m[3][4]={};
        for(unsigned j=0;j<window;++j) {double basis[3]={std::sin(w*(a+j)),std::cos(w*(a+j)),1};for(unsigned r=0;r<3;++r){for(unsigned c=0;c<3;++c)m[r][c]+=basis[r]*basis[c];m[r][3]+=basis[r]*v[a+j];}}
        for(unsigned r=0;r<3;++r){const double d=m[r][r];for(unsigned c=r;c<4;++c)m[r][c]/=d;for(unsigned k=0;k<3;++k)if(k!=r){const double f=m[k][r];for(unsigned c=r;c<4;++c)m[k][c]-=f*m[r][c];}}
        out.push_back(std::hypot(m[0][3],m[1][3]));
    }
    return out;
}
static double median(std::vector<double> x) {if(x.empty())throw std::runtime_error("empty median");std::sort(x.begin(),x.end());return x[x.size()/2];}
static std::vector<double> plateau_levels(const std::vector<double>& a) {
    std::vector<double> levels;
    for(size_t i=2;i+2<a.size();++i) {
        if(a[i]<8169.5*.38||a[i]>8169.5*.85)continue;
        auto bounds=std::minmax_element(a.begin()+i-2,a.begin()+i+3);
        if(20*std::log10(*bounds.second / *bounds.first)>.015)continue;
        double level=median(std::vector<double>(a.begin()+i-2,a.begin()+i+3));
        if(levels.empty()||std::abs(20*std::log10(levels.back()/level))>.03)levels.push_back(level);
    }
    return levels;
}
static double attenuation_step(const std::vector<double>& levels) {
    std::vector<double> steps;
    for(size_t i=1;i<levels.size();++i)steps.push_back(20*std::log10(levels[i-1]/levels[i]));
    return steps.empty()?0:median(steps);
}
static unsigned failures=0,checks=0;
static void check(const std::string& name,bool pass,double observed=0) {++checks;if(!pass)++failures;std::cout<<(pass?"PASS ":"FAIL ")<<name<<" observed="<<observed<<"\n";}
static void both_positive(Chip& c,const std::string& n) {check(n+" LEFT bipolar",bipolar(c.left));check(n+" RIGHT bipolar",bipolar(c.right));}
static void analysis_tests() {
    const double pi=std::acos(-1.0),w=2*pi*65984/1048576;
    for(double step:{.09375,.1875})for(double phase:{0.,.5,1.7}) {
        std::vector<int> signal;
        for(unsigned i=0;i<16384;++i)signal.push_back(std::lround(8169.5*std::pow(10.,-step*(i/128)/20)*std::sin(w*i+phase)+17));
        double observed=attenuation_step(plateau_levels(projected(signal)));
        check("synthetic staircase step",std::abs(observed-step)<.003,observed);
    }
    std::vector<int> quiet(4096,0);auto a=projected(quiet);check("synthetic zero projection",*std::max_element(a.begin(),a.end())==0);
}
static void timer_tests() {
    for(unsigned control=0;control<16;++control) {Chip c;c.write(0x10,255);c.write(0x11,3);c.write(0x12,255);c.write(0x14,0x30);c.write(0x14,control);c.step(8192);
        unsigned expected=((control&1)&&(control&4)?1:0)|((control&2)&&(control&8)?2:0);
        check("timer control "+std::to_string(control),c.status()==expected,c.status());}
    for(unsigned t=0;t<2;++t){Chip c;unsigned load=1<<t,enable=4<<t,clear=16<<t;c.write(0x10,255);c.write(0x11,3);c.write(0x12,255);
        c.write(0x14,load);c.step(8192);check("disabled overflow",c.status()==0,c.status());
        c.write(0x14,load|enable);c.step(8192);check("enabled overflow",c.status()==load,c.status());
        c.write(0x14,load);c.step(8192);check("disable preserves flag",c.status()==load,c.status());
        c.write(0x14,load|clear);c.step(8192);check("clear disabled",c.status()==0,c.status());
        c.write(0x14,load|enable);c.step(8192);c.write(0x14,0);c.step(8192);check("stop preserves flag",c.status()==load,c.status());c.write(0x14,clear);check("clear stopped",c.status()==0,c.status());}
}
static void attack_tests() {
    const unsigned settings[][2]={{14,0},{13,1},{11,2},{6,3}};
    for(auto& setting:settings){Chip c;c.voice(setting[0],setting[1]);c.begin();c.write(8,0x40);c.frames(16384);c.end();both_positive(c,"attack");
        auto a=projected(c.left);double tail=median(std::vector<double>(a.end()-512,a.end()));
        size_t lo=a.size(),hi=a.size();for(size_t i=0;i<a.size();++i){if(lo==a.size() && a[i]>=tail*.1)lo=i;if(lo<a.size()&&a[i]>=tail*.9){hi=i;break;}}
        double ms=(hi-lo)*4/55.9375;check("noninstant attack AR"+std::to_string(setting[0])+" KS"+std::to_string(setting[1]),hi<a.size()&&ms>35&&ms<55,ms);
    }
}
static void zero_tests() {
    for(unsigned ks=0;ks<4;++ks){Chip c;c.voice(0,ks);c.begin();c.write(8,0x40);c.frames(16384);c.end();check("AR0 inhibited KS"+std::to_string(ks),peak(c.left,0,c.left.size())==0&&peak(c.right,0,c.right.size())==0,peak(c.left,0,c.left.size()));}
    for(unsigned d2stage=0;d2stage<2;++d2stage)for(unsigned ks=0;ks<4;++ks){Chip c;c.voice(31,ks,d2stage?31:0,0,d2stage?4:15);c.write(8,0x40);c.step(4096);c.begin();c.frames(16384);c.end();both_positive(c,"zero hold");auto a=projected(c.left);double tail=median(std::vector<double>(a.end()-512,a.end()));double target=d2stage?2042.3:8169.5;check("raw D"+std::to_string(d2stage+1)+"R0 hold KS"+std::to_string(ks),std::abs(tail-target)<2,tail);}
    Chip positive;positive.voice(1,3);positive.begin();positive.write(8,0x40);positive.frames(16384);positive.end();both_positive(positive,"AR1 positive");
}
static void d1l_tests() {for(unsigned dl=14;dl<=15;++dl){Chip c;c.voice(31,0,31,0,dl);c.begin();c.write(8,0x40);c.frames(16384);c.end();both_positive(c,"D1L transient");for(auto* v:{&c.left,&c.right}){int p=peak(*v,12288,16384);check("D1L"+std::to_string(dl)+" tail",dl==15?p==0:p>=60&&p<=65,p);}}}
static void granularity_tests() {for(unsigned stage=1;stage<=2;++stage){Chip c;c.voice(31,0,0,0,stage==1?15:0);c.write(8,0x40);c.step(4096);c.begin();c.write((stage==1?0xa0:0xc0)+24,12);c.frames(16384);c.end();both_positive(c,"granularity");auto a=projected(c.left);auto levels=plateau_levels(a);double step=attenuation_step(levels);check("D"+std::to_string(stage)+" rate12 attenuation step dB",step>.089&&step<.099,step);check("D"+std::to_string(stage)+" resolved plateaus",levels.size()>=70&&levels.size()<=80,levels.size());}}
static void csm_tests() {for(unsigned period:{256,512,1024}){Chip c;c.voice();unsigned na=1024-period;c.write(0x10,na>>2);c.write(0x11,na&3);c.begin();c.write(0x14,0x81);c.frames(16384);c.end();both_positive(c,"automatic CSM");std::vector<double> power;for(size_t i=0;i+32<c.left.size();++i){double e=0;for(unsigned j=0;j<32;++j)e+=double(c.left[i+j])*c.left[i+j];power.push_back(e/32);}auto sorted=power;std::sort(sorted.begin(),sorted.end());double lo=sorted[sorted.size()/10],hi=sorted[sorted.size()*99/100],mid=(lo+hi)/2;std::vector<double> rises;for(size_t i=1;i<power.size();++i)if(power[i-1]<mid&&power[i]>=mid&&(rises.empty()||i-rises.back()>64))rises.push_back(i);std::vector<double> gaps;for(size_t i=1;i<rises.size();++i)gaps.push_back(rises[i]-rises[i-1]);double cadence=gaps.empty()?0:median(gaps);check("CSM finite cadence "+std::to_string(period),rises.size()>=5&&std::abs(cadence-period)<=64,cadence);
        c.write(0x14,0x30);c.step(3580000/2);c.begin();c.frames(4096);c.end();check("CSM disabled quiet tail",peak(c.left,0,4096)==0&&peak(c.right,0,4096)==0,peak(c.left,0,4096));}}
static void route_tests() {
    const unsigned masks[4]={8,32,16,64};
    for(unsigned op=0;op<4;++op)for(unsigned ch=0;ch<8;++ch) {
        Chip c;c.voice(31,0,0,0,0,15,0x4a,ch,op*8);
        c.write(0x10,128);c.write(0x11,0);c.begin();c.write(0x14,0x81);c.frames(4096);c.end();
        both_positive(c,"automatic CSM op"+std::to_string(op)+" ch"+std::to_string(ch));
    }
    for(unsigned op=0;op<4;++op) {
        Chip c;c.voice(31,0,0,0,0,15,0x4a,0,op*8);c.write(8,masks[op]);c.begin();c.frames(4096);c.end();
        both_positive(c,"manual route positive");double before=rms(c.left,0,4096);
        c.write(0x10,128);c.write(0x11,0);c.begin();c.write(0x14,0x81);c.frames(4096);c.end();
        double ratio=rms(c.left,0,4096)/before;
        check("manually held carrier with CSM",ratio>.98&&ratio<1.02,ratio);
    }
}
static void noise_tests() {
    for(unsigned history=0;history<3;++history) {
        Chip c;c.voice(31,0,0,0,0,15,0x4a,7);c.write(0x5f,0);c.write(0xf,0x8f);
        if(history) {
            c.write(0x7f,history==1?0:127);c.write(8,0x47);c.step(8192*32);
            c.begin();c.frames(16384);c.end();both_positive(c,"noise keyed positive");
            if(history==2)for(auto* v:{&c.left,&c.right})check("keyed TL127 noise -9/+8",*std::min_element(v->begin(),v->end())==-9&&*std::max_element(v->begin(),v->end())==8,*std::min_element(v->begin(),v->end()));
            c.write(8,7);
        }
        c.step(1790016);c.begin();c.frames(16384);c.end();
        for(auto* v:{&c.left,&c.right})check(history?"released maximum-off noise exact zero":"never-keyed maximum-off noise exact zero",peak(*v,0,v->size())==0,peak(*v,0,v->size()));
    }
}
static void slowhold_tests() {
    // Yamaha RATE0..3 infinite for AR/D1R/D2R. This is a documentary control,
    // not a claim that these particular KC programs were captured physically.
    for(unsigned kc:{0,4}) {
        unsigned ks=kc?3:0;
        Chip silent;silent.voice(1,ks,0,0,0,15,kc);silent.begin();silent.write(8,0x40);silent.frames(65536);silent.end();
        check("documented low nonrelease attack hold",peak(silent.left,0,silent.left.size())==0&&peak(silent.right,0,silent.right.size())==0,peak(silent.left,0,silent.left.size()));
        for(unsigned stage=1;stage<=2;++stage) {
            Chip c;c.voice(31,ks,0,0,stage==1?15:0,15,kc);c.write(8,0x40);c.step(4096);c.begin();c.write((stage==1?0xa0:0xc0)+24,1);c.frames(65536);c.end();
            both_positive(c,"low nonrelease hold positive");
            for(auto* v:{&c.left,&c.right}){int first=peak(*v,0,8192),last=peak(*v,v->size()-8192,v->size());
                check("documented D"+std::to_string(stage)+"R1 lowrate held peak",first>8000&&std::abs(first-last)<=16,last-first);}
        }
    }
}
static void csm_state_tests() {
    // Measured finite state controls, no exact keypulse or transition-time oracle.
    for(unsigned control:{1,0x80,0x82}) {
        Chip c;c.voice();c.write(0x10,128);c.write(0x11,0);c.write(0x12,255);c.begin();c.write(0x14,control);c.frames(8192);c.end();
        check("CSM inactive state "+std::to_string(control),peak(c.left,0,c.left.size())==0&&peak(c.right,0,c.right.size())==0,peak(c.left,0,c.left.size()));
    }
    for(unsigned ar:{30,31})for(unsigned stop:{1,0x80}) {
        Chip c;c.voice(ar);c.write(0x10,128);c.write(0x11,0);c.write(0x14,1);c.step(4096);
        c.begin();c.write(0x14,0x85);c.frames(8192);c.end();both_positive(c,"CSM enable running A AR"+std::to_string(ar));
        check("CSM enabled flag creation",c.status()==1,c.status());
        c.write(0x14,stop);c.step(1790016);c.begin();c.frames(4096);c.end();
        check(stop==1?"CSM disabled while A running quiet":"A stopped while CSM set quiet",peak(c.left,0,c.left.size())==0&&peak(c.right,0,c.right.size())==0,peak(c.left,0,c.left.size()));
    }
}
static void release_tests() {for(unsigned kc:{0,4,8,16}){Chip c;c.voice(31,kc?3:0,0,0,0,0,kc,6);c.begin(32);c.step(8192*75/2);c.write(8,0x7e);c.step(10408*75/2);c.write(8,6);c.frames(16384-c.left.size());c.end();both_positive(c,"low RR0");for(auto* v:{&c.left,&c.right}){double early=rms(*v,1024,2048),late=rms(*v,15360,16384),ratio=late/early;check("low RR0 KC"+std::to_string(kc)+" finite attenuation",early>1000&&ratio<.8&&ratio>.2,ratio);}}}
#include "mixing.hpp"
int main(int argc,char**argv){try{std::cout.setf(std::ios::unitbuf);Verilated::commandArgs(argc,argv);std::string which=argc>1?argv[1]:"all";if(which=="all"||which=="analysis")analysis_tests();if(which=="all"||which=="timer")timer_tests();if(which=="all"||which=="attack")attack_tests();if(which=="all"||which=="zero")zero_tests();if(which=="all"||which=="d1l")d1l_tests();if(which=="all"||which=="granularity")granularity_tests();if(which=="all"||which=="csm")csm_tests();if(which=="all"||which=="routes")route_tests();if(which=="all"||which=="noise")noise_tests();if(which=="all"||which=="slowhold")slowhold_tests();if(which=="all"||which=="csm_states")csm_state_tests();if(which=="all"||which=="release")release_tests();if(which=="all"||which=="mixing")mixing::tests();if(which=="all"||which=="mixing_mirror")mixing::mirror_tests();if(checks==0)throw std::runtime_error("unknown test group");std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;}catch(std::exception&e){std::cerr<<e.what()<<"\n";return 2;}}
