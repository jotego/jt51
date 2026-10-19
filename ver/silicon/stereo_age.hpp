namespace stereo_age {
struct Fit {double phase,amplitude,relative;};
static const double pi=std::acos(-1.),native_w=2*pi*4124/1048576,w=native_w*8;
static double wrap(double x){return std::remainder(x,2*pi);}
static Fit fit(const std::vector<int>& v,size_t first,size_t end){
    if(end-first<1000)throw std::runtime_error("minimum 1000 samples");
    double m[3][4]={};
    for(size_t n=first;n<end;++n){double b[3]={std::sin(w*n),std::cos(w*n),1};for(unsigned r=0;r<3;++r){for(unsigned c=0;c<3;++c)m[r][c]+=b[r]*b[c];m[r][3]+=b[r]*v.at(n);}}
    for(unsigned k=0;k<3;++k){unsigned p=k;for(unsigned r=k+1;r<3;++r)if(std::abs(m[r][k])>std::abs(m[p][k]))p=r;for(unsigned c=k;c<4;++c)std::swap(m[k][c],m[p][c]);if(std::abs(m[k][k])<1e-10)throw std::runtime_error("singular fit");double d=m[k][k];for(unsigned c=k;c<4;++c)m[k][c]/=d;for(unsigned r=0;r<3;++r)if(r!=k){double f=m[r][k];for(unsigned c=k;c<4;++c)m[r][c]-=f*m[k][c];}}
    double amplitude=std::hypot(m[0][3],m[1][3]),ss=0;
    for(size_t n=first;n<end;++n){double e=v[n]-(m[0][3]*std::sin(w*n)+m[1][3]*std::cos(w*n)+m[2][3]);ss+=e*e;}
    return {std::atan2(m[1][3],m[0][3]),amplitude,amplitude?std::sqrt(ss/(end-first))/amplitude:INFINITY};
}
static bool qualified(const std::vector<int>& v,const Fit& f,size_t first=128,size_t end=1920){auto bounds=std::minmax_element(v.begin()+first,v.begin()+end);return *bounds.first<0&&*bounds.second>0&&f.amplitude>=32&&f.relative<=.003;}
static bool age_ok(double observed,double expected){return std::abs(wrap(observed-expected))<=.001;}
static std::vector<int> synthetic(double phase){std::vector<int> v;for(unsigned n=0;n<2048;++n)v.push_back(std::lround(8160*std::sin(w*n+phase)/8)*8);return v;}
static void analysis(){
    for(double origin:{0.,.7,2.3})for(int age:{-1,0,1}){auto r=synthetic(origin),l=synthetic(origin+age*native_w);auto rf=fit(r,128,1920),lf=fit(l,128,1920);check("stereo synthetic qualified",qualified(l,lf)&&qualified(r,rf));check("stereo synthetic signed native age",age_ok(wrap(lf.phase-rf.phase),age*native_w));}
    std::vector<int> quiet(2048,0);check("stereo synthetic quiet rejected",!qualified(quiet,fit(quiet,128,1920)));
    auto other=synthetic(.5*native_w),r=synthetic(0);double d=wrap(fit(other,128,1920).phase-fit(r,128,1920).phase);check("stereo synthetic other not rounded to zero or one",!age_ok(d,0)&&!age_ok(d,native_w));
    check("stereo synthetic changed return rejected",!age_ok(.01,0));
    bool rejected=false;try{fit(quiet,0,999);}catch(const std::runtime_error&){rejected=true;}check("stereo synthetic minimum window",rejected);
    for(unsigned n=0;n<2048;++n)other[n]=std::lround(8160*std::sin((w+.001)*n));check("stereo synthetic wrong carrier rejected",!qualified(other,fit(other,128,1920)));
}
static void initialize(Chip& c,unsigned slot){
    const unsigned globals[][2]={{20,48},{15,0},{1,0},{16,0},{17,0},{18,0},{24,0},{25,0},{25,128},{27,0}};
    for(auto& p:globals)c.write(p[0],p[1]);for(unsigned ch=0;ch<8;++ch)c.write(8,ch);
    unsigned ch=slot%8;
    for(unsigned reg=32;reg<256;++reg){unsigned value=0;if(reg<40)value=reg==32+ch?199:7;else if(reg<48)value=74;else if(reg>=96&&reg<128)value=reg==96+slot?0:127;else if(reg>=128&&reg<160)value=reg==128+slot?31:0;else if(reg>=224)value=15;c.write(reg,value);}
    const unsigned masks[]={8,32,16,64};c.write(8,masks[slot/8]|ch);c.step(1790000);
}
static void tests(){
    for(unsigned slot=0;slot<32;++slot){Chip c;initialize(c,slot);c.begin(8);c.frames(2048);c.end();auto l=fit(c.left,128,1920),r=fit(c.right,128,1920);std::string name="stereo slot"+std::to_string(slot);check(name+" LEFT fixed carrier bipolar",qualified(c.left,l),l.relative);check(name+" RIGHT fixed carrier bipolar",qualified(c.right,r),r.relative);
        double d=wrap(l.phase-r.phase),expected=slot>=15&&slot<=30?-native_w:0;check(name+" raw native age",age_ok(d,expected),d/native_w);
        auto le=fit(c.left,128,1128),re=fit(c.right,128,1128),ll=fit(c.left,920,1920),rl=fit(c.right,920,1920);double change=wrap(wrap(ll.phase-rl.phase)-wrap(le.phase-re.phase));check(name+" within record stability",qualified(c.left,le,128,1128)&&qualified(c.right,re,128,1128)&&qualified(c.left,ll,920,1920)&&qualified(c.right,rl,920,1920)&&age_ok(change,0),change);
        c.write(8,slot%8);c.step(1790000);c.begin(8);c.frames(2048);c.end();check(name+" whole stereo cleanup quiet",std::all_of(c.left.begin(),c.left.end(),[](int v){return v==0;})&&std::all_of(c.right.begin(),c.right.end(),[](int v){return v==0;}));
    }
}
}
