/* Exact-VA cache for the immutable generated dispatch table. Callers hold the
 * guest machine lock, just as for the shared register and pending-shim state. */
#ifndef TF_DISPATCH_CACHE_H
#define TF_DISPATCH_CACHE_H

enum { TF_DISPATCH_CACHE_SIZE = 1024 };
typedef struct {
    uint32_t va;
    recomp_func_t function;
} TF_DispatchSlot;
typedef struct { TF_DispatchSlot slots[TF_DISPATCH_CACHE_SIZE]; } TF_DispatchCache;

static inline recomp_func_t tf_dispatch_lookup(uint32_t va,
        const recomp_dispatch_entry_t *table, uint32_t count, TF_DispatchCache *cache) {
    /* Imports use a separate high VA range. Avoid a full failed binary search
     * before every Win32/D3D9 call, while still admitting every table entry. */
    if (!count || va < table[0].address || va > table[count-1].address) return NULL;
    uint32_t index = ((va >> 4) ^ (va >> 14)) & (TF_DISPATCH_CACHE_SIZE-1);
    TF_DispatchSlot *slot = &cache->slots[index];
    if (slot->function && slot->va == va) return slot->function;
    uint32_t lo = 0, hi = count;
    while (lo < hi) {
        uint32_t mid = lo + (hi-lo)/2, current = table[mid].address;
        if (current == va) {
            slot->va = va;
            slot->function = table[mid].func;
            return slot->function;
        }
        if (current < va) lo = mid+1;
        else hi = mid;
    }
    return NULL;
}
#endif
