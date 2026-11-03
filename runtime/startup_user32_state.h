#ifndef TRANSFORMERS_STARTUP_USER32_STATE_H
#define TRANSFORMERS_STARTUP_USER32_STATE_H
#include <stdint.h>

/* Serialized guest-side state. No Android setting or host window is changed. */
enum { TF_USER32_QUEUE_CAPACITY = 256 };
typedef struct {
    uint32_t hwnd, message, wparam, lparam, time;
    int32_t x, y;
} TF_User32Message;
typedef struct { TF_User32Message items[TF_USER32_QUEUE_CAPACITY]; unsigned count; } TF_User32Queue;
typedef struct { uint32_t count, model; } TF_ComApartment;

static inline uint32_t tf_com_initialize(TF_ComApartment *s,uint32_t reserved,uint32_t flags) {
    if (reserved || (flags & ~14u)) return 0x80070057u; /* E_INVALIDARG */
    uint32_t model=flags & 2u;
    if (s->count && s->model!=model) return 0x80010106u; /* RPC_E_CHANGED_MODE */
    if (s->count==0xffffffffu) return 0x8007000eu;
    uint32_t result=s->count ? 1u : 0u;
    s->model=model; ++s->count; return result;
}
static inline void tf_com_uninitialize(TF_ComApartment *s) {
    if(s->count && !--s->count)s->model=0;
}
static inline int tf_user32_queue_add(TF_User32Queue *s,TF_User32Message m) {
    if(s->count==TF_USER32_QUEUE_CAPACITY)return 0;
    s->items[s->count++]=m;return 1;
}
static inline int tf_user32_queue_peek(TF_User32Queue *s,uint32_t hwnd,uint32_t low,
                                     uint32_t high,int remove,TF_User32Message *out) {
    for(unsigned i=0;i<s->count;++i) {
        TF_User32Message m=s->items[i];
        /* WM_QUIT bypasses hwnd/message filters, as PeekMessage requires. */
        if(m.message!=0x12u && ((hwnd==0xffffffffu ? m.hwnd!=0 : (hwnd && hwnd!=m.hwnd)) || ((low||high)&&(m.message<low||m.message>high))))continue;
        *out=m;
        if(remove){for(unsigned j=i+1;j<s->count;++j)s->items[j-1]=s->items[j];--s->count;}
        return 1;
    }
    return 0;
}
#endif
