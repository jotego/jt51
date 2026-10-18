#include <array>
#include <limits>
#include <utility>

namespace mixing {
constexpr unsigned first=128,training=8192,heldout=4096;
struct Record { std::vector<int> left,right; };
struct Cell { int count=0,lo=32767,hi=-32768; };
struct Interval { int lo=0,hi=0; bool valid=false; };
using Table=std::array<std::array<Cell,2>,32>;
using Hulls=std::array<std::array<Interval,2>,32>;
int branch(const std::vector<int>& r,unsigned n) {
    if(r[n]>r[n-1]&&r[n+1]>r[n])return 1;
    if(r[n]<r[n-1]&&r[n+1]<r[n])return 0;
    return -1;
}
Table table(const Record& r) {
    Table out{};
    for(unsigned n=first;n<first+training;++n) {
        int d=branch(r.right,n);if(d<0)continue;
        auto& c=out[(r.right[n]+32768)/2048][d];++c.count;c.lo=std::min(c.lo,r.left[n]);c.hi=std::max(c.hi,r.left[n]);
    }
    return out;
}
Hulls hulls(const Table& t,int domain_lo,int domain_hi) {
    Hulls out{};
    for(int b=0;b<32;++b)for(int d=0;d<2;++d) {
        int count=0,lo=32767,hi=-32768;bool valid=true;
        for(int a=std::max(0,b-1);a<=std::min(31,b+1);++a) {
            int lower=a*2048-32768,upper=lower+2047;
            if(upper<domain_lo||lower>domain_hi)continue;
            const auto& c=t[a][d];if(!c.count)valid=false;
            count+=c.count;lo=std::min(lo,c.lo);hi=std::max(hi,c.hi);
        }
        out[b][d]={lo-64,hi+64,valid&&count>=3};
    }
    return out;
}
struct Assessment {
    unsigned covered=0,outside=0,missing=0,ambiguous=0;
    std::array<unsigned,4> quadrants{};
    bool coverage(unsigned total)const {return covered*4>=total*3&&*std::min_element(quadrants.begin(),quadrants.end())>=3;}
    bool qualified(unsigned total)const {return coverage(total)&&!outside&&!missing;}
    void support(int right,int direction) {++covered;if(right)++quadrants[(right>0?2:0)+direction];}
};
Assessment assess(const Record& r,const Hulls& h,unsigned begin,unsigned end) {
    Assessment a;
    for(unsigned n=begin;n<end;++n) {
        int d=branch(r.right,n);if(d<0){++a.ambiguous;continue;}
        const auto& v=h[(r.right[n]+32768)/2048][d];if(!v.valid){++a.missing;continue;}
        a.support(r.right[n],d);if(r.left[n]<v.lo||r.left[n]>v.hi)++a.outside;
    }
    return a;
}
int clip(int v){return std::max(-32768,std::min(32767,v));}
std::vector<std::pair<int,int>> image(int lo,int hi,unsigned width) {
    std::vector<std::pair<int,int>> result;
    if(!width)result.push_back({clip(lo),clip(hi)});
    else {
        const int period=1<<width,half=period/2;
        if(hi-lo>=period-1)result.push_back({-32768,32767});
        else while(lo<=hi) {
            int q=(lo+half)/period;if(lo+half<0&&(lo+half)%period)--q;
            int end=std::min(hi,(q+1)*period-half-1);
            result.push_back({clip(lo-q*period),clip(end-q*period)});lo=end+1;
        }
    }
    for(auto& interval:result){interval.first=clip(interval.first-64);interval.second=clip(interval.second+64);}
    return result;
}
bool contains(const std::vector<std::pair<int,int>>& images,int value) {
    for(const auto& v:images)if(v.first<=value&&value<=v.second)return true;
    return false;
}
bool selected(unsigned n,unsigned offset) {return n==32||offset==0||offset==8||offset==16||offset==24||offset==1;}
unsigned pan(unsigned value,bool mirrored) {return mirrored?(value&~0xc0u)|((value&64)<<1)|((value&128)>>1):value;}
void init(Chip& c,unsigned n,bool mirrored) {
    const unsigned regs[][2]={{20,48},{15,0},{1,0},{16,0},{17,0},{18,0},{24,0},{25,0},{25,128},{27,0}};
    for(const auto& r:regs)c.write(r[0],r[1]);
    for(unsigned ch=0;ch<8;++ch)c.write(8,ch);
    for(unsigned r=32;r<256;++r) {
        unsigned v=0;
        if(r<40)v=64|((r-32==(n==5?1:0))?128:0)|7;
        else if(r<48)v=74;
        else if(r>=96&&r<128)v=selected(n,r-96)?32:127;
        else if(r>=128&&r<160)v=31;
        else if(r>=224)v=15;
        c.write(r,r<40?pan(v,mirrored):v);
    }
    for(unsigned ch=0;ch<8;++ch)c.write(8,ch|(n==32?120:ch==0?120:ch==1?8:0));
}
void expose(Chip& c,unsigned n,unsigned level,int channel,bool mirrored) {
    for(unsigned ch=0;ch<8;++ch)c.write(32+ch,pan(7|(ch==(n==5?1:0)?128:0),mirrored));
    for(unsigned s=0;s<32;++s)c.write(96+s,selected(n,s)?level:127);
    for(unsigned ch=0;ch<8;++ch)c.write(32+ch,pan(7|(ch==(n==5?1:0)?128:0)|((channel<0||ch==unsigned(channel))?64:0),mirrored));
}
Record capture(Chip& c,const std::string& label,bool mirrored) {
    c.step(179000);c.begin();c.frames(16384);c.end();
    check(label+" bipolar",bipolar(c.left)&&bipolar(c.right));Record result{c.left,c.right};if(mirrored)std::swap(result.left,result.right);return result;
}
void cleanup(Chip& c) {
    c.write(20,48);c.write(15,0);for(unsigned r=224;r<256;++r)c.write(r,15);for(unsigned ch=0;ch<8;++ch)c.write(8,ch);
    c.step(1790016);c.begin();c.frames(4096);c.end();check("mixing cleanup exact zero",peak(c.left,0,4096)==0&&peak(c.right,0,4096)==0);
}
void level_checks(const std::vector<Record>& components,const Record& aggregate,unsigned n,unsigned level,bool mirrored) {
    const std::string label=std::string(mirrored?"mixing mirrored N":"mixing N")+std::to_string(n)+" TL"+std::to_string(level);
    int domain_lo=*std::min_element(aggregate.right.begin(),aggregate.right.end()),domain_hi=*std::max_element(aggregate.right.begin(),aggregate.right.end());
    for(const auto& r:components){domain_lo=std::min(domain_lo,*std::min_element(r.right.begin(),r.right.end()));domain_hi=std::max(domain_hi,*std::max_element(r.right.begin(),r.right.end()));}
    std::vector<Hulls> bounds;
    for(unsigned i=0;i<components.size();++i) {
        const auto& r=components[i];bounds.push_back(hulls(table(r),domain_lo,domain_hi));
        auto a=assess(r,bounds.back(),first+training,first+training+heldout);
        check(label+" component heldout "+std::to_string(i),a.qualified(heldout),a.outside+a.missing);
        auto edges=std::minmax_element(r.left.begin()+first,r.left.begin()+first+training+heldout);
        check(label+" component uncensored "+std::to_string(i),*edges.first> -32768&&*edges.second<32704);
    }
    Assessment a;unsigned outside[4][2]={},excluded[4]={};const unsigned widths[4]={0,17,18,19};
    for(unsigned k=first;k<first+training+heldout;++k) {
        int d=branch(aggregate.right,k);if(d<0){++a.ambiguous;continue;}
        int lo=0,hi=0;bool valid=true;
        for(const auto& h:bounds){const auto& v=h[(aggregate.right[k]+32768)/2048][d];valid&=v.valid;lo+=v.lo;hi+=v.hi;}
        if(!valid){++a.missing;continue;}a.support(aggregate.right[k],d);
        for(unsigned j=0;j<4;++j)if(!contains(image(lo,hi,widths[j]),aggregate.left[k])){++excluded[j];if(aggregate.left[k])++outside[j][aggregate.left[k]>0?1:0];}
    }
    check(label+" aggregate coverage",a.coverage(training+heldout)&&!a.missing,a.covered);
    check(label+" measured wrap18 then clip16",excluded[2]==0,excluded[2]);
    if(n==32&&level==0)for(unsigned j:{0u,1u,3u})for(unsigned sign=0;sign<2;++sign)
        check(label+" excludes width"+std::to_string(widths[j])+" sign"+std::to_string(sign),outside[j][sign]>=3,outside[j][sign]);
}
void history(unsigned n,bool mirrored) {
    Chip c;cleanup(c);init(c,n,mirrored);auto initial=capture(c,"mixing initial low",mirrored);
    for(unsigned level:{32u,0u}) {
        std::vector<Record> components;
        for(unsigned ch=0;ch<(n+3)/4;++ch){expose(c,n,level,ch,mirrored);components.push_back(capture(c,"mixing component",mirrored));}
        expose(c,n,level,-1,mirrored);auto aggregate=capture(c,"mixing aggregate",mirrored);level_checks(components,aggregate,n,level,mirrored);
    }
    expose(c,n,32,-1,mirrored);auto returned=capture(c,"mixing returned low",mirrored);
    int lo=std::min(*std::min_element(initial.right.begin(),initial.right.end()),*std::min_element(returned.right.begin(),returned.right.end()));
    int hi=std::max(*std::max_element(initial.right.begin(),initial.right.end()),*std::max_element(returned.right.begin(),returned.right.end()));auto h=hulls(table(initial),lo,hi);
    check("mixing initial low heldout",assess(initial,h,first+training,first+training+heldout).qualified(heldout));
    check("mixing low history return",assess(returned,h,first,first+training+heldout).qualified(training+heldout));cleanup(c);
}
void tests() {
    for(unsigned width:{17u,18u,19u}) {int half=1<<(width-1);auto v=image(half-1,half,width);check("mixing signed boundary union "+std::to_string(width),contains(v,-32768)&&contains(v,32704)&&!contains(v,0));}
    for(unsigned n:{5u,32u})history(n,false);
}
void mirror_tests() {history(32,true);}
}
