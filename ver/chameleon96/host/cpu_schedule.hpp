#pragma once
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace cpu_timing {
enum class Kind { fetch, poll, address, data, status, end };
struct Edge {
    Kind kind;
    uint64_t cycle;
    unsigned offset;
    unsigned reg;
    unsigned value;
};
struct Schedule {
    std::vector<Edge> edges;
    uint64_t cycles=0;
    uint64_t writes=0;
};
inline const char* name(Kind kind) {
    switch(kind) {
        case Kind::fetch: return "table_read";
        case Kind::poll: return "busy_poll";
        case Kind::address: return "address_assert";
        case Kind::data: return "data_assert";
        case Kind::status: return "status_read";
        case Kind::end: return "end";
    }
    throw std::runtime_error("unknown CPU edge kind");
}
inline uint64_t master_offset(uint64_t cycles,unsigned carry) {
    if(carry>2 || cycles>(std::numeric_limits<uint64_t>::max()-carry)/4)
        throw std::runtime_error("CPU clock conversion overflow");
    return (4*cycles+carry)/3;
}
inline unsigned carry_after(uint64_t cycles,unsigned carry) {
    master_offset(cycles,carry);
    return unsigned((4*cycles+carry)%3);
}
inline Schedule compile(const std::vector<uint8_t>& bytes,unsigned table_low=0) {
    if(bytes.empty() || bytes.size()>8192 || table_low>255)
        throw std::runtime_error("invalid CPU table geometry");
    Schedule result;
    uint64_t tick=0;
    unsigned cursor=0;
    auto fetch=[&]() {
        if(cursor>=bytes.size()) throw std::runtime_error("truncated CPU table");
        tick+=14; // caller JSR7, LDA (pointer)7; read after the LDA cycles
        unsigned offset=cursor;
        unsigned value=bytes[cursor++];
        result.edges.push_back({Kind::fetch,tick,offset,0,value});
        tick+=17+(((table_low+cursor)&255)==0 ? 4:0); // INC, branch, optional high INC, RTS
        return value;
    };
    auto poll=[&](unsigned offset,unsigned reg) {
        tick+=12; // caller JSR7 and LDA absolute5
        result.edges.push_back({Kind::poll,tick,offset,reg,0});
        tick+=9; // BMI not taken2 and RTS7; taken busy is a failure, never stretched
    };
    while(cursor<bytes.size()) {
        unsigned op_offset=cursor;
        unsigned op=fetch();
        tick+=2; // CMP #0
        if(op==0) {
            tick+=4;
            if(cursor!=bytes.size()) throw std::runtime_error("trailing CPU table bytes");
            result.edges.push_back({Kind::end,tick,op_offset,0,0});
            result.cycles=tick;
            return result;
        }
        tick+=2+2; // BEQ false, CMP #1
        if(op==1) {
            tick+=4;
            poll(op_offset,0);
            unsigned reg=fetch();
            tick+=5;
            result.edges.push_back({Kind::address,tick,op_offset,reg,reg});
            poll(op_offset,reg);
            unsigned value=fetch();
            tick+=5;
            result.edges.push_back({Kind::data,tick,op_offset,reg,value});
            ++result.writes;
            tick+=4; // BRA dispatch
            continue;
        }
        tick+=2+2; // BEQ false, CMP #2
        if(op==2) {
            tick+=4;
            unsigned lo=fetch(); tick+=4;
            unsigned hi=fetch(); tick+=4;
            uint64_t count=lo+(hi<<8);
            tick+=28*count+4*(count/256)+12;
            continue; // final taken BEQ returns directly to dispatch
        }
        tick+=2+2; // BEQ false, CMP #3
        if(op==3) {
            tick+=4+5; // BEQ read_status, LDA YM+1
            result.edges.push_back({Kind::status,tick,op_offset,0,0});
            tick+=4+4; // STA status, BRA dispatch
            continue;
        }
        throw std::runtime_error("invalid CPU table opcode");
    }
    throw std::runtime_error("CPU table missing END");
}
}
