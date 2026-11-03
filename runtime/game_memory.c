/* Keep the pinned allocator unchanged. This translation unit adds a bounded
 * read adapter in the same scope as its reservation map, under the same lock. */
#include "../external/pcrecomp/runtime/win32hle/guest_mem.c"
#include "control_profile.h"
#include <stdio.h>
#ifdef HLE_FLAT
static struct {uint64_t lo,hi;} control_maps[8192];
static unsigned control_map_count;
/* A flat 32-bit guest shares the host address space. Refresh once per snapshot
 * under the guest machine lock, not once per field or on the Android UI thread. */
void tf_control_memory_begin(void){
    control_map_count=0;FILE *f=fopen("/proc/self/maps","r");if(!f)return;
    char line[1024],permissions[5];unsigned long long lo,hi;
    while(fgets(line,sizeof(line),f))if(sscanf(line,"%llx-%llx %4s",&lo,&hi,permissions)==3&&permissions[0]=='r'){
        if(control_map_count==8192)break;
        control_maps[control_map_count].lo=lo;control_maps[control_map_count++].hi=hi;
    }fclose(f);
}
int tf_control_memory_read(void *unused,uint32_t va,void *out,uint32_t n){
    (void)unused;if(!out||!n||va<0x10000||(uint64_t)va+n>0x100000000ull)return 0;
    for(unsigned i=0;i<control_map_count;++i)if(va>=control_maps[i].lo&&(uint64_t)va+n<=control_maps[i].hi){memcpy(out,hle_ptr(va),n);return 1;}
    return 0;
}
#else
void tf_control_memory_begin(void){}
int tf_control_memory_read(void *unused,uint32_t va,void *out,uint32_t n){
    (void)unused;if(!out||!n||va<0x10000||(uint64_t)va+n>SPAN)return 0;
    uint64_t at=va,end=(uint64_t)va+n;int ok=0;pthread_mutex_lock(&g_mem_lock);
    for(int i=0;i<g_nrng&&at<end;++i){
        if(at<g_rng[i].lo)break;if(at<g_rng[i].hi)at=g_rng[i].hi;
    }
    if(at>=end){memcpy(out,host(va),n);ok=1;}
    pthread_mutex_unlock(&g_mem_lock);return ok;
}
#endif
