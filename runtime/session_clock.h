#ifndef TF_SESSION_CLOCK_H
#define TF_SESSION_CLOCK_H
#include <stdint.h>
typedef struct {uint64_t excluded,paused_at;int running;} TF_SessionClock;
static inline void tf_session_clock_running(TF_SessionClock *s,uint64_t now,int running){
    if(s->running==running)return;
    if(running)s->excluded+=now-s->paused_at;else s->paused_at=now;
    s->running=running;
}
static inline uint64_t tf_session_clock_now(const TF_SessionClock *s,uint64_t now){
    return (s->running?now:s->paused_at)-s->excluded;
}
#endif
