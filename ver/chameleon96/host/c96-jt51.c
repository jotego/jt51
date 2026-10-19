/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "sha256.h"

enum { INFO, CAPS, LANE, MASK, TADDR, TLOW, TMID, THIGH, TRLOW, TRMID,
       TRHIGH, COMMIT, START, STATUS, COUNT, ABORT, CFGSEL, CFGWRITE,
       CFGREAD, SADDR, SLOW, SHIGH, META, SOURCE, PAGE_INDEX, PAGE_NEXT, PAGE_PREVIOUS, CONTINUE, PAGE_HASH };
#ifndef C96_JT51_TRANSPORT_TEST
static volatile uint32_t *regs;
static uint32_t toggle;
#endif
static uint32_t clock_hz;
static unsigned tape_capacity, capture_capacity;
static unsigned char source_bytes[32];
static char source_hex[65];
struct tape { uint32_t length, event_first; uint64_t capture, end, key; uint64_t *words; unsigned char file_sha[32], records_sha[32]; };
struct stream { struct tape *pages, logical; unsigned count; unsigned char manifest_sha[32]; };
struct job { char name[80]; uint32_t id, shift, frames, stride, mode; uint64_t idle; };

static void die(const char *s) { fprintf(stderr,"%s\n",s); exit(1); }
#ifdef C96_JT51_TRANSPORT_TEST
extern double c96_transport_test_now(void);
extern void c96_transport_test_sleep(unsigned ms);
extern uint32_t c96_transport_test_command(unsigned op,uint32_t arg);
#endif
static double now(void) {
#ifdef C96_JT51_TRANSPORT_TEST
    return c96_transport_test_now();
#else
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC,&t)) die("clock_gettime failed");
    return t.tv_sec+t.tv_nsec*1e-9;
