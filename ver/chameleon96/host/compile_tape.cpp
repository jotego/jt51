#include "cpu_schedule.hpp"
#include <openssl/sha.h>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifndef TAPE_SOURCE_SHA
#define TAPE_SOURCE_SHA "unrecorded"
#endif
#ifndef SCHEDULE_SOURCE_SHA
#define SCHEDULE_SOURCE_SHA "unrecorded"
#endif
namespace {
constexpr uint64_t MAX_TIME=(uint64_t(1)<<48)-1;
struct Program { std::string path,sha; std::vector<uint8_t> bytes; cpu_timing::Schedule schedule; unsigned low; };
struct Record { uint64_t time; unsigned op,data,pins; };
std::string quote(const std::string& s) {
    std::ostringstream o;o<<'"';
    for(unsigned char c:s) {
        if(c=='"'||c=='\\')o<<'\\'<<char(c);
        else if(c<32)o<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<unsigned(c)<<std::dec;
        else o<<char(c);
    }
    return o.str()+'"';
}
std::string digest(const std::vector<uint8_t>& b) {
    unsigned char d[SHA256_DIGEST_LENGTH];SHA256(b.data(),b.size(),d);
    std::ostringstream o;for(auto x:d)o<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(x);return o.str();
}
uint64_t number(const std::string& s) {
    if(s.empty()||s.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("expected unsigned decimal integer");
    return std::stoull(s);
}
Program load(const std::string& path,unsigned low) {
    Program p;p.path=std::filesystem::absolute(path).lexically_normal().string();p.low=low;
    std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("cannot read "+path);
    char c;while(f.get(c)) {p.bytes.push_back(uint8_t(c));if(p.bytes.size()>8192)throw std::runtime_error("program exceeds8192 bytes");}
    if(!f.eof())throw std::runtime_error("program read failed");
    p.schedule=cpu_timing::compile(p.bytes,low);p.sha=digest(p.bytes);return p;
}
void exclusive(const std::string& path,const std::vector<uint8_t>& bytes) {
    FILE* f=std::fopen(path.c_str(),"wbx");if(!f)throw std::runtime_error("cannot exclusively create "+path);
    bool ok=std::fwrite(bytes.data(),1,bytes.size(),f)==bytes.size();if(std::fclose(f))ok=false;
    if(!ok)throw std::runtime_error("write failed: "+path);
}
void le(std::vector<uint8_t>& b,uint64_t value,unsigned n) {for(unsigned i=0;i<n;++i)b.push_back(uint8_t(value>>(8*i)));}
struct Compiler {
    std::vector<Record> tape;std::vector<std::string> ledger,actions;
    unsigned din=0,pins=2,carry=0,event_first=0,status_mask=3,expected=0;
    uint64_t origin=0,capture=0,event_end=0,first_key=std::numeric_limits<uint64_t>::max();
    void emit(uint64_t t,unsigned op,unsigned data,unsigned bus,const std::string& program,const std::string& kind) {
        if(t>MAX_TIME||(!tape.empty()&&t<=tape.back().time))throw std::runtime_error("nonincreasing or overflowing tape timestamp");
        if(tape.size()>=4096)throw std::runtime_error("tape exceeds4096 records");
        unsigned index=unsigned(tape.size());tape.push_back({t,op,data,bus});
        std::ostringstream a;a<<"{\"index\":"<<index<<",\"program\":"<<quote(program)<<",\"kind\":"<<quote(kind)<<",\"timestamp\":"<<t<<",\"opcode\":"<<op<<",\"data\":"<<data<<",\"pins_field\":"<<bus<<",\"held_din\":"<<din<<",\"held_bus_pins\":"<<pins<<'}';actions.push_back(a.str());
    }
    void program(const Program& p,const std::string& name) {
        unsigned incoming=carry;
        for(const auto& e:p.schedule.edges) {
            auto t=origin+cpu_timing::master_offset(e.cycle,incoming);if(t>MAX_TIME)throw std::runtime_error("timestamp overflow");
            int index=-1;
            if(e.kind!=cpu_timing::Kind::fetch)index=int(tape.size());
            std::ostringstream row;row<<"{\"program\":"<<quote(name)<<",\"kind\":"<<quote(cpu_timing::name(e.kind))<<",\"byte_offset\":"<<e.offset<<",\"cpu_cycle\":"<<e.cycle<<",\"program_origin\":"<<origin<<",\"carry_in_thirds\":"<<incoming<<",\"ideal_residue_thirds\":"<<((4*e.cycle+incoming)%3)<<",\"timestamp\":"<<t<<",\"reg\":"<<e.reg<<",\"value\":"<<e.value<<",\"tape_index\":"<<index<<'}';ledger.push_back(row.str());
            if(e.kind==cpu_timing::Kind::poll) {
                pins=3;emit(t,1,din,pins,name,"busy_poll");
            } else if(e.kind==cpu_timing::Kind::status) {
                pins=3;unsigned mask=((status_mask&128)?4:0)|(status_mask&3);emit(t,2,expected,mask,name,"status_read");
            } else if(e.kind==cpu_timing::Kind::address||e.kind==cpu_timing::Kind::data) {
                bool data=e.kind==cpu_timing::Kind::data;din=e.value;pins=data?1:0;
                emit(t,0,din,pins,name,data?"data_assert":"address_assert");
                if(name=="event"&&data&&e.reg==8&&(e.value&0x78)&&first_key==std::numeric_limits<uint64_t>::max())first_key=t;
                pins|=2;emit(t+2,0,din,pins,name,data?"data_release":"address_release");
            } else if(e.kind==cpu_timing::Kind::end) {
                if(name=="init") {capture=t;emit(t,3,0,0,name,"capture");}
                else {event_end=t;emit(t,4,0,0,name,"end");}
            }
        }
        origin+=cpu_timing::master_offset(p.schedule.cycles,incoming);carry=cpu_timing::carry_after(p.schedule.cycles,incoming);
        if(name=="init")event_first=unsigned(tape.size());
    }
};
std::string array(const std::vector<std::string>& v) {std::ostringstream o;o<<"[\n";for(size_t i=0;i<v.size();++i)o<<(i?",\n":"")<<v[i];return o.str()+"\n]";}
}
int main(int argc,char** argv) {
    try {
        unsigned init_low=0,event_low=0,mask=3,expected=0;std::vector<std::string> args;
        for(int i=1;i<argc;++i) {
            std::string a=argv[i];
            if(a=="--init-low"||a=="--event-low"||a=="--status-mask"||a=="--expected-status") {
                if(++i>=argc)throw std::runtime_error("missing option value");
                auto n=number(argv[i]);
                if(n>255)throw std::runtime_error("option exceeds255");
                if(a=="--init-low")init_low=unsigned(n);else if(a=="--event-low")event_low=unsigned(n);else if(a=="--status-mask")mask=unsigned(n);else expected=unsigned(n);
            } else if(a.rfind("--",0)==0)throw std::runtime_error("unknown option: "+a);else args.push_back(a);
        }
        if(args.size()!=3)throw std::runtime_error("usage: compile_tape [--init-low N] [--event-low N] [--status-mask N] [--expected-status N] INIT EVENT OUTPUT.tape");
        if(mask&~131u)throw std::runtime_error("status mask supports only bits7,1,0");
        const std::string metadata=args[2]+".json";
        if(std::filesystem::exists(args[2])||std::filesystem::exists(metadata))throw std::runtime_error("output already exists");
        auto init=load(args[0],init_low),event=load(args[1],event_low);Compiler c;c.status_mask=mask;c.expected=expected;c.program(init,"init");unsigned init_carry=c.carry;c.program(event,"event");
        std::vector<uint8_t> bytes={'J','T','5','1','T','A','P','1'};le(bytes,1,4);le(bytes,64,4);le(bytes,c.tape.size(),4);le(bytes,c.event_first,4);le(bytes,c.first_key,8);le(bytes,c.capture,8);le(bytes,c.event_end,8);le(bytes,0,8);le(bytes,0,8);
        for(auto r:c.tape)le(bytes,(r.time<<16)|(uint64_t(r.op)<<12)|(uint64_t(r.data)<<4)|(r.pins<<1),8);
        std::ostringstream j;j<<"{\n\"format\":\"JT51TAP1\",\"version\":1,\"header_bytes\":64,\"length\":"<<c.tape.size()<<",\"event_first\":"<<c.event_first<<",\"first_event_KEY_offset\":"<<c.first_key<<",\"capture_offset\":"<<c.capture<<",\"event_end_offset\":"<<c.event_end<<",\"init_cycles\":"<<init.schedule.cycles<<",\"event_cycles\":"<<event.schedule.cycles<<",\"init_carry_out_thirds\":"<<init_carry<<",\"event_carry_out_thirds\":"<<c.carry<<",\"init_table_low\":"<<init.low<<",\"event_table_low\":"<<event.low<<",\"status_mask\":"<<mask<<",\"expected_status\":"<<expected<<",\"source_sha256\":"<<quote(TAPE_SOURCE_SHA)<<",\"scheduler_sha256\":"<<quote(SCHEDULE_SOURCE_SHA)<<",\"init_path\":"<<quote(init.path)<<",\"init_sha256\":"<<quote(init.sha)<<",\"event_path\":"<<quote(event.path)<<",\"event_sha256\":"<<quote(event.sha)<<",\"tape_sha256\":"<<quote(digest(bytes))<<",\"cpu_edges\":"<<array(c.ledger)<<",\"actions\":"<<array(c.actions)<<"\n}\n";
        exclusive(args[2],bytes);auto text=j.str();exclusive(metadata,std::vector<uint8_t>(text.begin(),text.end()));
        std::cout<<"{\"length\":"<<c.tape.size()<<",\"event_first\":"<<c.event_first<<",\"first_event_KEY_offset\":"<<c.first_key<<",\"capture_offset\":"<<c.capture<<",\"event_end_offset\":"<<c.event_end<<",\"tape_sha256\":"<<quote(digest(bytes))<<"}\n";
    } catch(const std::exception& e) {std::cerr<<"compile_tape: "<<e.what()<<'\n';return 1;}
}
