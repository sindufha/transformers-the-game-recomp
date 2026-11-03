#ifndef TF_FRAME_STATS_H
#define TF_FRAME_STATS_H
#include <stdint.h>
typedef struct {
    uint64_t wall, cpu, frames, present, calls, draws;
    int initialized;
} TF_FrameStats;
typedef struct { uint64_t wall, cpu, frames, present, calls, draws; } TF_FrameReport;
/* Called after a successful Present. No GPU queries, synchronization or frame
 * limiter. CPU is the presenting thread's accumulated CPU time, not GPU time. */
static inline int tf_frame_stats_update(TF_FrameStats *s, uint64_t wall,
        uint64_t cpu, uint64_t present, TF_FrameReport *report) {
    if (!s->initialized || wall<s->wall || cpu<s->cpu) {
        s->wall=wall;s->cpu=cpu;s->frames=s->present=s->calls=s->draws=0;
        s->initialized=1;return 0;
    }
    ++s->frames;s->present+=present;
    if (wall-s->wall<UINT64_C(2000000000)) return 0;
    report->wall=wall-s->wall;report->cpu=cpu-s->cpu;
    report->frames=s->frames;report->present=s->present;
    report->calls=s->calls;report->draws=s->draws;
    s->wall=wall;s->cpu=cpu;s->frames=s->present=s->calls=s->draws=0;
    return 1;
}
#endif