#endif
}
static uint64_t number(const char *s,uint64_t maximum) {
    char *end; errno=0;
    unsigned long long n=strtoull(s,&end,0);
    if (errno || !*s || *s=='-' || *end || n>maximum) die("Invalid numeric argument");
    return (uint64_t)n;
}
static void sleep_ms(unsigned ms) {
#ifdef C96_JT51_TRANSPORT_TEST
    c96_transport_test_sleep(ms);
#else
    struct timespec t={ms/1000,(long)(ms%1000)*1000000};
    while (nanosleep(&t,&t) && errno==EINTR) {}
#endif
}
static uint32_t command(unsigned op,uint32_t arg) {
#ifdef C96_JT51_TRANSPORT_TEST
    return c96_transport_test_command(op,arg);
#else
    if (op>63 || arg>0xffffff) die("Invalid FPGA command width");
    double deadline=now()+1;
    unsigned polls=0; uint32_t v;
    toggle^=0x80000000u;
    __sync_synchronize(); regs[4]=toggle|(op<<24)|arg; __sync_synchronize();
    for (;;) {
        v=regs[5];
        if ((v&0x80000000u)==toggle) {
            __sync_synchronize();
            if (regs[5]==v) break;
        }
        if ((++polls&255)==0 && now()>deadline) die("FPGA command timeout");
    }
    if (((v>>24)&63)!=op) die("FPGA reply opcode mismatch");
    if (v&0x40000000u) {
        fprintf(stderr,"FPGA rejected opcode%u argument%u error%u\n",op,arg,v&0xffffff);
        exit(1);
    }
    return v&0xffffff;
#endif
}
static uint64_t meta48(unsigned lo) {
    uint64_t a=command(META,lo),b=command(META,lo+1);
    return a|(b<<24);
}
static void connect_fpga(const char *expected_path) {
    char expected[66]={0};FILE *f;
#ifndef C96_JT51_TRANSPORT_TEST
    char model[64]={0},state[32]={0};
    f=fopen("/proc/device-tree/model","rb");
    if (!f || !fgets(model,sizeof(model),f)) die("Cannot identify board");
    fclose(f);
    if (strcmp(model,"NovTech CV96")) die("This tool requires NovTech CV96");
    f=fopen("/sys/class/fpga_manager/fpga0/state","r");
    if (!f || !fgets(state,sizeof(state),f)) die("Cannot read FPGA state");
    fclose(f);
    if (strcmp(state,"operating\n")) die("FPGA is not operating");
#endif
    f=fopen(expected_path,"r");
    if (!f || !fgets(expected,sizeof(expected),f)) die("Cannot read expected source hash");
    if (strlen(expected)==65 && expected[64]=='\n') expected[64]=0;
    if (strlen(expected)!=64 || fgetc(f)!=EOF) die("Expected hash must contain64 hex digits");
    fclose(f);
    for (unsigned i=0;i<64;i++) {
        if (!isxdigit((unsigned char)expected[i])) die("Invalid expected hash");
        expected[i]=(char)tolower((unsigned char)expected[i]);
    }
    #ifndef C96_JT51_TRANSPORT_TEST
    int lock=open("/var/lock/c96-capture.lock",O_CREAT|O_RDWR,0600);
    if (lock<0 || flock(lock,LOCK_EX|LOCK_NB)) die("Another FPGA command or load is running");
    int fd=open("/dev/mem",O_RDWR|O_SYNC);
    if (fd<0) die("Cannot open /dev/mem");
    regs=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0xff706000);
    if (regs==MAP_FAILED) die("Cannot map FPGA manager");
    close(fd); toggle=regs[5]&0x80000000u;
    #endif
    if (command(INFO,0)!=0x965102) die("Unexpected JT51 FPGA protocol");
    uint32_t caps=command(CAPS,0);
    if ((caps&255)!=4 || ((caps>>8)&255)!=12 || (caps>>16)!=14) die("Unexpected FPGA capacities");
    tape_capacity=1u<<((caps>>8)&255); capture_capacity=1u<<(caps>>16);
    for (unsigned i=0;i<16;i++) {
        uint32_t word=command(SOURCE,15-i);
        if (word>65535) die("Invalid source hash word");
        snprintf(source_hex+4*i,5,"%04x",word);
        source_bytes[2*i]=(unsigned char)(word>>8);
        source_bytes[2*i+1]=(unsigned char)word;
    }
    if (strcmp(source_hex,expected)) die("Loaded FPGA source hash differs from expected build");
    command(LANE,0); clock_hz=(uint32_t)meta48(11);
    if (clock_hz!=50000000) die("Unexpected FPGA execution clock");
}
static uint32_t get32(const unsigned char *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static uint64_t get64(const unsigned char *p) { return get32(p)|((uint64_t)get32(p+4)<<32); }
static void put32(unsigned char *p,uint32_t v) { for (unsigned i=0;i<4;i++) p[i]=(unsigned char)(v>>(8*i)); }
static void put64(unsigned char *p,uint64_t v) { for (unsigned i=0;i<8;i++) p[i]=(unsigned char)(v>>(8*i)); }
static void hex_hash(const unsigned char *hash,char hex[65]) {
    for(unsigned i=0;i<32;i++) snprintf(hex+2*i,3,"%02x",hash[i]);
}
static void parse_hash(const char *hex,unsigned char hash[32]) {
    if(strlen(hex)!=64) die("Invalid SHA256 length");
    for(unsigned i=0;i<32;i++) {
        unsigned x=0;
        for(unsigned j=0;j<2;j++) {
            unsigned char c=(unsigned char)hex[2*i+j];
            if(c>='0'&&c<='9') x=(x<<4)|(c-'0');
            else if(c>='a'&&c<='f') x=(x<<4)|(c-'a'+10);
            else die("SHA256 must be lowercase hexadecimal");
        }
        hash[i]=(unsigned char)x;
    }
}
static struct tape read_page(const char *path,int streaming) {
    struct tape t={0}; unsigned char h[64],w[8];struct sha256_ctx file_hash,records_hash;
    sha256_init(&file_hash);sha256_init(&records_hash);
    FILE *f=fopen(path,"rb");
    if (!f || fread(h,1,64,f)!=64) die("Cannot read tape header");
    sha256_update(&file_hash,h,64);
    if (memcmp(h,"JT51TAP1",8) || get32(h+8)!=1 || get32(h+12)!=64 || get64(h+48) || get64(h+56))
        die("Invalid tape header");
    t.length=get32(h+16); t.event_first=get32(h+20); t.key=get64(h+24);
    t.capture=get64(h+32); t.end=get64(h+40);
    if (!t.length || t.length>4096 || t.event_first>t.length) die("Invalid tape dimensions");
    t.words=calloc(t.length,sizeof(*t.words));
    if (!t.words) die("Cannot allocate tape");
    unsigned capture=0;
    for (unsigned i=0;i<t.length;i++) {
        if (fread(w,1,8,f)!=8) die("Truncated tape");
        sha256_update(&file_hash,w,8);sha256_update(&records_hash,w,8);
        uint64_t v=get64(w); unsigned op=(unsigned)((v>>12)&15);
        if ((v&1) || op>(streaming?5:4) || (i && (v>>16)<=(t.words[i-1]>>16))) die("Malformed tape action/order");
        if (op>=3 && (v&4095)) die("Malformed tape control");
        if (op==3) {
            if ((v>>16)!=t.capture || i>=t.event_first) die("Invalid capture action");
            capture++;
        }
        if (op>=4 && i+1!=t.length) die("Early tape termination");
        t.words[i]=v;
    }
    unsigned last=(unsigned)((t.words[t.length-1]>>12)&15);
    if (capture>1 || (!streaming&&capture!=1) ||
        (last!=4 && (!streaming||last!=5)) ||
        (t.words[t.length-1]>>16)!=t.end || fgetc(f)!=EOF) die("Invalid tape termination/length");
    fclose(f);sha256_final(&file_hash,t.file_sha);sha256_final(&records_hash,t.records_sha);return t;
}
static void fields(char *line,char **out,unsigned n) {
    size_t len=strlen(line);
    if(!len||line[len-1]!='\n')die("Manifest line must end in newline");
    line[len-1]=0;out[0]=line;
    for(unsigned i=1;i<n;i++) { char *tab=strchr(out[i-1],'\t');if(!tab)die("Missing manifest field");*tab=0;out[i]=tab+1; }
    if(strchr(out[n-1],'\t'))die("Extra manifest fields");
}
static struct stream read_stream(const char *path) {
    struct stream s={0};char line[1024],*part[5];unsigned char expected[32],record_bytes[8];
    struct sha256_ctx manifest_hash,records_hash;sha256_init(&manifest_hash);sha256_init(&records_hash);
    FILE *f=fopen(path,"rb");if(!f||!fgets(line,sizeof(line),f))die("Cannot read stream manifest");
    sha256_update(&manifest_hash,line,strlen(line));fields(line,part,5);
    if(strcmp(part[0],"JT51STREAM2"))die("Invalid stream manifest format");
    s.logical.length=(uint32_t)number(part[1],0xffffffffu);s.logical.event_first=(uint32_t)number(part[2],0xffffffffu);
    s.count=(unsigned)number(part[3],4096);parse_hash(part[4],expected);
    if(!s.logical.length||s.logical.length>4000000||!s.count||!s.logical.event_first||s.logical.event_first>=s.logical.length)die("Invalid stream dimensions");
    s.pages=calloc(s.count,sizeof(*s.pages));if(!s.pages)die("Cannot allocate stream pages");
    uint32_t total=0;unsigned captures=0,pins=2;uint64_t previous_time=0;
    const char *slash=strrchr(path,'/');size_t prefix=slash?(size_t)(slash-path)+1:0;
    for(unsigned p=0;p<s.count;p++) {
        if(!fgets(line,sizeof(line),f))die("Truncated manifest");
        sha256_update(&manifest_hash,line,strlen(line));fields(line,part,5);
        if(number(part[0],0xffffffffu)!=p)die("Missing or reordered stream page");
        if(!*part[1]||strlen(part[1])>127||part[1][0]=='.'||strstr(part[1],".."))die("Unsafe page basename");
        for(const char *c=part[1];*c;c++)if(!isalnum((unsigned char)*c)&&*c!='_'&&*c!='-'&&*c!='.')die("Unsafe page basename");
        char page_path[1024];if(prefix+strlen(part[1])+1>sizeof(page_path))die("Page path too long");
        memcpy(page_path,path,prefix);strcpy(page_path+prefix,part[1]);
        struct tape t=read_page(page_path,1);unsigned char declared[32];parse_hash(part[2],declared);
        if(memcmp(declared,t.file_sha,32)||number(part[3],4096)!=t.length||number(part[4],4096)!=t.event_first)die("Page hash/dimensions mismatch");
        uint32_t local=s.logical.event_first<=total?0:s.logical.event_first-total;if(local>t.length)local=t.length;
        if(local!=t.event_first)die("Page EVENT boundary differs from global boundary");
        if(t.length>s.logical.length-total)die("Stream pages exceed declared total records");
        if(p && (t.words[0]>>16)<=previous_time+1)die("No logical margin after PAGE");
        if(!p){s.logical.capture=t.capture;s.logical.key=t.key;}
        if(t.capture!=s.logical.capture||t.key!=s.logical.key)die("Page capture/KEY identity mismatch");
        for(unsigned i=0;i<t.length;i++) {
            uint64_t v=t.words[i];unsigned op=(unsigned)((v>>12)&15);
            if(op<2)pins=(unsigned)((v>>1)&7);else if(op==2)pins=3;
            if(op==3){captures++;if(total+i>=s.logical.event_first)die("CAPTURE follows EVENT");}
            if(op==5&&!(pins&2))die("PAGE may not split active WR pulse");
            put64(record_bytes,v);sha256_update(&records_hash,record_bytes,8);
        }
        unsigned last=(unsigned)((t.words[t.length-1]>>12)&15);
        if(last!=(p+1==s.count?4u:5u))die("Missing PAGE or final END");
        previous_time=t.end;total+=t.length;s.pages[p]=t;
    }
    if(fgetc(f)!=EOF||total!=s.logical.length||captures!=1)die("Stream count/CAPTURE/trailing-data mismatch");
    fclose(f);sha256_final(&manifest_hash,s.manifest_sha);sha256_final(&records_hash,s.logical.records_sha);
    if(memcmp(expected,s.logical.records_sha,32))die("Ordered stream record SHA256 mismatch");
    s.logical.end=previous_time;return s;
}
static size_t read_jobs(const char *path,struct job **result) {
    FILE *f=fopen(path,"r"); char line[512]; size_t n=0,capacity=0;
    struct job *jobs=NULL;
    if (!f || !fgets(line,sizeof(line),f) ||
        strcmp(line,"name\trun_id\tidle_clocks\tevent_shift\tframes\tstride\tmode\n")) die("Invalid jobs header");
    while (fgets(line,sizeof(line),f)) {
        char *fields[7]; fields[0]=line;
        size_t len=strlen(line);
        if (!len || line[len-1]!='\n') die("Truncated jobs row");
        line[len-1]=0;
        for (unsigned i=1;i<7;i++) {
            char *tab=strchr(fields[i-1],'\t');
            if (!tab) die("Missing jobs field");
            *tab=0; fields[i]=tab+1;
        }
        if (strchr(fields[6],'\t') || !*fields[0] || strlen(fields[0])>=80) die("Invalid job name/field count");
        for (const char *s=fields[0];*s;s++)
            if (!isalnum((unsigned char)*s) && *s!='_' && *s!='-') die("Unsafe job name");
        if (n==100000) die("Too many jobs");
        if (n==capacity) {
            capacity=capacity ? capacity*2:64;
            void *p=realloc(jobs,capacity*sizeof(*jobs));
            if (!p) die("Cannot allocate jobs");
            jobs=p;
        }
        struct job *j=&jobs[n]; memset(j,0,sizeof(*j)); strcpy(j->name,fields[0]);
        j->id=(uint32_t)number(fields[1],0xffffff); j->idle=number(fields[2],0xffffffffffffULL);
        j->shift=(uint32_t)number(fields[3],63); j->frames=(uint32_t)number(fields[4],16384);
        j->stride=(uint32_t)number(fields[5],0xffffff); j->mode=(uint32_t)number(fields[6],1);
        if (!j->id || !j->frames || !j->stride) die("Zero job ID/count/stride");
        for (size_t i=0;i<n;i++)
            if (!strcmp(jobs[i].name,j->name) || jobs[i].id==j->id) die("Duplicate job name or ID");
        n++;
    }
    if (ferror(f) || !n) die("Invalid/empty jobs file");
    fclose(f); *result=jobs; return n;
}
static void cfg(unsigned index,uint32_t value) {
    command(CFGSEL,index); command(CFGWRITE,value);
}
static uint64_t tape_read(void) {
    uint64_t v=command(TRLOW,0),mid=command(TRMID,0),hi=command(TRHIGH,0);
    if (hi>65535) die("Invalid tape read width");
    return v|(mid<<24)|(hi<<48);
}
static void upload(const struct tape *t,int paused_ok) {
    for (unsigned lane=0;lane<4;lane++) {
        command(LANE,lane);
        if ((command(STATUS,0)&2) && (!paused_ok || command(META,22)!=1)) die("FPGA lane running without verified page pause");
    }
    command(MASK,15); command(TADDR,0);
    for (unsigned i=0;i<t->length;i++) {
        uint64_t v=t->words[i];
        command(TLOW,(uint32_t)(v&0xffffff));
        command(TMID,(uint32_t)((v>>24)&0xffffff)); command(THIGH,(uint32_t)(v>>48));
    }
    for (unsigned lane=0;lane<4;lane++) {
        command(LANE,lane); command(TADDR,0);
        for (unsigned i=0;i<t->length;i++) if (tape_read()!=t->words[i]) die("FPGA tape readback differs");
    }
    command(MASK,15); cfg(0,t->length); cfg(1,t->event_first);
    for(unsigned lane=0;lane<4;lane++) {
        command(LANE,lane);
        if(command(CFGREAD,0)!=t->length || command(CFGREAD,1)!=t->event_first)die("Uploaded page configuration readback differs");
    }
}
static void configure_job(unsigned lane,const struct job *j,const struct tape *t,const struct stream *stream) {
    command(LANE,lane); command(MASK,1u<<lane);
    cfg(2,(uint32_t)(j->idle&0xffffff)); cfg(3,(uint32_t)(j->idle>>24));
    cfg(4,j->shift); cfg(5,j->frames); cfg(6,j->mode); cfg(7,j->id); cfg(8,j->stride);
    cfg(9,stream?1:0);cfg(10,stream?(stream->logical.event_first&0xffffff):0);cfg(11,stream?(stream->logical.event_first>>24):0);
    uint32_t expected[]={t->length,t->event_first,(uint32_t)(j->idle&0xffffff),
        (uint32_t)(j->idle>>24),j->shift,j->frames,j->mode,j->id,j->stride};
    for (unsigned k=0;k<9;k++) if (command(CFGREAD,k)!=expected[k]) die("FPGA configuration readback differs");
    if(command(CFGREAD,9)!=(stream?1u:0u) ||
       command(CFGREAD,10)!=(stream?(stream->logical.event_first&0xffffff):0) ||
       command(CFGREAD,11)!=(stream?(stream->logical.event_first>>24):0))die("FPGA stream configuration readback differs");
    command(COMMIT,0);
}
static void dump(unsigned lane,const struct job *j,const struct tape *t,const struct stream *stream,const char *out) {
    command(LANE,lane);
    uint32_t status=command(STATUS,0),count=command(COUNT,0),error=command(META,1);
    uint32_t page_executed=command(META,0),executed=(uint32_t)meta48(18),page=(uint32_t)meta48(20),id=command(META,16),generation=command(META,17);
    uint64_t first=meta48(4),last=meta48(6),logical=meta48(2),failure=meta48(8);
    if ((status&2) || count>capture_capacity || count>j->frames || id!=j->id) die("Invalid terminal capture identity");
    unsigned char h[256]={0}; memcpy(h,"JT51FP02",8);
    uint32_t fields[]={256,2,0x965102,clock_hz,id,lane,count,j->frames,j->stride,j->mode,
        status,error,executed,t->length};
    for (unsigned i=0;i<14;i++) put32(h+8+4*i,fields[i]);
    put64(h+64,first); put64(h+72,last); put64(h+80,logical); put64(h+88,failure);
    put32(h+96,generation); put32(h+100,j->shift); put64(h+104,j->idle);
    memcpy(h+112,source_bytes,32); put32(h+144,t->event_first); put32(h+148,5);
    memcpy(h+160,t->records_sha,32);
    for(unsigned i=0;i<16;i++) {
        unsigned word=command(PAGE_HASH,15-i);if(word>65535)die("Invalid active page hash word");
        h[192+2*i]=(unsigned char)(word>>8);h[193+2*i]=(unsigned char)word;
    }
    put32(h+224,page+1);put32(h+228,page_executed);put32(h+232,stream?1:0);
    if(stream && (page+1!=stream->count||page_executed!=stream->pages[page].length||memcmp(h+192,stream->pages[page].file_sha,32)))die("Terminal stream page identity mismatch");
    if(!stream && (page!=0||memcmp(h+192,(unsigned char[32]){0},32)))die("Unexpected resident page identity");
    char path[1024];
    if (snprintf(path,sizeof(path),"%s/%s.bin",out,j->name)>=(int)sizeof(path)) die("Output path too long");
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);
    if (fd<0) die("Cannot exclusively create capture");
    FILE *f=fdopen(fd,"wb");
    if (!f || fwrite(h,1,sizeof(h),f)!=sizeof(h)) die("Cannot write capture header");
    if (count) command(SADDR,0);
    for (unsigned i=0;i<count;i++) {
        uint64_t v=command(SLOW,0),hi=command(SHIGH,0);
        if (hi>65535) die("Invalid capture word width");
        v|=hi<<24; unsigned char bytes[5];
        for (unsigned b=0;b<5;b++) bytes[b]=(unsigned char)(v>>(8*b));
        if (fwrite(bytes,1,5,f)!=5) die("Cannot write PCM word");
    }
    if (fflush(f) || fsync(fd) || fclose(f)) die("Cannot finish capture file");
    printf("{\"name\":\"%s\",\"lane\":%u,\"run_id\":%u,\"count\":%u,\"status\":%u,\"error\":%u,\"executed\":%u,\"first_clock\":%" PRIu64 ",\"last_clock\":%" PRIu64 "}\n",
           j->name,lane,id,count,status,error,executed,first,last);
    fflush(stdout);
    if (!(status&4) || (status&8) || error || count!=j->frames || executed!=t->length ||
        last-first!=(uint64_t)(count-1)*64*j->stride) die("Failed/incomplete FPGA result preserved");
}
static void batch(const struct tape *t,struct job *jobs,size_t n,const char *out) {
    if (mkdir(out,0700)) die("Output directory must not already exist");
    double upload_start=now(); upload(t,0); double start=now();
    double upload_seconds=start-upload_start, execution_seconds=0, export_seconds=0;
    for (size_t offset=0;offset<n;offset+=4) {
        unsigned lanes=n-offset<4 ? (unsigned)(n-offset):4;
        uint64_t maxclocks=0;
        for (unsigned lane=0;lane<lanes;lane++) {
            const struct job *j=&jobs[offset+lane];
            uint64_t finish=t->capture+(uint64_t)j->frames*j->stride*64+128;
            if (finish<t->end+j->shift) finish=t->end+j->shift;
            finish+=512+j->idle;
            if (finish>0xffffffffffffULL || finish/clock_hz>3600) die("Job exceeds logical/wall-clock bound");
            if (finish>maxclocks) maxclocks=finish;
            configure_job(lane,j,t,NULL);
        }
        double execution_start=now();
        command(START,(1u<<lanes)-1);
        double deadline=now()+(double)maxclocks/clock_hz+5;
        for (;;) {
            unsigned running=0,failed=0;
            for (unsigned lane=0;lane<lanes;lane++) {
                command(LANE,lane); uint32_t s=command(STATUS,0);
                running|=s&2; failed|=s&8;
            }
            if (!running) break;
            if (failed || now()>deadline) { command(ABORT,(1u<<lanes)-1); sleep_ms(1); break; }
            sleep_ms(1);
        }
        execution_seconds+=now()-execution_start;
        double export_start=now();
        for (unsigned lane=0;lane<lanes;lane++) dump(lane,&jobs[offset+lane],t,NULL,out);
        export_seconds+=now()-export_start;
    }
    printf("{\"state\":\"all_jobs_completed\",\"jobs\":%zu,\"seconds\":%.6f,\"source_hash\":\"%s\",\"upload_seconds\":%.6f,\"execution_seconds\":%.6f,\"export_seconds\":%.6f}\n",n,now()-start,source_hex,upload_seconds,execution_seconds,export_seconds);
}
static void page_identity(unsigned index,const unsigned char next[32],const unsigned char previous[32]) {
    command(PAGE_INDEX,index);
    for(unsigned i=0;i<16;i++) {
        unsigned n=((unsigned)next[30-2*i]<<8)|next[31-2*i];
        unsigned p=((unsigned)previous[30-2*i]<<8)|previous[31-2*i];
        command(PAGE_NEXT,(i<<20)|n);command(PAGE_PREVIOUS,(i<<20)|p);
    }
}
struct frozen_state { uint64_t clock,first,last;uint32_t count,status,id,generation,total,page; };
static struct frozen_state frozen(unsigned lane) {
    struct frozen_state f={0};command(LANE,lane);
    uint32_t status=command(STATUS,0);
    if((status&14)!=2||command(META,22)!=1)die("Lane is not paused and nonfailed");
    f.clock=meta48(2);f.first=meta48(4);f.last=meta48(6);f.count=command(COUNT,0);
    f.status=command(META,10);f.id=command(META,16);f.generation=command(META,17);
    f.total=(uint32_t)meta48(18);f.page=(uint32_t)meta48(20);return f;
}
static void wait_stream(unsigned mask,int last,double seconds,FILE *receipt) {
    double deadline=now()+seconds+10;
    for(;;) {
        unsigned ready=0,failed=0;
        for(unsigned lane=0;lane<4;lane++)if(mask&(1u<<lane)) {
            command(LANE,lane);unsigned state=command(STATUS,0);
            failed|=state&8;
            if(last ? ((state&14)==4) : ((state&14)==2&&command(META,22)==1))ready|=1u<<lane;
        }
        if(ready==mask)return;
        if(failed||now()>deadline) {
            fprintf(receipt,"{\"state\":\"failed_or_timed_out\",\"failed\":%u,\"ready_mask\":%u}\n",failed,ready);fflush(receipt);
            command(ABORT,mask);sleep_ms(1);die("Incomplete stream preserved; all active lanes aborted");
        }
        sleep_ms(1);
    }
}
static void stream_batch(const struct stream *s,struct job *jobs,size_t n,const char *out) {
    if(mkdir(out,0700))die("Output directory must not already exist");
    char path[1024],manifest_hex[65],records_hex[65];hex_hash(s->manifest_sha,manifest_hex);hex_hash(s->logical.records_sha,records_hex);
    if(snprintf(path,sizeof(path),"%s/stream-pages.jsonl",out)>=(int)sizeof(path))die("Receipt path too long");
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);FILE *receipt=fd<0?NULL:fdopen(fd,"w");if(!receipt)die("Cannot exclusively create stream receipt");
    fprintf(receipt,"{\"state\":\"stream_inputs_verified\",\"source_sha256\":\"%s\",\"manifest_sha256\":\"%s\",\"records_sha256\":\"%s\",\"total_records\":%u,\"global_event_first\":%u,\"page_count\":%u}\n",source_hex,manifest_hex,records_hex,s->logical.length,s->logical.event_first,s->count);fflush(receipt);
    double start=now(),execution_seconds=0,upload_seconds=0,export_seconds=0;
    for(size_t offset=0;offset<n;offset+=4) {
        unsigned lanes=n-offset<4?(unsigned)(n-offset):4,mask=(1u<<lanes)-1;uint64_t maxclocks=0;
        for(unsigned lane=0;lane<lanes;lane++) {
            const struct job *j=&jobs[offset+lane];uint64_t finish=s->logical.capture+(uint64_t)j->frames*j->stride*64+128;
            if(finish<s->logical.end+j->shift)finish=s->logical.end+j->shift;
            finish+=512+j->idle;if(finish>0xffffffffffffULL||finish/clock_hz>3600)die("Stream job exceeds logical/wall-clock bound");
            if(finish>maxclocks)maxclocks=finish;
        }
        double t=now();upload(&s->pages[0],0);page_identity(0,s->pages[0].file_sha,(unsigned char[32]){0});
        for(unsigned lane=0;lane<lanes;lane++)configure_job(lane,&jobs[offset+lane],&s->pages[0],s);
        upload_seconds+=now()-t;command(START,mask);uint32_t consumed=0;
        for(unsigned p=0;p<s->count;p++) {
            t=now();wait_stream(mask,p+1==s->count,(double)maxclocks/clock_hz,receipt);execution_seconds+=now()-t;
            consumed+=s->pages[p].length;
            char page_hex[65],next_hex[65];hex_hash(s->pages[p].file_sha,page_hex);
            if(p+1==s->count) {
                fprintf(receipt,"{\"state\":\"final_page_completed\",\"group_offset\":%zu,\"page\":%u,\"page_sha256\":\"%s\",\"total_records\":%u}\n",offset,p,page_hex,consumed);fflush(receipt);break;
            }
            struct frozen_state before[4];
            for(unsigned lane=0;lane<lanes;lane++) {
                before[lane]=frozen(lane);
                if(before[lane].id!=jobs[offset+lane].id||before[lane].total!=consumed||before[lane].page!=p)die("Paused stream run/page/count identity mismatch");
            }
            t=now();upload(&s->pages[p+1],1);command(MASK,mask);page_identity(p+1,s->pages[p+1].file_sha,s->pages[p].file_sha);command(COMMIT,0);
            hex_hash(s->pages[p+1].file_sha,next_hex);
            for(unsigned lane=0;lane<lanes;lane++) {
                struct frozen_state after=frozen(lane);
                if(before[lane].clock!=after.clock||before[lane].first!=after.first||before[lane].last!=after.last||before[lane].count!=after.count||before[lane].status!=after.status||before[lane].id!=after.id||before[lane].generation!=after.generation||before[lane].total!=after.total||before[lane].page!=after.page)die("Logical state advanced while uploading a stream page");
                fprintf(receipt,"{\"state\":\"page_readback_and_frozen_state_verified\",\"group_offset\":%zu,\"lane\":%u,\"run_id\":%u,\"generation\":%u,\"previous_page\":%u,\"next_page\":%u,\"previous_sha256\":\"%s\",\"next_sha256\":\"%s\",\"logical_clock\":%" PRIu64 ",\"count\":%u,\"first_clock\":%" PRIu64 ",\"last_clock\":%" PRIu64 ",\"status\":%u,\"total_executed\":%u}\n",offset,lane,after.id,after.generation,p,p+1,page_hex,next_hex,after.clock,after.count,after.first,after.last,after.status,after.total);
            }
            if(fflush(receipt)||fsync(fd))die("Cannot finish page receipt");
            command(CONTINUE,mask);upload_seconds+=now()-t;
            for(unsigned lane=0;lane<lanes;lane++) {
                command(LANE,lane);
                if(meta48(20)!=p+1||command(META,16)!=jobs[offset+lane].id)die("Continuation changed page or run identity");
                for(unsigned i=0;i<16;i++) {
                    unsigned word=command(PAGE_HASH,15-i);
                    if(word!=((unsigned)s->pages[p+1].file_sha[2*i]<<8|s->pages[p+1].file_sha[2*i+1]))die("Continuation active hash mismatch");
                }
            }
        }
        t=now();for(unsigned lane=0;lane<lanes;lane++)dump(lane,&jobs[offset+lane],&s->logical,s,out);export_seconds+=now()-t;
    }
    fprintf(receipt,"{\"state\":\"all_stream_jobs_completed\",\"jobs\":%zu}\n",n);
    if(fflush(receipt)||fsync(fd)||fclose(receipt))die("Cannot finish stream receipt");
    printf("{\"state\":\"all_stream_jobs_completed\",\"jobs\":%zu,\"pages_per_job\":%u,\"seconds\":%.6f,\"execution_seconds\":%.6f,\"upload_seconds\":%.6f,\"export_seconds\":%.6f}\n",n,s->count,now()-start,execution_seconds,upload_seconds,export_seconds);
}
int main(int argc,char **argv) {
    if(argc==3 && !strcmp(argv[1],"hash")) {
        FILE *f=fopen(argv[2],"rb");if(!f)die("Cannot read hash input");
        struct sha256_ctx hash;sha256_init(&hash);unsigned char b[1031],digest[32];size_t n;
        while((n=fread(b,1,sizeof(b),f))!=0)sha256_update(&hash,b,n);
        if(ferror(f)||fclose(f))die("Hash input read error");
        sha256_final(&hash,digest);char hex[65];hex_hash(digest,hex);puts(hex);return 0;
    }
    if(argc==4 && !strcmp(argv[1],"validate-stream")) {
        struct stream s=read_stream(argv[2]);struct job *jobs;size_t n=read_jobs(argv[3],&jobs);
        printf("{\"state\":\"valid_stream_inputs\",\"pages\":%u,\"total_records\":%u,\"global_event_first\":%u,\"jobs\":%zu}\n",s.count,s.logical.length,s.logical.event_first,n);return 0;
    }
    if(argc==7 && !strcmp(argv[1],"stream") && !strcmp(argv[5],"--expect-hash")) {
        struct stream s=read_stream(argv[2]);struct job *jobs;size_t n=read_jobs(argv[3],&jobs);
        connect_fpga(argv[6]);stream_batch(&s,jobs,n,argv[4]);return 0;
    }
    if (argc==4 && !strcmp(argv[1],"validate")) {
        struct tape t=read_page(argv[2],0); struct job *jobs; size_t n=read_jobs(argv[3],&jobs);
        printf("{\"state\":\"valid_inputs\",\"tape_records\":%u,\"jobs\":%zu}\n",t.length,n);
        free(t.words); free(jobs); return 0;
    }
    if (argc==4 && !strcmp(argv[1],"probe") && !strcmp(argv[2],"--expect-hash")) {
        connect_fpga(argv[3]);
        printf("protocol=0x965102 lanes=4 tape=%u frames=%u clock=%u source_hash=%s\n",
               tape_capacity,capture_capacity,clock_hz,source_hex);
        return 0;
    }
    if (argc==7 && !strcmp(argv[1],"batch") && !strcmp(argv[5],"--expect-hash")) {
        struct tape t=read_page(argv[2],0); struct job *jobs; size_t n=read_jobs(argv[3],&jobs);
        connect_fpga(argv[6]); batch(&t,jobs,n,argv[4]); free(t.words); free(jobs); return 0;
    }
    die("Usage: c96-jt51 validate-stream MANIFEST JOBS.tsv\n       c96-jt51 stream MANIFEST JOBS.tsv NEW_OUTPUT_DIRECTORY --expect-hash FILE\n       c96-jt51 validate TAPE JOBS.tsv\n       c96-jt51 probe --expect-hash FILE\n       c96-jt51 batch TAPE JOBS.tsv NEW_OUTPUT_DIRECTORY --expect-hash FILE");
    return 1;
}
