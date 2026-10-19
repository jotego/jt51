// Small real-DUT lane qualification against an already committed CPU CSV.
#include "Vjt51_replay_lane.h"
#include "verilated.h"
#include <cstdint>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <array>
static uint64_t le(const std::vector<unsigned char>& b,unsigned off,unsigned n) {uint64_t x=0;for(unsigned i=0;i<n;++i)x|=uint64_t(b.at(off+i))<<(8*i);return x;}
int main(int argc,char**argv) {try {
 if(argc!=8)throw std::runtime_error("TAPE GOLDEN CSVOUT IDLE SHIFT STRIDE MODE");
 VerilatedContext context;Vjt51_replay_lane top{&context};
 auto tick=[&](){top.clk=0;top.eval();context.timeInc(5);top.clk=1;top.eval();context.timeInc(5);};
 std::ifstream in(argv[1],std::ios::binary);if(!in)throw std::runtime_error("missing tape");
 std::vector<unsigned char>b((std::istreambuf_iterator<char>(in)),{});
 if(b.size()<64||std::string(b.begin(),b.begin()+8)!="JT51TAP1"||le(b,8,4)!=1||le(b,12,4)!=64)throw std::runtime_error("bad tape header");
 const unsigned n=le(b,16,4),first=le(b,20,4);if(!n||n>4096||b.size()!=64+8*n)throw std::runtime_error("bad tape length");
 std::ifstream golden(argv[2]);if(!golden)throw std::runtime_error("missing golden");std::string line;std::getline(golden,line);
 if(line!="sample,left,right,xleft,xright,clock,status")throw std::runtime_error("golden header");
 std::vector<std::array<int64_t,7>> rows;while(std::getline(golden,line)){std::stringstream ss(line);std::string cell;std::array<int64_t,7>a{};unsigned j=0;while(std::getline(ss,cell,',')){if(j>=7)throw std::runtime_error("golden columns");a[j++]=std::stoll(cell);}if(j!=7||a[0]!=int64_t(rows.size()))throw std::runtime_error("golden sequence");rows.push_back(a);}
 if(rows.empty()||rows.size()>16384)throw std::runtime_error("golden bounds");
 top.rst=1;top.start=0;top.abort_run=0;top.prog_we=0;top.prog_addr=0;top.prog_data=0;top.read_addr=0;tick();tick();tick();top.rst=0;tick();
 for(unsigned i=0;i<n;++i){uint64_t word=le(b,64+i*8,8);top.prog_addr=i;top.prog_data=word;top.prog_we=1;tick();top.prog_we=0;tick();if(top.prog_q!=word)throw std::runtime_error("tape RAM readback");}
 top.length=n;top.event_first=first;top.idle_clocks=std::stoull(argv[4]);top.event_shift=std::stoul(argv[5]);top.stride=std::stoul(argv[6]);top.output_mode=std::stoul(argv[7]);top.capture_limit=rows.size();top.start=1;tick();top.start=0;
 uint64_t guard=0;while(!top.done && guard++<100000000)tick();
 if(!top.done||top.failed){std::cerr<<"lane failure code="<<unsigned(top.error_code)<<" at="<<uint64_t(top.failure_clock)<<" executed="<<unsigned(top.executed)<<" count="<<unsigned(top.count)<<"\n";return 2;}
 if(top.count!=rows.size()||top.executed!=n)throw std::runtime_error("terminal count");
 if(std::ifstream(argv[3]).good())throw std::runtime_error("output exists");std::ofstream out(argv[3]);if(!out)throw std::runtime_error("output create");out<<(top.output_mode?"sample,xleft,xright,clock,status\n":"sample,left,right,clock,status\n");
 unsigned mismatches=0;int lo[2]={0,0},hi[2]={0,0};
 for(unsigned i=0;i<rows.size();++i){top.read_addr=i;tick();uint64_t v=top.read_data;int left=int16_t(v>>16),right=int16_t(v);unsigned status=(v>>32)&255;uint64_t clock=top.first_clock+uint64_t(i)*64*top.stride;out<<i<<','<<left<<','<<right<<','<<clock<<','<<status<<'\n';
   const unsigned lc=top.output_mode?3:1,rc=top.output_mode?4:2;
   if(left!=rows[i][lc]||right!=rows[i][rc]||clock!=uint64_t(rows[i][5])||status!=rows[i][6]){if(mismatches++<8)std::cerr<<"mismatch row="<<i<<" got="<<left<<','<<right<<','<<clock<<','<<status<<" expected="<<rows[i][lc]<<','<<rows[i][rc]<<','<<rows[i][5]<<','<<rows[i][6]<<'\n';}
   lo[0]=std::min(lo[0],left);lo[1]=std::min(lo[1],right);hi[0]=std::max(hi[0],left);hi[1]=std::max(hi[1],right);
 }
 if(top.last_clock!=top.first_clock+uint64_t(rows.size()-1)*64*top.stride)throw std::runtime_error("last clock");
 std::cout<<"rows="<<rows.size()<<" records="<<n<<" first="<<uint64_t(top.first_clock)<<" last="<<uint64_t(top.last_clock)<<" mismatches="<<mismatches<<" left="<<lo[0]<<":"<<hi[0]<<" right="<<lo[1]<<":"<<hi[1]<<"\n";
 top.final();return mismatches?1:0;
 }catch(std::exception&e){std::cerr<<e.what()<<'\n';return 3;}}
