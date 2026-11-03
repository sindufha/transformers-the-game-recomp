#include "session_lifecycle.h"
#include "presentation_gate.h"
#include <atomic>
#include <cstdio>
#include <future>
#include <thread>

extern "C" int tf_session_lifecycle_selftest(){
    TF_SessionClock clock{0,1000,0};
    if(tf_session_clock_now(&clock,5000)!=1000)return 180;
    tf_session_clock_running(&clock,6000,1);
    if(tf_session_clock_now(&clock,7000)!=2000)return 181;
    tf_session_clock_running(&clock,8000,0);
    tf_session_clock_running(&clock,12000,0); // duplicate pause must not reset it
    if(tf_session_clock_now(&clock,30000)!=3000)return 182;
    tf_session_clock_running(&clock,30000,1);
    tf_session_clock_running(&clock,31000,1);
    if(tf_session_clock_now(&clock,32000)!=5000)return 183;
    using namespace std::chrono_literals;
    TF_PresentationGate gate;gate.set_foreground(true);
    for(int cycle=0;cycle<6;++cycle){
        gate.set_surface(false);
        std::promise<void> reached;auto done=reached.get_future();
        std::thread render([&]{gate.enter();reached.set_value();gate.leave();});
        bool blocked=done.wait_for(20ms)==std::future_status::timeout;
        gate.set_foreground(false);gate.set_surface(true);
        blocked=blocked&&done.wait_for(20ms)==std::future_status::timeout;
        gate.set_foreground(true);
        bool resumed=done.wait_for(2s)==std::future_status::ready;
        render.join();if(!blocked||!resumed)return 184;
    }
    // Surface release must wait until the active native graphics operation ends.
    std::promise<void> active,release,detached;auto started=active.get_future();
    auto released=release.get_future();auto idle=detached.get_future();
    std::thread render([&]{gate.enter();active.set_value();released.wait();gate.leave();});
    started.wait();gate.set_surface(false);
    std::thread detach([&]{auto lock=gate.exclusive();detached.set_value();});
    bool blocked=idle.wait_for(20ms)==std::future_status::timeout;
    release.set_value();bool complete=idle.wait_for(2s)==std::future_status::ready;
    render.join();detach.join();if(!blocked||!complete)return 185;
    uint64_t paused=gate.time_ns();std::this_thread::sleep_for(20ms);
    if(gate.time_ns()!=paused)return 186;
    gate.set_surface(true);
    if(!gate.ready()||gate.time_ns()<paused)return 187;
    // If focus changes inside a nested native call, the current operation must
    // be allowed to unwind; waiting while its outer lock is held deadlocks detach.
    gate.enter();gate.set_foreground(false);gate.enter();gate.leave();gate.leave();
    gate.set_foreground(true);
    std::fprintf(stderr,"[lifecycle-test] PASS: foreground/surface gates, six detach/resume cycles, in-flight serialization, nested focus-loss unwind, frozen guest clock, duplicate pause/resume; no game, screen or input\n");
    return 0;
}
