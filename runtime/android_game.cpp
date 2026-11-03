/* Android surface owner + the same verified lifted-code runtime as the probe.
 * This library runs only in the app's separate :game process. */
#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <exception>
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include "android_wsi.h"
#include "d3d9_bridge.h"
#include "dxvk_smoke.h"
#include "android_vulkan.h"
#include "control_profile.h"
#include "presentation_gate.h"
extern "C" {
#include "win32hle.h"
#include "pe_loader.h"
#include "probe_support.h"
#include "probe_build.h"
#include "startup.h"
#include "startup_user32.h"
#include "startup_services.h"
#include "startup_input.h"
#include "startup_audio.h"
}

namespace {
std::atomic_flag entered = ATOMIC_FLAG_INIT;
std::atomic<uint32_t> presented_frames{0};
void observe_frame(){
    presented_frames.fetch_add(1,std::memory_order_relaxed);
    tf_control_profile_sample();
}
TF_PresentationGate presentation;
uint32_t surface_width=0,surface_height=0;
void enter_graphics(){presentation.enter();}
void leave_graphics(){presentation.leave();}
uint64_t guest_time_ns(){
    // A guest frame limiter may poll its clock instead of calling Present.
    // Park those polls too, rather than spinning on a frozen timestamp.
    presentation.enter();uint64_t now=presentation.time_ns();presentation.leave();return now;
}
void session_ticks(){RET(uint32_t(guest_time_ns()/1000000u),0);}
void session_counter(){
    uint64_t us=guest_time_ns()/1000u;
    if(A32(0)){MEM32(A32(0))=uint32_t(us);MEM32(A32(0)+4)=uint32_t(us>>32);}RET(1,1);
}
uint32_t allocate_guest(uint32_t bytes) { return hle_va(hle_alloc(bytes)); }
void free_guest(uint32_t address) { hle_free(hle_ptr(address)); }

void event(JNIEnv *env,jobject activity,const char *message) {
    std::fprintf(stderr,"[android] %s\n",message);
    jclass type = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(type,"onNativeEvent","(Ljava/lang/String;)V");
    if (method) {
        jstring text = env->NewStringUTF(message);
        if (text) { env->CallVoidMethod(activity,method,text); env->DeleteLocalRef(text); }
    }
    env->DeleteLocalRef(type);
    if (env->ExceptionCheck()) env->ExceptionClear();
}

}

extern "C" JNIEXPORT void JNICALL
Java_io_github_transformers_recompiled_diagnostics_GameActivity_nativeSetForeground(JNIEnv*,jobject,jboolean active){
    presentation.set_foreground(active);
    tf_audio_set_suspended(!presentation.ready());
    std::fprintf(stderr,"[lifecycle] foreground=%d ready=%d\n",int(active),int(presentation.ready()));
}
extern "C" JNIEXPORT void JNICALL
Java_io_github_transformers_recompiled_diagnostics_GameActivity_nativeSetSurface(JNIEnv *env,jobject,jobject surface,jboolean nativeResolution){
    // Stop new graphics calls before waiting for the current operation. Flush
    // DXVK's asynchronous command/present threads before SurfaceDestroyed returns.
    presentation.set_surface(false);tf_audio_set_suspended(1);
    auto lock=presentation.exclusive();
    tf_d3d9_suspend_presenters();dxvkAndroidSetWindow(nullptr);
    if(!surface){std::fprintf(stderr,"[lifecycle] surface detached; guest session retained\n");return;}
    ANativeWindow *window=ANativeWindow_fromSurface(env,surface);
    if(!window)return;
    uint32_t width=uint32_t(ANativeWindow_getWidth(window)),height=uint32_t(ANativeWindow_getHeight(window));
    if(!width||!height||width>8192||height>8192){ANativeWindow_release(window);return;}
    if(width>1280&&!nativeResolution){
        uint32_t scaled=std::max(2u,(height*1280u/width)&~1u);
        if(!ANativeWindow_setBuffersGeometry(window,1280,scaled,0)){width=1280;height=scaled;}
    }
    dxvkAndroidSetWindow(window);dxvkAndroidSetRenderSize(width,height);ANativeWindow_release(window);
    surface_width=width;surface_height=height;
    presentation.set_surface(true);tf_audio_set_suspended(!presentation.ready());
    std::fprintf(stderr,"[lifecycle] surface attached %ux%u hwnd=%p; ready=%d\n",width,height,dxvkAndroidGetWindowHandle(),int(presentation.ready()));
}

