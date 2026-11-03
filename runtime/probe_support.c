#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "win32hle.h"
#include "probe_support.h"
#include "probe_build.h"

uint32_t g_es_base;

void tf_probe_state_save(tf_probe_state *r) {
    memset(r, 0, sizeof *r);
    r->eax=g_eax; r->ecx=g_ecx; r->edx=g_edx; r->esp=g_esp;
    r->ebx=g_ebx; r->esi=g_esi; r->edi=g_edi; r->ebp=g_ebp;
    r->fs=g_fs_base; r->gs=g_gs_base; r->es_base=g_es_base; r->cur=g_cur_func;
    r->fpu_cw=g_fpu_cw; r->fp_top=g_fp_top; r->mxcsr=g_mxcsr;
    r->seg[0]=g_seg_cs; r->seg[1]=g_seg_ds; r->seg[2]=g_seg_es;
    r->seg[3]=g_seg_fs; r->seg[4]=g_seg_gs; r->seg[5]=g_seg_ss;
    memcpy(r->st,g_st,sizeof r->st); memcpy(r->st_i64,g_st_i64,sizeof r->st_i64);
    memcpy(r->mm,g_mm,sizeof r->mm); memcpy(r->xmm,g_xmm,sizeof r->xmm);
    r->flag_k=g_flag_k; r->flag_a=g_flag_a; r->flag_b=g_flag_b; r->flag_cf=g_flag_cf;
    r->cur_import=g_cur_import; r->x87=g_tf_x87;
}
void tf_probe_state_load(const tf_probe_state *r) {
    g_eax=r->eax; g_ecx=r->ecx; g_edx=r->edx; g_esp=r->esp;
    g_ebx=r->ebx; g_esi=r->esi; g_edi=r->edi; g_ebp=r->ebp;
    g_fs_base=r->fs; g_gs_base=r->gs; g_es_base=r->es_base; g_cur_func=r->cur;
    g_fpu_cw=r->fpu_cw; g_fp_top=r->fp_top; g_mxcsr=r->mxcsr;
    g_seg_cs=r->seg[0]; g_seg_ds=r->seg[1]; g_seg_es=r->seg[2];
    g_seg_fs=r->seg[3]; g_seg_gs=r->seg[4]; g_seg_ss=r->seg[5];
    memcpy(g_st,r->st,sizeof r->st); memcpy(g_st_i64,r->st_i64,sizeof r->st_i64);
    memcpy(g_mm,r->mm,sizeof r->mm); memcpy(g_xmm,r->xmm,sizeof r->xmm);
    g_flag_k=r->flag_k; g_flag_a=r->flag_a; g_flag_b=r->flag_b; g_flag_cf=r->flag_cf;
    g_cur_import=r->cur_import; g_tf_x87=r->x87;
}
void tf_probe_state_init(tf_probe_state *r) {
    memset(r,0,sizeof *r);
    r->fpu_cw=0x037f; r->mxcsr=0x1f80; r->x87.tags=0xffff;
    r->x87.limitations=TF_X87_LIMITS;
}

void tf_probe_missing_dispatch(const char *kind, uint32_t target, uint32_t caller) {
    hle_fatal("unsupported %s target 0x%08X from guest 0x%08X",kind,target,caller);
    abort();
}
void tf_probe_unimplemented(const char *name) {
    hle_fatal("unsupported operation %s from guest 0x%08X",name,g_cur_func);
    abort();
}
void transformers_guest_interrupt(uint32_t vector,uint32_t instruction_va,uint32_t next_va) {
    hle_fatal("guest INT %u at 0x%08X (next 0x%08X): protected-mode IDT/CPL exception mapping unavailable",
        vector,instruction_va,next_va);
    abort();
}
void recomp_eh_push(recomp_ehframe_t *frame) {
    (void)frame; tf_probe_unimplemented("SEH/C++ exception frame installation");
}
void recomp_eh_pop(recomp_ehframe_t *frame) {
    (void)frame; tf_probe_unimplemented("SEH/C++ exception frame removal");
}
void recomp_dump_trace(const char *why) {
    fprintf(stderr,"[probe] %s: guest=0x%08X, indirect_calls=%u\n",why,g_cur_func,g_icall_count);
}

/* Small streaming SHA-256: no OpenSSL/Android crypto dependency. It protects
 * the mapping boundary; provenance is independently checked by Python too. */
