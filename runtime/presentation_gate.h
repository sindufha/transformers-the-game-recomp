#ifndef TF_PRESENTATION_GATE_H
#define TF_PRESENTATION_GATE_H
#include <chrono>
#include <condition_variable>
#include <mutex>
#include "session_clock.h"

/* Serializes native graphics entry with Android surface replacement. Waiting
 * render threads release the operation lock, so detach/attach cannot deadlock
 * behind a paused caller. No lifecycle callback enters the guest machine. */
class TF_PresentationGate {
    std::recursive_mutex operations;
    unsigned depth=0; // protected by operations; supports nested host calls
    std::mutex state;
    std::condition_variable changed;
    bool foreground=false,surface=false;
    TF_SessionClock clock{};
    static uint64_t now(){return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());}
    void update(){tf_session_clock_running(&clock,now(),foreground&&surface);changed.notify_all();}
public:
    TF_PresentationGate(){clock.paused_at=now();}
    void set_foreground(bool value){std::lock_guard<std::mutex> lock(state);foreground=value;update();}
    void set_surface(bool value){std::lock_guard<std::mutex> lock(state);surface=value;update();}
    bool ready(){std::lock_guard<std::mutex> lock(state);return foreground&&surface;}
    uint64_t time_ns(){std::lock_guard<std::mutex> lock(state);return tf_session_clock_now(&clock,now());}
    void enter(){
        for(;;){
            operations.lock();
            if(depth){++depth;return;}
            {std::lock_guard<std::mutex> lock(state);if(foreground&&surface){depth=1;return;}}
            operations.unlock();
            std::unique_lock<std::mutex> lock(state);
            changed.wait(lock,[this]{return foreground&&surface;});
        }
    }
    void leave(){--depth;operations.unlock();}
    std::unique_lock<std::recursive_mutex> exclusive(){return std::unique_lock<std::recursive_mutex>(operations);}
};
#endif
