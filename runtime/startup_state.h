#ifndef TRANSFORMERS_STARTUP_STATE_H
#define TRANSFORMERS_STARTUP_STATE_H
#include <stdint.h>
typedef struct { uint32_t standard[3], filter; } TF_StartupState;
#define TF_STARTUP_STATE_INITIALIZER {{0x11u,0x12u,0x13u},0u}
static inline int tf_standard_index(uint32_t id) {
    if (id==0xfffffff6u) return 0;
    if (id==0xfffffff5u) return 1;
    if (id==0xfffffff4u) return 2;
    return -1;
}
static inline uint32_t tf_standard_get(const TF_StartupState *state,uint32_t id) {
    int index=tf_standard_index(id); return index<0 ? 0xffffffffu : state->standard[index];
}
static inline int tf_standard_set(TF_StartupState *state,uint32_t id,uint32_t value) {
    int index=tf_standard_index(id); if(index<0)return 0;state->standard[index]=value;return 1;
}
static inline uint32_t tf_filter_set(TF_StartupState *state,uint32_t value) {
    uint32_t previous=state->filter;state->filter=value;return previous;
}
#endif
