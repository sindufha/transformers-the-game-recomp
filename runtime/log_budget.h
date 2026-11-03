#ifndef TF_LOG_BUDGET_H
#define TF_LOG_BUDGET_H
#include <stdint.h>
typedef struct { uint32_t window, emitted, suppressed; } TF_LogBudget;
static inline int tf_log_admit(TF_LogBudget *budget, uint32_t now,
                              uint32_t limit, uint32_t *previous_suppressed) {
    *previous_suppressed=0;
    if ((uint32_t)(now-budget->window)>=1000u) {
        *previous_suppressed=budget->suppressed;
        budget->window=now;budget->emitted=0;budget->suppressed=0;
    }
    if (budget->emitted<limit) { ++budget->emitted;return 1; }
    if (budget->suppressed!=UINT32_MAX) ++budget->suppressed;
    return 0;
}
#endif
