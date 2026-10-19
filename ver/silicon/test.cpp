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
#include <utility>
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
        c.write(0x10,128);c.write(0x11,0);c.begin();c.step((op*7+ch*11)%32);c.write(0x14,0x81);const size_t start=c.left.size();c.frames(start+4096);c.end();
        both_positive(c,"automatic CSM op"+std::to_string(op)+" ch"+std::to_string(ch));
        check("CSM initial load route op"+std::to_string(op)+" ch"+std::to_string(ch),
            peak(c.left,start,start+384)>0||peak(c.right,start,start+384)>0,
            std::max(peak(c.left,start,start+384),peak(c.right,start,start+384)));
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
    // Fixed public-output windows match the qualified physical observer:
    // 32768 serial words = 16384 samples per lane, with early [1024,5120),
    // late [11264,15360), sampled at stride4.
    // Physical RATE2/3 AR late RMS was 3.40..7.12 PCM; D1/D2 early-late
    // RMS deltas were 283.7..370.4 PCM across three captures per family.
    constexpr size_t e0=1024,e1=5120,l0=11264,l1=15360;
    auto ar_case=[&](unsigned kc,unsigned ks,unsigned raw_rate,bool progress) {
        Chip c;c.voice(raw_rate,ks,0,0,0,15,kc);c.begin(4);c.write(8,0x40);c.frames(16384);c.end();
        for(auto* v:{&c.left,&c.right}) {
            const double late=rms(*v,l0,l1);
            check("AR KC"+std::to_string(kc)+" KS"+std::to_string(ks)+" raw"+std::to_string(raw_rate)+" late RMS",
                progress ? late>2.0&&late<32.0 : late==0.0,late);
        }
    };
    ar_case(0x00,0,0,false); // effective RATE0, exact-zero control
    ar_case(0x04,3,0,false); // effective RATE0, matched KC/KS control
    ar_case(0x00,0,1,true);  // effective RATE2
    ar_case(0x04,3,1,true);  // effective RATE3
    ar_case(0x00,0,2,true);  // effective RATE4 active control
    ar_case(0x04,3,2,true);  // effective RATE5 active control

    for(unsigned stage=1;stage<=2;++stage) {
        for(auto setup:{std::pair<unsigned,unsigned>{0x00,0}, {0x04,3}}) {
            const auto [kc,ks]=setup;
            auto decay_case=[&](unsigned raw_rate,const std::string& label,bool progress) {
                Chip c;c.voice(31,ks,stage==2?31:0,0,stage==1?15:0,15,kc);c.write(8,0x40);c.step(4096);
                c.begin(4);c.write((stage==1?0xa0:0xc0)+24,raw_rate);c.frames(16384);c.end();
                both_positive(c,"D"+std::to_string(stage)+" "+label+" positive carrier");
                for(auto* v:{&c.left,&c.right}) {
                    const double first=rms(*v,e0,e1),last=rms(*v,l0,l1),delta=first-last;
                    check("D"+std::to_string(stage)+" "+label+" KC"+std::to_string(kc)+" KS"+std::to_string(ks)+" early-late RMS",
                        progress ? (raw_rate==1 ? delta>200.0&&delta<450.0 : delta>200.0&&delta<650.0) : std::abs(delta)<32.0,delta);
                }
            };
            decay_case(0,"raw RATE0 hold",false); // effective RATE0 matched hold
            decay_case(1,"raw RATE1",true);        // effective RATE2 or RATE3
            decay_case(2,"raw RATE2 positive",true); // effective RATE4 or RATE5 control
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
static void diagnostic_window(const std::string& label,const std::vector<int>& v,size_t begin,size_t end) {
    const int p=peak(v,begin,end);
    std::cout<<"DIAG "<<label<<" rms="<<rms(v,begin,end)<<" peak="<<p<<"\n";
}
static void held_ar_voice(Chip& c,unsigned raw_ar,bool target_key=false,bool target_mute=false) {
    c.write(1,0);c.write(0x14,0x30);c.write(0x10,0);c.write(0x11,0);c.write(0x12,0);
    c.write(0x0f,0);c.write(0x18,0);c.write(0x19,0);c.write(0x19,0x80);c.write(0x1b,0);
    for(unsigned ch=0;ch<8;++ch) {
        c.write(0x20+ch,ch==0?0x47:(ch==1?0x87:7));
        c.write(0x28+ch,0x4a);c.write(0x30+ch,0);c.write(0x38+ch,0);
    }
    for(unsigned s=0;s<32;++s) {
        const bool target=s==24,marker=s==25;
        c.write(0x40+s,(target||marker)?8:1);
        c.write(0x60+s,(marker||(target&&!target_mute))?0:127);
        c.write(0x80+s,target?raw_ar:(marker?31:0));
        c.write(0xa0+s,0);c.write(0xc0+s,0);c.write(0xe0+s,15);
    }
    c.write(8,0x41 | (target_key?0x40:0));
}
static void held_ar_probe_tests() {
    constexpr size_t e0=128,e1=640,l0=3584,l1=4096;
    auto held_case=[&](unsigned raw_ar) {
        Chip c;held_ar_voice(c,10);
        c.begin();c.write(8,0x40);c.write(0x29,0x52);c.frames(4093);
        const size_t before=c.left.size();c.write(0x98,raw_ar);c.write(0x29,0x4a);
        const size_t event=c.left.size();c.frames(4096);c.end();
        const double pre=rms(c.left,before-512,before),early=rms(c.left,event+e0,event+e1),late=rms(c.left,event+l0,event+l1);
        check("held AR10->"+std::to_string(raw_ar)+" prewrite finite",pre>50.0&&pre<160.0,pre);
        check("held AR10->"+std::to_string(raw_ar)+" early plateau positive",early>80.0&&early<200.0,early);
        check("held AR10->"+std::to_string(raw_ar)+" late plateau positive",late>80.0&&late<200.0,late);
        check("held AR10->"+std::to_string(raw_ar)+" plateau stable",std::abs(early-late)<16.0,early-late);
        check("held AR10->"+std::to_string(raw_ar)+" marker right positive",rms(c.right,event+l0,event+l1)>5000.0,rms(c.right,event+l0,event+l1));
    };
    held_case(30);held_case(31);

    auto control=[&](const std::string& label) {
        Chip c;held_ar_voice(c,10,false,label=="marker-only");
        c.begin();if(label!="marker-only")c.write(8,0x40);c.write(0x29,0x52);c.frames(4093);
        const size_t before=c.left.size();if(label=="noop10")c.write(0x98,10);else if(label=="active12")c.write(0x98,12);
        c.write(0x29,0x4a);const size_t event=c.left.size();c.frames(4096);c.end();
        const double early=rms(c.left,event+e0,event+e1),late=rms(c.left,event+l0,event+l1);
        if(label=="marker-only") {
            check("held AR marker-only target quiet",peak(c.left,event+l0,event+l1)<=4,peak(c.left,event+l0,event+l1));
        } else if(label=="noop10") {
            check("held AR10 no-op keeps progressing",late>500.0,late);
        } else {
            check("held AR10->12 early progresses",early>150.0,early);
            check("held AR10->12 late progresses",late>2500.0,late);
        }
        check("held AR positive marker right",rms(c.right,event+l0,event+l1)>5000.0,rms(c.right,event+l0,event+l1));
    };
    control("marker-only");control("noop10");control("active12");

    Chip positive;held_ar_voice(positive,31);
    positive.begin();positive.write(8,0x40);positive.write(0x29,0x52);positive.frames(307);positive.write(0x29,0x4a);
    const size_t newkey_event=positive.left.size();positive.frames(4096);positive.end();
    check("held AR fresh-key AR31 early positive",rms(positive.left,newkey_event+128,newkey_event+640)>5000.0&&rms(positive.left,newkey_event+128,newkey_event+640)<6500.0,rms(positive.left,newkey_event+128,newkey_event+640));
    check("held AR fresh-key AR31 late positive",rms(positive.left,newkey_event+3584,newkey_event+4096)>5000.0&&rms(positive.left,newkey_event+3584,newkey_event+4096)<6500.0,rms(positive.left,newkey_event+3584,newkey_event+4096));

    Chip fresh30;held_ar_voice(fresh30,30);fresh30.begin();fresh30.write(8,0x40);const size_t fresh30_event=fresh30.left.size();fresh30.frames(4096);fresh30.end();
    check("held AR fresh effective-max AR30 early positive",rms(fresh30.left,fresh30_event+128,fresh30_event+640)>5000&&rms(fresh30.left,fresh30_event+128,fresh30_event+640)<6500,rms(fresh30.left,fresh30_event+128,fresh30_event+640));
    check("held AR fresh effective-max AR30 late positive",rms(fresh30.left,fresh30_event+3584,fresh30_event+4096)>5000&&rms(fresh30.left,fresh30_event+3584,fresh30_event+4096)<6500,rms(fresh30.left,fresh30_event+3584,fresh30_event+4096));

    for(auto setup:{std::pair<unsigned,unsigned>{0x00,0},{0x04,3}}) {
        const auto [kc,ks]=setup;
        Chip fast;held_ar_voice(fast,10);fast.write(0x28,kc);fast.write(0x80+24,(ks<<6)|10);
        fast.begin();fast.write(8,0x40);fast.write(0x29,0x52);fast.frames(4093);
        fast.write(0x98,(ks<<6)|30);fast.write(0x29,0x4a);const size_t event=fast.left.size();fast.frames(4096);fast.end();
        const double early=rms(fast.left,event+128,event+640),late=rms(fast.left,event+3584,event+4096);
        check("unmeasured effective-rate"+std::to_string(ks==0?60:61)+" held AR30 advances",early>32.0&&late>5000.0,early);
    }

    const unsigned opmask[4]={8,32,16,64};
    for(unsigned op=0;op<4;++op)for(unsigned ch=0;ch<8;++ch) {
        Chip route;route.voice(30,0,0,0,0,15,0x4a,ch,op*8);
        for(unsigned other=0;other<8;++other) if(other!=ch)
            route.write(0x28+other,0x08+((ch*3+other*5)&0x7f));
        for(unsigned slot=0;slot<32;++slot) if(slot!=op*8+ch)
            route.write(0x80+slot,(((slot+op+ch)&3)<<6));
        route.begin();route.write(8,opmask[op]|ch);const size_t event=route.left.size();route.frames(4096);route.end();
        check("effective-max AR30 new-key route op"+std::to_string(op)+" ch"+std::to_string(ch)+" early",
            rms(route.left,event+128,event+640)>5000&&rms(route.left,event+128,event+640)<6500,rms(route.left,event+128,event+640));
        check("effective-max AR30 new-key route op"+std::to_string(op)+" ch"+std::to_string(ch)+" late",
            rms(route.left,event+3584,event+4096)>5000&&rms(route.left,event+3584,event+4096)<6500,rms(route.left,event+3584,event+4096));
    }

    positive.write(8,0x00);positive.write(8,0x01);positive.write(0x14,0x30);positive.step(1790016);positive.begin();positive.frames(2048);positive.end();
    check("held AR cleanup quiet",peak(positive.left,0,2048)==0&&peak(positive.right,0,2048)==0,peak(positive.left,0,2048));
}
static void csm_start_probe_tests() {
    auto prepare=[](Chip& c) {
        held_ar_voice(c,31);
        c.write(0x10,0);c.write(0x11,0);c.write(0x14,0x30);
    };
    auto capture_start=[](Chip& c,unsigned ctl) {
        c.begin();c.write(0x29,0x52);const size_t tag=c.left.size();
        c.write(0x14,ctl);c.frames(tag+1500-c.left.size());c.end();return tag;
    };
    Chip startup;prepare(startup);size_t tag=capture_start(startup,0x81);
    const double burst=rms(startup.left,tag+128,tag+384),over=rms(startup.left,tag+1088,tag+1344);
    check("CSM initial load startup window",burst>=24&&burst<=55,burst);
    check("CSM first overflow window",over>=190&&over<=320,over);
    check("CSM startup right marker",rms(startup.right,tag+1088,tag+1344)>5000,rms(startup.right,tag+1088,tag+1344));
    startup.write(8,0x00);startup.write(8,0x01);startup.write(0x14,0x30);startup.step(1790016);startup.begin();startup.frames(256);startup.end();
    check("CSM startup cleanup quiet",peak(startup.left,0,256)==0&&peak(startup.right,0,256)==0,peak(startup.left,0,256));
    startup.write(8,0x41);startup.write(0x29,0x4a);startup.write(0x10,0);startup.write(0x11,0);startup.write(0x14,0x30);
    const size_t restart_tag=capture_start(startup,0x81);
    check("CSM load restart burst",rms(startup.left,restart_tag+128,restart_tag+384)>=24&&rms(startup.left,restart_tag+128,restart_tag+384)<=55,rms(startup.left,restart_tag+128,restart_tag+384));

    Chip timer_only;prepare(timer_only);size_t timer_tag=capture_start(timer_only,0x01);
    check("CSM timer-only startup quiet",rms(timer_only.left,timer_tag+128,timer_tag+384)<=4&&peak(timer_only.left,timer_tag+128,timer_tag+384)<=4,peak(timer_only.left,timer_tag+128,timer_tag+384));
    Chip stopped;prepare(stopped);size_t stop_tag=capture_start(stopped,0x80);
    check("CSM stopped timer startup quiet",rms(stopped.left,stop_tag+128,stop_tag+384)<=4&&peak(stopped.left,stop_tag+128,stop_tag+384)<=4,peak(stopped.left,stop_tag+128,stop_tag+384));
    Chip delayed;prepare(delayed);delayed.write(0x14,0x01);delayed.step(512*64);size_t delayed_tag=capture_start(delayed,0x81);
    check("CSM delayed enable before overflow quiet",rms(delayed.left,delayed_tag+128,delayed_tag+384)<=4&&peak(delayed.left,delayed_tag+128,delayed_tag+384)<=4,peak(delayed.left,delayed_tag+128,delayed_tag+384));
    check("CSM delayed enable after overflow positive",rms(delayed.left,delayed_tag+482,delayed_tag+908)>800&&rms(delayed.left,delayed_tag+482,delayed_tag+908)<1600&&peak(delayed.left,delayed_tag+482,delayed_tag+908)>500,rms(delayed.left,delayed_tag+482,delayed_tag+908));
}
static void release_tests() {for(unsigned kc:{0,4,8,16}){Chip c;c.voice(31,kc?3:0,0,0,0,0,kc,6);c.begin(32);c.step(8192*75/2);c.write(8,0x7e);c.step(10408*75/2);c.write(8,6);c.frames(16384-c.left.size());c.end();both_positive(c,"low RR0");for(auto* v:{&c.left,&c.right}){double early=rms(*v,1024,2048),late=rms(*v,15360,16384),ratio=late/early;check("low RR0 KC"+std::to_string(kc)+" finite attenuation",early>1000&&ratio<.8&&ratio>.2,ratio);}}}
#include "mixing.hpp"
#include "nominal_pitch.hpp"
#include "stereo_age.hpp"
int main(int argc,char**argv){try{std::cout.setf(std::ios::unitbuf);Verilated::commandArgs(argc,argv);std::string which=argc>1?argv[1]:"all";if(which=="all"||which=="analysis")analysis_tests();if(which=="all"||which=="timer")timer_tests();if(which=="all"||which=="attack")attack_tests();if(which=="all"||which=="zero")zero_tests();if(which=="all"||which=="d1l")d1l_tests();if(which=="all"||which=="granularity")granularity_tests();if(which=="all"||which=="csm")csm_tests();if(which=="all"||which=="routes")route_tests();if(which=="all"||which=="noise")noise_tests();if(which=="all"||which=="slowhold")slowhold_tests();if(which=="all"||which=="csm_states")csm_state_tests();if(which=="all"||which=="held_ar_probe")held_ar_probe_tests();if(which=="all"||which=="csm_start_probe")csm_start_probe_tests();if(which=="all"||which=="release")release_tests();if(which=="all"||which=="mixing")mixing::tests();if(which=="all"||which=="mixing_mirror")mixing::mirror_tests();if(which=="all"||which=="nominal_pitch")nominal_pitch::tests();if(which=="all"||which=="stereo_age_analysis")stereo_age::analysis();if(which=="all"||which=="stereo_age")stereo_age::tests();if(checks==0)throw std::runtime_error("unknown test group");std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;}catch(std::exception&e){std::cerr<<e.what()<<"\n";return 2;}}
