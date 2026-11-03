/* Target runtime additions. Include after recomp_types.h and x87 overlay. */
#ifndef TRANSFORMERS_RUNTIME_H
#define TRANSFORMERS_RUNTIME_H

#include <stdint.h>

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noreturn))
#endif
void transformers_guest_interrupt(uint32_t vector, uint32_t instruction_va,
                                 uint32_t next_va);

/* A guest CPU identity, independent of the Android host architecture. This
 * reports only the instruction families handled by the global-register lifter.
 * It does not promise hardware floating-point precision or Windows SEH support.
 * No XSAVE/AVX/extended leaves are exposed. */
static inline void transformers_cpuid(uint32_t leaf, uint32_t subleaf) {
    (void)subleaf;
    g_eax = g_ebx = g_ecx = g_edx = 0;
    if (leaf == 0) {
        g_eax = 1;
        g_ebx = 0x756e6547u; /* GenuineIntel */
        g_edx = 0x49656e69u;
        g_ecx = 0x6c65746eu;
    } else if (leaf == 1) {
        g_eax = 0x000006f2u;
        g_ebx = 0x00010000u; /* one logical processor */
        /* FPU, TSC, CMOV, MMX, SSE and SSE2. FXSR is not advertised. */
        g_edx = (1u | (1u << 4) | (1u << 15) | (1u << 23) |
                 (1u << 25) | (1u << 26)) & g_cpuid_edx1;
    } else if (leaf == 0x80000000u) {
        g_eax = 0x80000000u;
    }
}

#undef CPUID
#define CPUID(a, b, c, d) do { \
    uint32_t _tf_leaf = (uint32_t)(a), _tf_subleaf = (uint32_t)(c); \
    RECOMP_REGS_OUT(); \
    transformers_cpuid(_tf_leaf, _tf_subleaf); \
    RECOMP_REGS_IN(); \
} while (0)

#endif