typedef struct { uint32_t h[8]; uint64_t bytes; unsigned used; unsigned char block[64]; } sha256;
static uint32_t rr(uint32_t x,unsigned n) { return (x>>n)|(x<<(32-n)); }
static void sha_block(sha256 *s,const unsigned char *p) {
    static const uint32_t k[64]={
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    uint32_t w[64],a=s->h[0],b=s->h[1],c=s->h[2],d=s->h[3],e=s->h[4],f=s->h[5],g=s->h[6],h=s->h[7];
    for(int i=0;i<16;i++) w[i]=(uint32_t)p[4*i]<<24|(uint32_t)p[4*i+1]<<16|(uint32_t)p[4*i+2]<<8|p[4*i+3];
    for(int i=16;i<64;i++) w[i]=w[i-16]+(rr(w[i-15],7)^rr(w[i-15],18)^(w[i-15]>>3))+w[i-7]+(rr(w[i-2],17)^rr(w[i-2],19)^(w[i-2]>>10));
    for(int i=0;i<64;i++) {
        uint32_t t1=h+(rr(e,6)^rr(e,11)^rr(e,25))+((e&f)^(~e&g))+k[i]+w[i];
        uint32_t t2=(rr(a,2)^rr(a,13)^rr(a,22))+((a&b)^(a&c)^(b&c));
        h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
}
static void sha_init(sha256 *s) {
    const uint32_t iv[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    memset(s,0,sizeof *s); memcpy(s->h,iv,sizeof iv);
}
static void sha_update(sha256 *s,const unsigned char *p,size_t n) {
    s->bytes+=n;
    while(n--) { s->block[s->used++]=*p++; if(s->used==64) { sha_block(s,s->block);s->used=0; } }
}
static void sha_hex(sha256 *s,char out[65]) {
    uint64_t bits=s->bytes*8; unsigned char one=128,zero=0;
    sha_update(s,&one,1);
    while(s->used!=56) sha_update(s,&zero,1);
    unsigned char tail[8]; for(int i=0;i<8;i++) tail[7-i]=(unsigned char)(bits>>(8*i));
    sha_update(s,tail,8);
    for(int i=0;i<8;i++) snprintf(out+8*i,9,"%08x",s->h[i]);
}
int tf_probe_verify_image(const char *path) {
    FILE *f=fopen(path,"rb");
    if(!f) { fprintf(stderr,"[probe] cannot open image\n");return 1; }
    sha256 s;sha_init(&s);unsigned char block[16384];size_t n;uint64_t total=0;
    while((n=fread(block,1,sizeof block,f))) { total+=n;sha_update(&s,block,n); }
    int bad=ferror(f);fclose(f);char digest[65];sha_hex(&s,digest);
    if(bad || total!=TF_PROBE_INPUT_SIZE || strcmp(digest,TF_PROBE_INPUT_SHA256)) {
        fprintf(stderr,"[probe] image identity mismatch: got %s (%llu bytes)\n",digest,(unsigned long long)total);return 1;
    }
    return 0;
}

static void selftest_callback(void) { RET(A32(0)+A32(1),2); }
static void *selftest_thread(void *unused) {
    (void)unused;mach_enter();
    /* Deliberately overwrite every saved field. No lifted instruction uses
     * this synthetic state; only the machine save/restore path touches it. */
    tf_probe_state r;memset(&r,0xa5,sizeof r);r.cur_import="__probe_thread";
    tf_probe_state_load(&r);mach_leave();return NULL;
}
int tf_probe_selftest(void) {
    sha256 sha;char digest[65];sha_init(&sha);sha_update(&sha,(const unsigned char*)"abc",3);sha_hex(&sha,digest);
    if(strcmp(digest,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")) return 10;
    hle_mem_init();void *p=hle_alloc(64);if(!p || !hle_va(p) || hle_ptr(hle_va(p))!=p) return 11;
    for(int i=0;i<64;i++) if(((unsigned char*)p)[i]) return 12;
    mach_enter();tf_probe_state saved,actual;tf_probe_state_save(&saved);
    uint32_t args[2]={19,23};
    const win32hle_shim shims[]={{"__probe_add",selftest_callback},{NULL,NULL}};
    win32hle_register(shims);
    if(hle_call_guest(hle_resolve("__probe_add"),2,args)!=42) return 13;
    tf_probe_state_save(&actual);if(memcmp(&saved,&actual,sizeof saved)) return 14;
    mach_leave();pthread_t th;
    if(pthread_create(&th,NULL,selftest_thread,NULL) || pthread_join(th,NULL)) return 15;
    mach_enter();tf_probe_state_save(&actual);
    if(memcmp(&saved,&actual,sizeof saved)) return 16;
    mach_leave();hle_free(p);
    puts("[probe] self-test passed: SHA-256, guest memory, callback stack, complete thread state; no game code executed");
    return 0;
}
