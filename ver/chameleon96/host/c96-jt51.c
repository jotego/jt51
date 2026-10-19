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

enum { INFO, CAPS, LANE, MASK, TADDR, TLOW, TMID, THIGH, TRLOW, TRMID,
       TRHIGH, COMMIT, START, STATUS, COUNT, ABORT, CFGSEL, CFGWRITE,
       CFGREAD, SADDR, SLOW, SHIGH, META, SOURCE };
static volatile uint32_t *regs;
static uint32_t toggle, clock_hz;
static unsigned tape_capacity, capture_capacity;
static unsigned char source_bytes[32];
static char source_hex[65];
struct tape { uint32_t length, event_first; uint64_t capture, end, key; uint64_t *words; };
struct job { char name[80]; uint32_t id, shift, frames, stride, mode; uint64_t idle; };

static void die(const char *s) { fprintf(stderr,"%s\n",s); exit(1); }
static double now(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC,&t)) die("clock_gettime failed");
    return t.tv_sec+t.tv_nsec*1e-9;
}
static uint64_t number(const char *s,uint64_t maximum) {
    char *end; errno=0;
    unsigned long long n=strtoull(s,&end,0);
    if (errno || !*s || *s=='-' || *end || n>maximum) die("Invalid numeric argument");
    return (uint64_t)n;
}
static void sleep_ms(unsigned ms) {
    struct timespec t={ms/1000,(long)(ms%1000)*1000000};
    while (nanosleep(&t,&t) && errno==EINTR) {}
}
static uint32_t command(unsigned op,uint32_t arg) {
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
}
static uint64_t meta48(unsigned lo) {
    uint64_t a=command(META,lo),b=command(META,lo+1);
    return a|(b<<24);
}
static void connect_fpga(const char *expected_path) {
    char model[64]={0},state[32]={0},expected[66]={0};
    FILE *f=fopen("/proc/device-tree/model","rb");
    if (!f || !fgets(model,sizeof(model),f)) die("Cannot identify board");
    fclose(f);
    if (strcmp(model,"NovTech CV96")) die("This tool requires NovTech CV96");
    f=fopen("/sys/class/fpga_manager/fpga0/state","r");
    if (!f || !fgets(state,sizeof(state),f)) die("Cannot read FPGA state");
    fclose(f);
    if (strcmp(state,"operating\n")) die("FPGA is not operating");
    f=fopen(expected_path,"r");
    if (!f || !fgets(expected,sizeof(expected),f)) die("Cannot read expected source hash");
    if (strlen(expected)==65 && expected[64]=='\n') expected[64]=0;
    if (strlen(expected)!=64 || fgetc(f)!=EOF) die("Expected hash must contain64 hex digits");
    fclose(f);
    for (unsigned i=0;i<64;i++) {
        if (!isxdigit((unsigned char)expected[i])) die("Invalid expected hash");
        expected[i]=(char)tolower((unsigned char)expected[i]);
    }
    int lock=open("/var/lock/c96-capture.lock",O_CREAT|O_RDWR,0600);
    if (lock<0 || flock(lock,LOCK_EX|LOCK_NB)) die("Another FPGA command or load is running");
    int fd=open("/dev/mem",O_RDWR|O_SYNC);
    if (fd<0) die("Cannot open /dev/mem");
    regs=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0xff706000);
    if (regs==MAP_FAILED) die("Cannot map FPGA manager");
    close(fd); toggle=regs[5]&0x80000000u;
    if (command(INFO,0)!=0x965101) die("Unexpected JT51 FPGA protocol");
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
static struct tape read_tape(const char *path) {
    struct tape t={0}; unsigned char h[64],w[8];
    FILE *f=fopen(path,"rb");
    if (!f || fread(h,1,64,f)!=64) die("Cannot read tape header");
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
        uint64_t v=get64(w); unsigned op=(unsigned)((v>>12)&15);
        if ((v&1) || op>4 || (i && (v>>16)<=(t.words[i-1]>>16))) die("Malformed tape action/order");
        if ((op==3 || op==4) && (v&4095)) die("Malformed tape control");
        if (op==3) {
            if ((v>>16)!=t.capture || i>=t.event_first) die("Invalid capture action");
            capture++;
        }
        if (op==4 && i+1!=t.length) die("Early tape END");
        t.words[i]=v;
    }
    if (capture!=1 || ((t.words[t.length-1]>>12)&15)!=4 ||
        (t.words[t.length-1]>>16)!=t.end || fgetc(f)!=EOF) die("Invalid tape termination/length");
    fclose(f); return t;
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
static void upload(const struct tape *t) {
    for (unsigned lane=0;lane<4;lane++) {
        command(LANE,lane);
        if (command(STATUS,0)&2) die("FPGA lane already running");
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
}
static void configure_job(unsigned lane,const struct job *j,const struct tape *t) {
    command(LANE,lane); command(MASK,1u<<lane);
    cfg(2,(uint32_t)(j->idle&0xffffff)); cfg(3,(uint32_t)(j->idle>>24));
    cfg(4,j->shift); cfg(5,j->frames); cfg(6,j->mode); cfg(7,j->id); cfg(8,j->stride);
    uint32_t expected[]={t->length,t->event_first,(uint32_t)(j->idle&0xffffff),
        (uint32_t)(j->idle>>24),j->shift,j->frames,j->mode,j->id,j->stride};
    for (unsigned k=0;k<9;k++) if (command(CFGREAD,k)!=expected[k]) die("FPGA configuration readback differs");
    command(COMMIT,0);
}
static void dump(unsigned lane,const struct job *j,const struct tape *t,const char *out) {
    command(LANE,lane);
    uint32_t status=command(STATUS,0),count=command(COUNT,0),error=command(META,1);
    uint32_t executed=command(META,0),id=command(META,16),generation=command(META,17);
    uint64_t first=meta48(4),last=meta48(6),logical=meta48(2),failure=meta48(8);
    if ((status&2) || count>capture_capacity || count>j->frames || id!=j->id) die("Invalid terminal capture identity");
    unsigned char h[160]={0}; memcpy(h,"JT51FP01",8);
    uint32_t fields[]={160,1,0x965101,clock_hz,id,lane,count,j->frames,j->stride,j->mode,
        status,error,executed,t->length};
    for (unsigned i=0;i<14;i++) put32(h+8+4*i,fields[i]);
    put64(h+64,first); put64(h+72,last); put64(h+80,logical); put64(h+88,failure);
    put32(h+96,generation); put32(h+100,j->shift); put64(h+104,j->idle);
    memcpy(h+112,source_bytes,32); put32(h+144,t->event_first); put32(h+148,5);
    char path[1024];
    if (snprintf(path,sizeof(path),"%s/%s.bin",out,j->name)>=(int)sizeof(path)) die("Output path too long");
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);
    if (fd<0) die("Cannot exclusively create capture");
    FILE *f=fdopen(fd,"wb");
    if (!f || fwrite(h,1,160,f)!=160) die("Cannot write capture header");
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
    double upload_start=now(); upload(t); double start=now();
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
            configure_job(lane,j,t);
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
        for (unsigned lane=0;lane<lanes;lane++) dump(lane,&jobs[offset+lane],t,out);
        export_seconds+=now()-export_start;
    }
    printf("{\"state\":\"all_jobs_completed\",\"jobs\":%zu,\"seconds\":%.6f,\"source_hash\":\"%s\",\"upload_seconds\":%.6f,\"execution_seconds\":%.6f,\"export_seconds\":%.6f}\n",n,now()-start,source_hex,upload_seconds,execution_seconds,export_seconds);
}
int main(int argc,char **argv) {
    if (argc==4 && !strcmp(argv[1],"validate")) {
        struct tape t=read_tape(argv[2]); struct job *jobs; size_t n=read_jobs(argv[3],&jobs);
        printf("{\"state\":\"valid_inputs\",\"tape_records\":%u,\"jobs\":%zu}\n",t.length,n);
        free(t.words); free(jobs); return 0;
    }
    if (argc==4 && !strcmp(argv[1],"probe") && !strcmp(argv[2],"--expect-hash")) {
        connect_fpga(argv[3]);
        printf("protocol=0x965101 lanes=4 tape=%u frames=%u clock=%u source_hash=%s\n",
               tape_capacity,capture_capacity,clock_hz,source_hex);
        return 0;
    }
    if (argc==7 && !strcmp(argv[1],"batch") && !strcmp(argv[5],"--expect-hash")) {
        struct tape t=read_tape(argv[2]); struct job *jobs; size_t n=read_jobs(argv[3],&jobs);
        connect_fpga(argv[6]); batch(&t,jobs,n,argv[4]); free(t.words); free(jobs); return 0;
    }
    die("Usage: c96-jt51 validate TAPE JOBS.tsv\n       c96-jt51 probe --expect-hash FILE\n       c96-jt51 batch TAPE JOBS.tsv NEW_OUTPUT_DIRECTORY --expect-hash FILE");
    return 1;
}
