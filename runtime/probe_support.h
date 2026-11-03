#ifndef TRANSFORMERS_PROBE_SUPPORT_H
#define TRANSFORMERS_PROBE_SUPPORT_H
#include "recomp_types.h"
#include "transformers_x87.h"

/* Everything which belongs to one simulated CPU/thread, including lazy flags.
 * Trace rings, dispatch and g_mem_base deliberately belong to the machine. */
typedef struct tf_probe_state {
    uint32_t eax, ecx, edx, esp, ebx, esi, edi, ebp, fs, gs, es_base, cur;
    uint16_t fpu_cw, seg[6];
    int fp_top;
    double st[8];
    int64_t st_i64[8];
    uint64_t mm[8];
    xmm_t xmm[8];
    uint32_t mxcsr, flag_k, flag_a, flag_b, flag_cf;
    const char *cur_import;
    TF_X87State x87;
} tf_probe_state;

void tf_probe_state_save(tf_probe_state *r);
void tf_probe_state_load(const tf_probe_state *r);
void tf_probe_state_init(tf_probe_state *r);
int tf_probe_selftest(void);
int tf_probe_verify_image(const char *path);
__attribute__((noreturn)) void tf_probe_missing_dispatch(const char *kind,
    uint32_t target, uint32_t caller);
__attribute__((noreturn)) void tf_probe_unimplemented(const char *name);
__attribute__((noreturn)) void transformers_guest_interrupt(uint32_t vector,
    uint32_t instruction_va, uint32_t next_va);
#endif