extern "C" JNIEXPORT jint JNICALL
Java_io_github_transformers_recompiled_diagnostics_GameActivity_nativeRun(
        JNIEnv *env,jobject activity,jobject surface,jstring directory,jstring logPath,jboolean smoke,jint renderScale,jstring driverMode,jstring driverPath) {
    if (entered.test_and_set()) return 90;
    const char *dir = env->GetStringUTFChars(directory,nullptr);
    if (!dir) return 91;
    std::string game_dir(dir); env->ReleaseStringUTFChars(directory,dir);
    if(!driverMode||!driverPath)return 96;
    const char* mode=env->GetStringUTFChars(driverMode,nullptr);if(!mode)return 96;
    std::string driver_mode(mode);env->ReleaseStringUTFChars(driverMode,mode);
    const char* custom=env->GetStringUTFChars(driverPath,nullptr);if(!custom)return 96;
    std::string driver_path(custom);env->ReleaseStringUTFChars(driverPath,custom);
    const char *path = env->GetStringUTFChars(logPath,nullptr);
    if (!path) return 92;
    int log = open(path,O_WRONLY | O_CREAT | O_TRUNC,0600);
    env->ReleaseStringUTFChars(logPath,path);
    if (log < 0) return 93;
    if (dup2(log,STDOUT_FILENO)<0 || dup2(log,STDERR_FILENO)<0) { close(log); return 94; }
    close(log); setvbuf(stdout,nullptr,_IONBF,0); setvbuf(stderr,nullptr,_IONBF,0);
    if (chdir(game_dir.c_str())) { event(env,activity,"Folder game tidak dapat dibuka."); return 95; }
    setenv("DXVK_LOG_LEVEL","info",1);
    setenv("DXVK_LOG_PATH",game_dir.c_str(),1);
    (void)surface; // Surface callbacks own replaceable native windows.
    presentation.enter();uint32_t width=surface_width,height=surface_height;presentation.leave();
    tf_d3d9_set_operation_guard(enter_graphics,leave_graphics);
    std::fprintf(stderr,"[android] DXVK surface=%ux%u, input=%s, mode=%s\n",width,height,
        TF_PROBE_INPUT_SHA256,smoke ? "draw-readback-test":"lifted-game-entry");
    try {
        tf_android_configure_vulkan_driver(game_dir.c_str(),driver_mode.c_str(),driver_path.c_str());
        if (smoke) {
            event(env,activity,"Uji DXVK: menggambar segitiga dan memeriksa pixel GPU...");
            int result=tf_dxvk_smoke(width,height);
            if (result) return result;
            hle_mem_init(); mach_enter();
            tf_d3d9_configure(reinterpret_cast<void*>(g_mem_base),UINT64_C(0x100000000),
                allocate_guest,free_guest,dxvkAndroidGetWindowHandle);
            tf_d3d9_register();
            uint32_t guest_result=tf_d3d9_smoke(width,height);
            tf_d3d9_shutdown(); mach_leave();
            return guest_result & 0x80000000u ? 18 : 0;
        }
        std::string image_path=game_dir+"/Transformers.exe";
        event(env,activity,"Memeriksa EXE dan menyiapkan bridge Direct3D 9...");
        if (tf_probe_verify_image(image_path.c_str())) return 3;
        hle_mem_init(); pe_image image{};
        hle_set_drive('C',game_dir.c_str()); // Stable guest root; SAF links stay below it.
        if (recomp_pe_map(image_path.c_str(),&image)) return 4;
        if (image.entry != TF_PROBE_ENTRY || !recomp_lookup(image.entry)) return 5;
        mach_enter();
        const win32hle_shim timers[]={{"GetTickCount",session_ticks},{"timeGetTime",session_ticks},
            {"QueryPerformanceCounter",session_counter},{nullptr,nullptr}};
        win32hle_register(timers); // First registration wins in the pinned HLE.
        std::fprintf(stderr,"[android] startup APIs: %d\n",tf_startup_register(&image,image_path.c_str()));
        std::fprintf(stderr,"[android] window/COM APIs: %d\n",tf_user32_register());
        int services=tf_services_register(game_dir.c_str());
        if (services<0) { mach_leave(); return 7; }
        std::fprintf(stderr,"[android] path/registry/crypto APIs: %d\n",services);
        std::fprintf(stderr,"[android] input APIs: %d\n",tf_input_register());
        tf_user32_set_display(int(width),int(height));
        tf_d3d9_set_render_scale(static_cast<uint32_t>(renderScale));
        tf_control_profile_init();tf_d3d9_set_frame_observer(observe_frame);
        std::fprintf(stderr,"[android] internal render scale=%u%%\n",static_cast<uint32_t>(renderScale));
        tf_d3d9_configure(reinterpret_cast<void*>(g_mem_base),UINT64_C(0x100000000),
            allocate_guest,free_guest,dxvkAndroidGetWindowHandle);
        std::fprintf(stderr,"[android] D3D9/D3DX bridge entries: %d\n",tf_d3d9_register());
        int bind = recomp_pe_bind(&image,hle_resolve_or_stub);
        mach_leave();
        if (bind) return 6;
        event(env,activity,"Menjalankan startup kode game dengan DXVK. Operasi yang belum didukung dicatat di log.");
        uint32_t result = hle_call_guest(image.entry,0,nullptr);
        tf_audio_shutdown();
        mach_enter(); tf_d3d9_shutdown(); mach_leave();
        std::fprintf(stderr,"[android] lifted entry returned 0x%08X; no gameplay assertion\n",result);
        return int(result & 255u);
    } catch (const std::exception &error) {
        std::fprintf(stderr,"[android] native exception: %s\n",error.what());
        return 98;
    } catch (...) { event(env,activity,"Runtime native berhenti dengan exception yang tidak dikenal."); return 99; }
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_transformers_recompiled_diagnostics_GameActivity_nativeKey(
        JNIEnv *,jobject,jint key,jboolean down) {
    tf_user32_key_event(uint32_t(key),down);
    tf_input_key_event(uint32_t(key),down);
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_transformers_recompiled_diagnostics_GameActivity_nativeMouse(
        JNIEnv *,jobject,jint dx,jint dy,jint wheel,jint buttons) {
    tf_input_mouse_event(dx,dy,wheel,uint32_t(buttons));
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_transformers_recompiled_diagnostics_GameActivity_nativeAudioGain(
        JNIEnv *,jobject,jfloat gain) { tf_audio_focus_gain(gain); }
extern "C" JNIEXPORT jint JNICALL
Java_io_github_transformers_recompiled_diagnostics_GameActivity_nativeControlProfile(JNIEnv *,jobject){return jint(tf_control_profile_current());}
extern "C" JNIEXPORT jint JNICALL
Java_io_github_transformers_recompiled_diagnostics_GameActivity_nativePresentedFrames(JNIEnv *,jobject){
    return static_cast<jint>(presented_frames.load(std::memory_order_relaxed));
}
