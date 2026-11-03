#include "control_profile_state.h"
#include <atomic>
#include <cstdio>
#include <cstring>
extern "C" {
#include "win32hle.h"
}
namespace {
std::atomic<uint32_t> current{TF_CONTROL_HIDDEN};
const char *names[]={"HIDDEN","MENU","ROBOT","CAR"};
}
extern "C" void tf_control_profile_init(){current=TF_CONTROL_HIDDEN;}
extern "C" uint32_t tf_control_profile_current(){return current.load(std::memory_order_acquire);}
extern "C" void tf_control_profile_sample(){
    tf_control_memory_begin();auto state=tf_control_decode(tf_control_memory_read,nullptr);
    uint32_t before=current.exchange(state.profile,std::memory_order_release);
    if(before!=state.profile)std::fprintf(stderr,"[controls] profile=%s actor=%08x flags=%08x anim=%x frontend=%08x scene=%08x pause=%u/%u menu=%u mission=%u start=%u phase=%u confirm=%u confirm_phase=%u save=%u loading=%u ready=%u enabled=%u inhibit=%u\n",
        names[state.profile],state.actor,state.flags,state.animation,state.frontend,state.scene,state.paused,state.pending,state.pause_menu,state.mission_menu,state.start_menu,state.start_phase,state.confirm_kind,state.confirm_phase,state.save_menu,state.loading_active,state.loading_ready,state.loading_enabled,state.inhibit);
}
extern "C" int tf_control_profile_selftest(){
    const uint32_t image=0x400000,size=0x400000;
    if(hle_vm_reserve(image,size)!=image)return 150;
    uint32_t actor=hle_va(hle_alloc(0x1c00)),scene=hle_va(hle_alloc(32)),name=hle_guest_strdup("demoTranquility"),config=hle_va(hle_alloc(32));
    int result=0;
    auto check=[&](uint32_t expected){tf_control_memory_begin();return tf_control_decode(tf_control_memory_read,nullptr).profile==expected;};
    MEM32(0x7183a8)=scene;MEM32(scene)=name;MEM32(0x770614)=actor;MEM32(actor+0x518)=config;
    MEM32(actor+0x628)=0x40101000;MEM32(actor+0x1b24)=1;
    if(!check(TF_CONTROL_ROBOT))result=151;
    MEM32(actor+0x628)=0x400c1000;if(!check(TF_CONTROL_CAR))result=152;
    MEM8(0x713677)=1;MEM32(0x6fb0b0)=MEM32(0x6fb0b4)=1;if(!check(TF_CONTROL_MENU))result=153;
    MEM8(0x713677)=0;if(!check(TF_CONTROL_HIDDEN))result=154;
    MEM8(0x71437a)=1;if(!check(TF_CONTROL_MENU))result=188;
    MEM8(0x71437a)=0;if(!check(TF_CONTROL_HIDDEN))result=189;
    MEM32(0x6fb1d0)=0x80;MEM32(0x713464)=1;MEM32(0x713468)=1;
    if(!check(TF_CONTROL_MENU))result=192;
    MEM32(0x713468)=0;if(!check(TF_CONTROL_HIDDEN))result=193;
    MEM32(0x6fb0b0)=MEM32(0x6fb0b4)=0;
    if(!check(TF_CONTROL_HIDDEN))result=194;
    MEM32(0x713468)=1;if(!check(TF_CONTROL_MENU))result=195;
    MEM32(0x6fb1d0)=0;if(!check(TF_CONTROL_CAR))result=196;
    MEM32(0x713464)=MEM32(0x713468)=0;MEM32(0x6fb0b0)=MEM32(0x6fb0b4)=1;
    uint32_t root=hle_va(hle_alloc(0xa0)),chapter=hle_va(hle_alloc(0x40));
    MEM32(0x713648)=root;MEM32(root+0x4c)=chapter;MEM32(chapter)=0x687358;
    MEM32(chapter+0x28)=9;MEM8(chapter+0x2c)=1;
    if(!check(TF_CONTROL_MENU))result=162;
    MEM32(chapter+0x28)=8;if(!check(TF_CONTROL_HIDDEN))result=163;
    MEM32(chapter+0x28)=10;if(!check(TF_CONTROL_HIDDEN))result=164;
    MEM32(chapter+0x28)=9;MEM8(chapter+0x2c)=0;if(!check(TF_CONTROL_HIDDEN))result=165;
    // Reuse the inactive chapter allocation for each independently typed modal.
    MEM32(root+0x4c)=0;MEM32(root+0x3c)=chapter;MEM32(chapter)=0x687388;
    MEM32(chapter+0x28)=22;MEM8(chapter+0x2c)=1;if(!check(TF_CONTROL_MENU))result=166;
    MEM32(chapter+0x28)=21;if(!check(TF_CONTROL_HIDDEN))result=167;
    MEM32(chapter+0x28)=20;MEM8(0x71437a)=1;if(!check(TF_CONTROL_MENU))result=190;
    MEM8(0x71437a)=0;if(!check(TF_CONTROL_HIDDEN))result=191;
    MEM32(chapter+0x28)=22;MEM32(chapter+0x2c)=0x100;if(!check(TF_CONTROL_HIDDEN))result=168;
    MEM32(root+0x3c)=0;MEM32(root+0x78)=chapter;MEM32(chapter)=0x686fb0;
    MEM32(chapter+0x28)=12;MEM8(chapter+0x2c)=1;if(!check(TF_CONTROL_MENU))result=169;
    MEM32(chapter+0x28)=13;if(!check(TF_CONTROL_HIDDEN))result=170;
    MEM32(root+0x78)=0;MEM32(root+0x40)=chapter;MEM32(chapter)=0x6873c0;
    MEM32(chapter+0x30)=7;MEM8(chapter+0x34)=1;MEM8(chapter+0x35)=1;if(!check(TF_CONTROL_MENU))result=171;
    MEM8(chapter+0x35)=0;if(!check(TF_CONTROL_HIDDEN))result=172;
    MEM32(root+0x40)=0;
    MEM32(0x6fb0b0)=MEM32(0x6fb0b4)=0;MEM32(0x7622cc)=1;if(!check(TF_CONTROL_HIDDEN))result=155;
    MEM32(0x7622cc)=0;MEM32(actor+0x628)=0x400c1080;if(!check(TF_CONTROL_CAR))result=156;
    MEM32(actor+0x628)=0x40101040;if(!check(TF_CONTROL_ROBOT))result=160;
    MEM32(actor+0x1b24)=0x45;if(!check(TF_CONTROL_ROBOT))result=161;
    MEM32(0x71318c)=1;if(!check(TF_CONTROL_MENU))result=157;MEM32(0x71318c)=0;
    MEM32(0x770614)=0x7f000000;if(!check(TF_CONTROL_HIDDEN))result=158;
    tf_control_memory_begin();uint32_t value=0x12345678;
    if(tf_control_memory_read(nullptr,0x7f000000,&value,4)||value!=0x12345678)result=159;
    hle_free(hle_ptr(actor));hle_free(hle_ptr(scene));hle_free(hle_ptr(name));hle_free(hle_ptr(config));hle_free(hle_ptr(root));hle_free(hle_ptr(chapter));hle_vm_release(image,size);
    if(!result)std::fprintf(stderr,"[controls-test] PASS: mapped guest reads, invalid-pointer rejection, frontend/robot/car/pause/inhibit, continuous transform controls, saving menu, loading ENTER prompt and return to combat, Start Mission and completion/failure/submission confirmation phases; loading without prompt/cleanup stay hidden; no game or screen input\n");
    return result;
}
