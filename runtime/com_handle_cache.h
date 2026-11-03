#ifndef TF_COM_HANDLE_CACHE_H
#define TF_COM_HANDLE_CACHE_H
#include <stdint.h>

/* Exact-key front cache for stable std::map nodes. The bridge's existing
 * operation/machine locks serialize callers. Invalidate before node erasure;
 * collisions fall back to the authoritative registry and never alias handles. */
enum { TF_COM_CACHE_SIZE=256 };
typedef struct { uint32_t address;void *object; } TF_ComCacheSlot;
typedef struct { TF_ComCacheSlot slots[TF_COM_CACHE_SIZE]; } TF_ComCache;

static inline unsigned tf_com_cache_index(uint32_t address){
    return ((address>>4)^(address>>13))&(TF_COM_CACHE_SIZE-1);
}
static inline void *tf_com_cache_get(const TF_ComCache *cache,uint32_t address){
    const TF_ComCacheSlot *slot=&cache->slots[tf_com_cache_index(address)];
    return slot->address==address?slot->object:0;
}
static inline void tf_com_cache_put(TF_ComCache *cache,uint32_t address,void *object){
    TF_ComCacheSlot *slot=&cache->slots[tf_com_cache_index(address)];
    slot->address=address;slot->object=object;
}
static inline void tf_com_cache_erase(TF_ComCache *cache,uint32_t address){
    TF_ComCacheSlot *slot=&cache->slots[tf_com_cache_index(address)];
    if(slot->address==address){slot->address=0;slot->object=0;}
}
#endif
