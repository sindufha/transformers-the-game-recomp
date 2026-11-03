/* A diagnostic CLI, not an Android Activity or a playable game. */
#include <stdio.h>
#include <string.h>
#include "win32hle.h"
#include "pe_loader.h"
#include "probe_support.h"
#include "probe_build.h"
#if TF_PROBE_STARTUP
#include "startup.h"
#endif
#if TF_PROBE_PLATFORM
#include "startup_user32.h"
#include "startup_services.h"
#include "startup_input.h"
#endif
#ifdef TF_ANDROID_AUDIO
extern int tf_source_files_selftest(void);
extern int tf_android_driver_probe(const char*,const char*);
#include "startup_audio.h"
#include "d3d9_bridge.h"
#include "control_profile.h"
#include "session_lifecycle.h"
static uint32_t image_allocate(uint32_t bytes){return hle_va(hle_alloc(bytes));}
static void image_release(uint32_t address){hle_free(hle_ptr(address));}
#endif

int main(int argc, char **argv) {
#ifdef TF_ANDROID_AUDIO
    if(argc==2&&!strcmp(argv[1],"--source-files-self-test"))return tf_source_files_selftest();
    if((argc==3||argc==4)&&!strcmp(argv[1],"--gpu-driver-test"))return tf_android_driver_probe(argv[2],argc==4?argv[3]:"");
    if(argc==2&&!strcmp(argv[1],"--lifecycle-self-test"))return tf_session_lifecycle_selftest();
    if(argc==2&&!strcmp(argv[1],"--control-profile-self-test")){
        hle_mem_init();mach_enter();int result=tf_control_profile_selftest();mach_leave();return result;
    }
    if(argc==2&&!strcmp(argv[1],"--d3dx-image-self-test")){
        hle_mem_init();mach_enter();
        tf_d3d9_configure((void*)g_mem_base,UINT64_C(0x100000000),image_allocate,image_release,NULL);
        tf_d3d9_register();int result=tf_d3d9_image_selftest();tf_d3d9_shutdown();mach_leave();return result;
    }
    if(argc==2&&!strcmp(argv[1],"--audio-self-test")){
        hle_mem_init();tf_startup_register(NULL,NULL);return tf_audio_selftest(1);
    }
#endif
    if (argc == 2 && !strcmp(argv[1], "--self-test")) {
        int result=tf_probe_selftest();
#if TF_PROBE_STARTUP
        if (!result) result=tf_startup_selftest();
#endif
#if TF_PROBE_PLATFORM
        tf_user32_register();
        if (tf_services_register(NULL)<0) return 70;
        if (!result) result=tf_user32_selftest();
        if (!result) result=tf_services_selftest();
        tf_input_register();
        if (!result) result=tf_input_selftest();
#endif
        return result;
    }
    if (argc == 2 && !strcmp(argv[1], "--provenance")) {
        printf("input_sha256=%s\nentry=0x%08X\nplayable=false\n",
            TF_PROBE_INPUT_SHA256, TF_PROBE_ENTRY);
        return 0;
    }
    if (argc != 3 || (strcmp(argv[1], "--map") && strcmp(argv[1], "--run"))) {
        fprintf(stderr, "usage: %s --self-test | --provenance | --map image | --run image\n", argv[0]);
        return 2;
    }
    /* The small upstream mapper is not a general untrusted-PE parser. Accept
     * only the exact input validated by the build tool before touching it. */
    if (tf_probe_verify_image(argv[2])) return 3;
    pe_image image;
    hle_mem_init();
    if (recomp_pe_map(argv[2], &image)) return 4;
    if (image.entry != TF_PROBE_ENTRY || !recomp_lookup(image.entry)) {
        fprintf(stderr, "[probe] entry does not match the compiled dispatch\n");
        return 5;
    }
#if TF_PROBE_STARTUP
    fprintf(stderr,"[probe] registered %d selected Win32 startup APIs\n",tf_startup_register(&image,argv[2]));
#endif
#if TF_PROBE_PLATFORM
    fprintf(stderr,"[probe] registered %d window/COM APIs\n",tf_user32_register());
    if (tf_services_register(".")<0) return 7;
    fprintf(stderr,"[probe] registered %d input APIs\n",tf_input_register());
#endif
    /* Every unanswered import still binds to a named fatal boundary. */
    if (recomp_pe_bind(&image, hle_resolve_or_stub)) return 6;
    fprintf(stderr, "[probe] mapped verified input; unsupported APIs have fatal boundaries; playable=false\n");
    if (!strcmp(argv[1], "--map")) return 0;
    fprintf(stderr, "[probe] explicitly entering lifted entry 0x%08X\n", image.entry);
    uint32_t result = hle_call_guest(image.entry, 0, NULL);
    fprintf(stderr, "[probe] entry returned 0x%08X; this is not a gameplay result\n", result);
    return (int)(result & 255u);
}
