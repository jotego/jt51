// Executes the actual native C stream driver against the real RTL controller.
#include "Vc96_jt51_control.h"
#include "verilated.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
static VerilatedContext context;
static Vc96_jt51_control controller{&context};
static uint32_t toggle;
static void tick() { controller.clk=0;controller.eval();context.timeInc(10);controller.clk=1;controller.eval();context.timeInc(10); }
extern "C" double c96_transport_test_now() { return context.time()*1e-9; }
extern "C" void c96_transport_test_sleep(unsigned ms) { for(uint64_t i=0;i<uint64_t(ms)*50000;i++)tick(); }
extern "C" uint32_t c96_transport_test_command(unsigned op,uint32_t arg) {
    if(op>63||arg>0xffffff){std::fprintf(stderr,"bad simulated command width\n");std::exit(3);}
    toggle^=0x80000000u;controller.gp_out=toggle|(op<<24)|arg;
    unsigned guard=0;do { tick();if(++guard>1000){std::fprintf(stderr,"simulated command timeout\n");std::exit(3);} }while((controller.gp_in&0x80000000u)!=toggle);
    uint32_t reply=controller.gp_in;
    if(((reply>>24)&63)!=op||(reply&0x40000000u)){std::fprintf(stderr,"RTL rejected native op%u arg%u reply%08x\n",op,arg,reply);std::exit(3);}
    return reply&0xffffff;
}
extern "C" int c96_native_main(int,char**);
int main(int argc,char**argv) {
    controller.rst=1;controller.gp_out=0;for(unsigned i=0;i<6;i++)tick();controller.rst=0;for(unsigned i=0;i<6;i++)tick();
    int first_argc=(argc==10 && !std::strcmp(argv[7],"--then-batch"))?7:argc;
    int result=c96_native_main(first_argc,argv);
    if(result==0 && first_argc==7 && argc==10) {
        char batch_name[]="batch";
        char *batch_args[]={argv[0],batch_name,argv[8],argv[3],argv[9],argv[5],argv[6],nullptr};
        result=c96_native_main(7,batch_args);
    }
    controller.final();return result;
}
