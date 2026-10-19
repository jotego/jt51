// Public continuous-versus-segmented replay. No internal DUT reads or deposits.
#include "Vjt51_replay_lane.h"
#include "verilated.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
static uint64_t le(const std::vector<unsigned char>& b,unsigned p,unsigned n) { uint64_t x=0; for(unsigned i=0;i<n;i++) x|=uint64_t(b.at(p+i))<<(8*i); return x; }
static void need(bool good,const std::string& message) { if(!good) throw std::runtime_error(message); }
struct Lane {
    VerilatedContext context;
    Vjt51_replay_lane dut{&context};
    bool streaming=false;unsigned global_first=0,uploaded_records=0;
    void tick() { dut.clk=0;dut.eval();context.timeInc(10);dut.clk=1;dut.eval();context.timeInc(10); }
    void init(bool stream,unsigned event,unsigned frames,unsigned stride,unsigned mode,unsigned shift) {
        dut.rst=1;dut.start=0;dut.abort_run=0;dut.prog_we=0;dut.continue_page=0;
        streaming=stream;global_first=event;uploaded_records=0;
        dut.stream_enable=stream;dut.stream_event_first=event;dut.read_addr=0;
        dut.idle_clocks=169792;dut.event_shift=shift;dut.capture_limit=frames;
        dut.stride=stride;dut.output_mode=mode;dut.event_first=event;
        tick();tick();tick();dut.rst=0;tick();
    }
    void upload(const std::vector<uint64_t>& page) {
        need(!page.empty()&&page.size()<=4096,"page dimensions");
        for(unsigned i=0;i<page.size();i++) {
            dut.prog_addr=i;dut.prog_data=page[i];dut.prog_we=1;tick();
            dut.prog_we=0;tick();need(dut.prog_q==page[i],"page RAM readback");
        }
        dut.length=page.size();
        if(streaming) {
            unsigned local=global_first<=uploaded_records?0:global_first-uploaded_records;
            dut.event_first=std::min<unsigned>(local,page.size());uploaded_records+=page.size();
        }
    }
    std::array<uint64_t,8> journal() {
        return {uint64_t(dut.logical_clock),uint64_t(dut.debug_din),
                uint64_t(dut.debug_cs_n)|(uint64_t(dut.debug_wr_n)<<1)|(uint64_t(dut.debug_a0)<<2),
                uint64_t(dut.debug_dut_rst),uint64_t(dut.last_status),uint64_t(dut.count),
                uint64_t(dut.first_clock),uint64_t(dut.last_clock)};
    }
    std::array<uint64_t,7> frozen() {
        return {uint64_t(dut.logical_clock),uint64_t(dut.count),uint64_t(dut.last_status),
                uint64_t(dut.first_clock),uint64_t(dut.last_clock),uint64_t(dut.total_executed),
                uint64_t(dut.debug_din)|(uint64_t(dut.debug_cs_n)<<8)|(uint64_t(dut.debug_wr_n)<<9)|
                (uint64_t(dut.debug_a0)<<10)|(uint64_t(dut.debug_dut_rst)<<11)};
    }
};
int main(int argc,char** argv) { try {
    need(argc==5,"stream TAPE MODE STRIDE SHIFT");
    unsigned mode=std::stoul(argv[2]),stride=std::stoul(argv[3]),shift=std::stoul(argv[4]);
    need(mode<2&&stride>0&&stride<=2&&shift<64,"arguments");
    std::ifstream in(argv[1],std::ios::binary);need(bool(in),"input tape");
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)),{});
    need(bytes.size()>=64&&std::string(bytes.begin(),bytes.begin()+8)=="JT51TAP1","tape magic");
    unsigned n=le(bytes,16,4),event=le(bytes,20,4),frames=16384/stride;
    need(n>2&&bytes.size()==64+8*n,"tape length");
    std::vector<uint64_t> continuous;
    for(unsigned i=0;i+1<n;i++) continuous.push_back(le(bytes,64+8*i,8));
    // Extend the END-only tail with idempotent public idle-bus records. This
    // gives page boundaries throughout the nonzero feedback waveform while
    // retaining the complete original register-write and clock history.
    uint64_t end=le(bytes,64+8*(n-1),8)>>16,capture=le(bytes,32,8),held=0;
    for(auto w:continuous) if(((w>>12)&15)<2) held=w&4095;
    uint64_t newend=capture+uint64_t(frames-1)*64*stride-1024;
    need(newend>end,"nonzero post-KEY tail available");
    for(uint64_t t=end+1024;t+1024<newend;t+=4096) continuous.push_back((t<<16)|held);
    continuous.push_back((newend<<16)|(uint64_t(4)<<12));
    need(continuous.size()<=4096,"continuous oracle fits resident RAM");
    std::vector<std::vector<uint64_t>> pages(1);
    unsigned stream_event=0,inserted=0,lastsplit=0;
    for(unsigned i=0;i<continuous.size();i++) {
        if(i==event) stream_event=i+inserted;
        pages.back().push_back(continuous[i]);
        if(i+1<continuous.size() && i-lastsplit>=37 &&
           (continuous[i+1]>>16)>(continuous[i]>>16)+3) {
            uint64_t t=(continuous[i]>>16)+1;
            // Deliberately close pages on emitted sample edges after KEY,
            // exercising a delayed BRAM capture held across host loading.
            if(i>=n-1) t+=(257-shift-(t%128))%128;
            need(t+1<(continuous[i+1]>>16),"PAGE logical refill margin");
            pages.back().push_back((t<<16)|(uint64_t(5)<<12));
            pages.emplace_back();inserted++;lastsplit=i+1;
        }
    }
    need(pages.size()>10,"multiple physical page boundaries");
    Lane reference,segmented;
    reference.init(false,event,frames,stride,mode,shift);
    segmented.init(true,stream_event,frames,stride,mode,shift);
    reference.upload(continuous);segmented.upload(pages[0]);
    reference.dut.start=1;segmented.dut.start=1;reference.tick();segmented.tick();
    reference.dut.start=0;segmented.dut.start=0;
    unsigned page=0;uint64_t checked=0,stall_edges=0,pause_after_audio=0,capture_edge_pauses=0,guard=0;
    // Keep both DUTs on precisely the same logical edge. Host-only edges run
    // exclusively in the segmented runner during each page upload/refill.
    while(!reference.dut.done||!segmented.dut.done) {
        need(++guard<5000000,"logical completion bound");
        if(segmented.dut.paused) {
            auto held_state=segmented.frozen();
            need(page+1<pages.size(),"unexpected PAGE termination");
            if(segmented.dut.count>6000/stride) pause_after_audio++;
            if(segmented.dut.first_clock &&
               (segmented.dut.logical_clock-segmented.dut.first_clock)%64==0) capture_edge_pauses++;
            unsigned delay=13+(page*7919)%113;
            for(unsigned j=0;j<delay;j++) {
                segmented.tick();stall_edges++;
                need(segmented.frozen()==held_state,"public/capture state changed during host stall");
            }
            segmented.upload(pages[++page]);stall_edges+=2*pages[page].size();
            need(segmented.frozen()==held_state,"public/capture state changed during RAM upload");
            segmented.dut.continue_page=1;segmented.tick();stall_edges++;
            segmented.dut.continue_page=0;
            while(segmented.dut.paused) {
                segmented.tick();stall_edges++;
                // Page accounting changes at CONTINUE; actual execution does not.
                need(segmented.dut.logical_clock==held_state[0]&&segmented.dut.count==held_state[1],"logical/capture clocks changed during RAM refill");
            }
            need(segmented.dut.logical_clock==reference.dut.logical_clock,"resume inserted logical clock");
        }
        reference.tick();segmented.tick();
        need(!reference.dut.failed&&!segmented.dut.failed,"lane failure ref="+std::to_string(reference.dut.error_code)+" stream="+std::to_string(segmented.dut.error_code));
        need(reference.journal()==segmented.journal(),"public bus/status/capture journal differs at logical clock "+std::to_string(reference.dut.logical_clock));
        checked++;
    }
    need(page+1==pages.size()&&pause_after_audio>0&&capture_edge_pauses>0,"all pages consumed through nonzero audio region");
    need(reference.dut.count==frames&&segmented.dut.count==frames,"full capture");
    need(segmented.dut.total_executed==continuous.size()+inserted,"complete stream accounting");
    need(segmented.dut.page_index==pages.size()-1,"page accounting");
    int low[2]={0,0},high[2]={0,0};uint64_t pcmhash=1469598103934665603ULL;
    for(unsigned i=0;i<frames;i++) {
        reference.dut.read_addr=i;segmented.dut.read_addr=i;reference.tick();segmented.tick();
        uint64_t v=reference.dut.read_data;
        need(v==segmented.dut.read_data,"public PCM/status mismatch at sample "+std::to_string(i));
        int a[2]={int16_t(v>>16),int16_t(v)};
        for(unsigned c=0;c<2;c++){low[c]=std::min(low[c],a[c]);high[c]=std::max(high[c],a[c]);}
        for(unsigned j=0;j<5;j++){pcmhash^=(v>>(8*j))&255;pcmhash*=1099511628211ULL;}
    }
    need(low[0]<0&&high[0]>0&&low[1]<0&&high[1]>0,"bipolar nonzero feedback output");
    std::cout<<"PASS stream mode="<<mode<<" stride="<<stride<<" shift="<<shift<<" logical_journal_rows="<<checked<<" pcm_rows="<<frames<<" pages="<<pages.size()<<" host_only_edges="<<stall_edges<<" pauses_after_audio="<<pause_after_audio<<" capture_edge_pauses="<<capture_edge_pauses<<" left="<<low[0]<<":"<<high[0]<<" right="<<low[1]<<":"<<high[1]<<" pcm_fnv64="<<pcmhash<<"\n";
    return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
