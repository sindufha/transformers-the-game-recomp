/* Project extension for the pinned pcrecomp global-register x87 model.
 * Include AFTER recomp_types.h in every generated translation unit.
 * See docs/X87.md: this preserves environments, not 80-bit arithmetic. */
#ifndef TRANSFORMERS_X87_H
#define TRANSFORMERS_X87_H

typedef struct {
    uint16_t sw, tags, opcode, cs, ds;
    uint8_t top;
    uint32_t ip, dp, limitations;
} TF_X87State;

enum {
    TF_X87_LIMIT_BINARY64 = 1u,
    TF_X87_LIMIT_EXCEPTION_FLAGS = 2u,
    TF_X87_LIMIT_PRECISION_CONTROL = 4u,
    TF_X87_LIMIT_ROUNDING = 8u
};
#define TF_X87_LIMITS 15u
RECOMP_SHARED_DATA TF_X87State g_tf_x87 = { .tags = 0xffffu, .limitations = TF_X87_LIMITS };

#ifndef TF_X87_FAULT
#define TF_X87_FAULT(ip, reason) do { \
    fprintf(stderr, "unsupported x87 state at 0x%08x: %s\n", (unsigned)(ip), (reason)); \
    abort(); \
} while (0)
#endif

static inline uint16_t tf_x87_cw(uint16_t value) {
    return (uint16_t)((value & 0x1f3fu) | 0x0040u);
}
static inline int tf_x87_empty(unsigned physical) {
    return ((g_tf_x87.tags >> (2u * (physical & 7u))) & 3u) == 3u;
}
static inline void tf_x87_set_tag(unsigned physical, unsigned tag) {
    unsigned shift = 2u * (physical & 7u);
    g_tf_x87.tags = (uint16_t)((g_tf_x87.tags & ~(3u << shift)) | (tag << shift));
}
static inline unsigned tf_x87_value_tag(double value) {
    /* Every finite nonzero binary64, including a binary64 subnormal, is a
     * normal extended-real value. We cannot represent x87 subnormals. */
    return value == 0.0 ? 1u : (!isfinite(value) ? 2u : 0u);
}
static inline void tf_x87_depth(void) {
    unsigned i; g_fp_top = 0;
    for (i = 0; i < 8; ++i) if (!tf_x87_empty(i)) ++g_fp_top;
}
static inline void tf_x87_rotate(int increment);
static inline void tf_x87_init(void) {
    unsigned i;
    /* FINIT changes TOP to zero and marks registers empty, without clearing
     * their physical contents. A later FLDENV can expose those contents. */
    unsigned distance = (0u - g_tf_x87.top) & 7u;
    for (i = 0; i < distance; ++i) tf_x87_rotate(1);
    g_tf_x87 = (TF_X87State){ .tags = 0xffffu, .limitations = TF_X87_LIMITS };
    g_fpu_cw = 0x037fu; g_fp_top = 0;
}
static inline void tf_x87_wait(uint32_t ip) {
    if (g_tf_x87.sw & (uint16_t)~g_fpu_cw & 0x003fu)
        TF_X87_FAULT(ip, "pending unmasked exception requires guest exception dispatch");
}
static inline uint16_t tf_x87_status(void) {
    return (uint16_t)((g_tf_x87.sw & ~0x3800u) | ((g_tf_x87.top & 7u) << 11));
}
static inline void tf_x87_summary(void) {
    g_tf_x87.sw &= (uint16_t)~0x8080u;
    if (g_tf_x87.sw & (uint16_t)~g_fpu_cw & 0x003fu) g_tf_x87.sw |= 0x8080u;
}
static inline void tf_x87_condition(int comparison) {
    unsigned bits = comparison >= 0x10000 ? (unsigned)comparison & 0x4700u :
        comparison == 2 ? 0x4500u : comparison < 0 ? 0x0100u :
        comparison == 0 ? 0x4000u : 0u;
    g_tf_x87.sw = (uint16_t)((g_tf_x87.sw & ~0x4700u) | bits);
}
static inline void tf_x87_note(uint32_t ip, unsigned opcode, uint32_t dp, int memory, uint16_t selector) {
    g_tf_x87.ip = ip; g_tf_x87.opcode = (uint16_t)(opcode & 0x7ffu);
    g_tf_x87.cs = g_seg_cs;
    if (memory) { g_tf_x87.dp = dp; g_tf_x87.ds = selector; }
}
static inline void tf_x87_rotate(int increment) {
    unsigned i;
    if (increment) {
        double old = g_st[0]; int64_t integer = g_st_i64[0];
        for (i = 0; i < 7; ++i) { g_st[i] = g_st[i+1]; g_st_i64[i] = g_st_i64[i+1]; }
        g_st[7] = old; g_st_i64[7] = integer;
        g_tf_x87.top = (uint8_t)((g_tf_x87.top + 1u) & 7u);
    } else {
        double old = g_st[7]; int64_t integer = g_st_i64[7];
        for (i = 7; i > 0; --i) { g_st[i] = g_st[i-1]; g_st_i64[i] = g_st_i64[i-1]; }
        g_st[0] = old; g_st_i64[0] = integer;
        g_tf_x87.top = (uint8_t)((g_tf_x87.top + 7u) & 7u);
    }
}
static inline void tf_x87_require(unsigned logical, uint32_t ip) {
    if (tf_x87_empty(g_tf_x87.top + logical))
        TF_X87_FAULT(ip, "empty-register operand requires x87 stack-fault emulation");
}
static inline void tf_x87_mark(unsigned logical) {
    /* Marking makes this slot nonempty. Maintain the occupied count instead
     * of scanning all eight tags after every arithmetic result and push.
     * Environment loads and explicit whole-tag writes still call depth(). */
    if (tf_x87_empty(g_tf_x87.top + logical)) ++g_fp_top;
    tf_x87_set_tag(g_tf_x87.top + logical, tf_x87_value_tag(g_st[logical & 7u]));
}
static inline void tf_x87_push(double value) {
    if (!tf_x87_empty(g_tf_x87.top + 7u))
        TF_X87_FAULT(g_tf_x87.ip, "stack overflow requires x87 stack-fault emulation");
    tf_x87_rotate(0); g_st[0] = value; g_st_i64[0] = 0;
    tf_x87_mark(0); g_tf_x87.sw &= (uint16_t)~0x0200u;
}
static inline double tf_x87_pop(void) {
    double value;
    tf_x87_require(0, g_tf_x87.ip); value = g_st[0];
    tf_x87_set_tag(g_tf_x87.top, 3); tf_x87_rotate(1); --g_fp_top;
    return value;
}
static inline void tf_x87_free(unsigned logical) {
    if (!tf_x87_empty(g_tf_x87.top + logical)) --g_fp_top;
    tf_x87_set_tag(g_tf_x87.top + logical, 3);
}
static inline void tf_x87_examine(void) {
    double value = g_st[0]; unsigned bits;
    if (tf_x87_empty(g_tf_x87.top)) bits = 0x4100u;
    else bits = isnan(value) ? 0x0100u : isinf(value) ? 0x0500u :
        value == 0.0 ? 0x4000u : 0x0400u;
    if (signbit(value)) bits |= 0x0200u;
    g_tf_x87.sw = (uint16_t)((g_tf_x87.sw & ~0x4700u) | bits);
}
static inline uint16_t tf_x87_get16(const unsigned char *p) {
    return (uint16_t)(p[0] | (unsigned)p[1] << 8);
}
static inline uint32_t tf_x87_get32(const unsigned char *p) {
    return (uint32_t)tf_x87_get16(p) | (uint32_t)tf_x87_get16(p + 2) << 16;
}
static inline void tf_x87_put16(unsigned char *p, uint16_t value) {
    p[0] = (unsigned char)value; p[1] = (unsigned char)(value >> 8);
}
static inline void tf_x87_put32(unsigned char *p, uint32_t value) {
    tf_x87_put16(p, (uint16_t)value); tf_x87_put16(p + 2, (uint16_t)(value >> 16));
}
static inline void tf_x87_store_env(unsigned char *p) {
    unsigned i;
    /* Reserved bits match the observed modern x86 image, but callers must
     * treat reserved bits as unspecified. All 28 bytes are written. */
    for (i = 0; i < 28; ++i) p[i] = 0xff;
    for (i = 0; i < 8; ++i) if (!tf_x87_empty(g_tf_x87.top + i)) tf_x87_mark(i);
    tf_x87_put16(p, g_fpu_cw); tf_x87_put16(p + 4, tf_x87_status());
    tf_x87_put16(p + 8, g_tf_x87.tags); tf_x87_put32(p + 12, g_tf_x87.ip);
    tf_x87_put16(p + 16, g_tf_x87.cs); tf_x87_put16(p + 18, g_tf_x87.opcode & 0x7ffu);
    tf_x87_put32(p + 20, g_tf_x87.dp); tf_x87_put16(p + 24, g_tf_x87.ds);
    g_fpu_cw |= 0x003fu; tf_x87_summary();
}
static inline void tf_x87_load_env(const unsigned char *p) {
    unsigned old_top = g_tf_x87.top, new_top = (tf_x87_get16(p + 4) >> 11) & 7u;
    unsigned i, tags = tf_x87_get16(p + 8);
    /* FLDENV changes logical-to-physical association without loading values. */
    for (i = 0; i < ((new_top - old_top) & 7u); ++i) tf_x87_rotate(1);
    g_fpu_cw = tf_x87_cw(tf_x87_get16(p)); g_tf_x87.sw = tf_x87_get16(p + 4);
    g_tf_x87.ip = tf_x87_get32(p + 12); g_tf_x87.cs = tf_x87_get16(p + 16);
    g_tf_x87.opcode = tf_x87_get16(p + 18) & 0x7ffu;
    g_tf_x87.dp = tf_x87_get32(p + 20); g_tf_x87.ds = tf_x87_get16(p + 24);
    g_tf_x87.tags = (uint16_t)tags;
    for (i = 0; i < 8; ++i) if (!tf_x87_empty(new_top + i)) tf_x87_mark(i);
    tf_x87_depth(); tf_x87_summary();
}

#undef fp_push
#undef fp_pop
#define fp_push(value) tf_x87_push(value)
#define fp_pop() tf_x87_pop()
#endif
