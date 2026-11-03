/* Selected POSIX-backed Win32 startup APIs. No renderer or permissive no-ops. */
#define _LARGEFILE64_SOURCE
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "win32hle.h"
#include "startup.h"
#include "startup_state.h"
#include "log_budget.h"
#ifdef TF_ANDROID_AUDIO
#include "startup_audio.h"
#endif

static const char *const admitted[] = {
    "GetVersion", "GetVersionExA", "GetCommandLineA", "GetCurrentProcess", "GetCurrentProcessId", "GetCurrentThreadId",
    "GetCurrentThread", "GetLastError", "SetLastError", "OutputDebugStringA", "ExitProcess", "TerminateProcess",
    "CreateThread", "ResumeThread", "ExitThread", "GetExitCodeThread",
    "HeapCreate", "HeapAlloc", "HeapFree", "HeapReAlloc", "HeapSize",
    "VirtualAlloc", "VirtualFree", "GetTickCount", "QueryPerformanceCounter", "QueryPerformanceFrequency",
    "Sleep", "SleepEx", "GetStartupInfoA", "InitializeCriticalSection", "EnterCriticalSection", "LeaveCriticalSection",
    "TryEnterCriticalSection", "DeleteCriticalSection", "InterlockedIncrement", "InterlockedDecrement",
    "InterlockedExchange", "InterlockedExchangeAdd", "InterlockedCompareExchange",
    "CreateEventA", "SetEvent", "ResetEvent", "CreateMutexA", "ReleaseMutex", "WaitForSingleObject", "WaitForMultipleObjects",
    "TlsAlloc", "TlsFree", "TlsGetValue", "TlsSetValue",
    "GetACP", "GetOEMCP", "GetCPInfo", "IsValidCodePage", "GetStringTypeA", "GetStringTypeW", "GetStringTypeExA",
    "LCMapStringA", "LCMapStringW", "CompareStringA", "CompareStringW", "GetLocaleInfoA", "GetUserDefaultLCID",
    "GetUserDefaultLangID", "GetThreadLocale", "IsValidLocale", "GetSystemInfo", "GetSystemDefaultLCID",
    "GetEnvironmentStrings", "GetEnvironmentStringsA", "GetEnvironmentStringsW", "FreeEnvironmentStringsA",
    "FreeEnvironmentStringsW", "GetEnvironmentVariableA", "SetEnvironmentVariableA", "MultiByteToWideChar", "WideCharToMultiByte",
    "GetModuleHandleA", "GetModuleFileNameA", "GetProcAddress", "LoadLibraryA", "FreeLibrary",
    "GetCurrentDirectoryA", "SetCurrentDirectoryA", "FindResourceA", "LoadResource", "LockResource", "SizeofResource", "LoadStringA",
    "CreateFileA", "ReadFile", "WriteFile", "SetFilePointer", "SetEndOfFile", "FlushFileBuffers", "GetFileSize",
    "GetFileType", "CloseHandle", "GetFileAttributesA", "CreateDirectoryA", "RemoveDirectoryA", "DeleteFileA", "MoveFileA",
    "FindFirstFileA", "FindNextFileA", "FindClose", "GetSystemTime", "GetLocalTime", "GetSystemTimeAsFileTime",
    "FileTimeToSystemTime", "SystemTimeToFileTime", "GetDateFormatA", "GetTimeFormatA",
    NULL
};
static TF_StartupState startup_state=TF_STARTUP_STATE_INITIALIZER;
static int registered, admitted_count;

static void output_debug_string(void) {
    const char *text=ASTR(0);if(!text)text="";
    // Scene add/remove warnings repeat every frame in this development EXE.
    // Keep samples and a count; other diagnostics and fatal errors are intact.
    if(strstr(text,"NUWARNING")&&strstr(text,"DynoScene.cpp")) {
        static TF_LogBudget budget;
        uint32_t suppressed=0;
        int allowed=tf_log_admit(&budget,hle_ticks_ms(),4,&suppressed);
        if(suppressed)fprintf(stderr,"[guest] repeated scene warnings suppressed: %u\n",suppressed);
        if(!allowed)RETV(1);
    }
    fprintf(stderr,"[guest] %s",text);RETV(1);
}

static void get_standard_handle(void) {
    int index = tf_standard_index(A32(0));
    if (index < 0) { hle_set_last_error(87); RET(0xffffffffu, 1); }
    RET(tf_standard_get(&startup_state,A32(0)), 1);
}
static void set_standard_handle(void) {
    int index = tf_standard_index(A32(0));
    if (index < 0) { hle_set_last_error(87); RET(0, 2); }
    tf_standard_set(&startup_state,A32(0),A32(1)); RET(1, 2);
}
static void set_exception_filter(void) {
    uint32_t previous = tf_filter_set(&startup_state,A32(0)); RET(previous, 1);
}
static void set_handle_count(void) { RET(A32(0), 1); } /* obsolete Windows API returns requested count */
static void get_process_affinity(void) {
    if (A32(0) != 0xffffffffu || !A32(1) || !A32(2)) { hle_set_last_error(87); RET(0, 3); }
    MEM32(A32(1)) = 1; MEM32(A32(2)) = 1; RET(1, 3);
}
static void set_process_affinity(void) {
    if (A32(0) != 0xffffffffu || A32(1) != 1) { hle_set_last_error(87); RET(0, 2); }
    RET(1, 2); /* the serialized guest exposes exactly one virtual CPU */
}
static void set_thread_affinity(void) {
    if (A32(0) != 0xfffffffeu || A32(1) != 1) { hle_set_last_error(87); RET(0, 2); }
    RET(1, 2); /* previous guest mask was 1; no host scheduling promise */
}
static void set_thread_priority(void) {
    // Android's serialized guest machine does not expose Windows priority
    // classes. Report a real unsupported-operation failure to the caller.
    uint32_t handle=A32(0);
    if(handle!=0xfffffffeu&&hle_handle_type(handle)!=HLE_H_THREAD){hle_set_last_error(6);RET(0,2);}
    hle_set_last_error(120);RET(0,2);
}
static void bink_sound_unavailable(void) {
    fprintf(stderr,"[media] Bink sound backend unavailable; sound-system setup failed\n");RET(0,2);
}
static uint32_t bink_track_count,bink_track_ids[32];
static void bink_sound_tracks(void) {
    uint32_t count=A32(0),tracks=A32(1);if(count>32||(count&&!tracks))hle_fatal("invalid Bink sound-track selection");
    bink_track_count=count;for(uint32_t i=0;i<count;++i)bink_track_ids[i]=MEM32(tracks+4*i);
    RET(0,2); /* void BinkSetSoundTrack: applies to subsequent opens */
}
static void bink_open_unavailable(void) {
    fprintf(stderr,"[media] BinkOpen failed: Bink decoder is not available (tracks=%u)\n",bink_track_count);
    RET(0,2); /* Real factory failure, no movie object or pretend frames. */
}
#ifndef TF_ANDROID_AUDIO
static void direct_sound_unavailable(void) {
    if(!A32(1))RET(0x80070057u,3);
    MEM32(A32(1))=0;if(A32(2))RET(0x80040110u,3);
    fprintf(stderr,"[audio] DirectSoundCreate8 failed: no Android sound driver implemented\n");
    RET(0x88780078u,3); /* DSERR_NODRIVER; no pretend device/mixer. */
}
#endif
static __thread uint32_t winsock_error;
static void winsock_unavailable(void) {
    fprintf(stderr,"[network] WSAStartup failed: Winsock transport is not implemented\n");
    RET(10091u,2); /* WSASYSNOTREADY; game can select its offline path. */
}
static void winsock_last_error(void){RET(winsock_error,0);}
static void winsock_set_error(void){winsock_error=A32(0);RET(0,1);}
static void winsock_lookup1(void){winsock_error=10093;RET(0,1);}
static void winsock_lookup2(void){winsock_error=10093;RET(0,2);}
static void winsock_lookup3(void){winsock_error=10093;RET(0,3);}
static void winsock_error0(void){winsock_error=10093;RET(0xffffffffu,0);}
static void winsock_error1(void){winsock_error=10093;RET(0xffffffffu,1);}
static void winsock_error3(void){winsock_error=10093;RET(0xffffffffu,3);}
static void winsock_error4(void){winsock_error=10093;RET(0xffffffffu,4);}
static void winsock_error5(void){winsock_error=10093;RET(0xffffffffu,5);}
static void winsock_swap16(void){RET(__builtin_bswap16((uint16_t)A32(0)),1);}
static void winsock_swap32(void){RET(__builtin_bswap32(A32(0)),1);}
static void winsock_inet_addr(void){if(!A32(0))RET(0xffffffffu,1);RET(inet_addr(ASTR(0)),1);}
static void winsock_inet_ntoa(void){
    static __thread uint32_t buffer;char text[16];uint32_t address=A32(0);
    if(!buffer)buffer=hle_va(hle_alloc(16));if(!buffer)RET(0,1);
    snprintf(text,sizeof text,"%u.%u.%u.%u",address&255,(address>>8)&255,(address>>16)&255,address>>24);
    memcpy(hle_ptr(buffer),text,strlen(text)+1);RET(buffer,1);
}
static void virtual_directory(const char *path) {
    uint32_t size=(uint32_t)strlen(path),capacity=A32(1),buffer=A32(0);
    if(capacity<=size)RET(size+1,2);
    if(!buffer){hle_set_last_error(87);RET(0,2);}
    memcpy(hle_ptr(buffer),path,size+1);RET(size,2);
}
static void system_directory(void){virtual_directory("C:\\WINDOWS\\SYSTEM32");}
static void windows_directory(void){virtual_directory("C:\\WINDOWS");}
static void set_file_pointer_ex(void) {
    int fd=hle_file_fd(A32(0));
    if(fd<0){hle_set_last_error(6);RET(0,5);}
    uint32_t method=A32(4);int whence;
    if(method==0)whence=SEEK_SET;else if(method==1)whence=SEEK_CUR;else if(method==2)whence=SEEK_END;
    else{hle_set_last_error(87);RET(0,5);}
    int64_t distance=(int64_t)(((uint64_t)A32(2)<<32)|A32(1));
    if(whence==SEEK_SET && distance<0){hle_set_last_error(131);RET(0,5);}
    off64_t result=lseek64(fd,(off64_t)distance,whence);
    if(result<0){hle_set_last_error(errno==EBADF?6:errno==ESPIPE?1:errno==EINVAL?131:25);RET(0,5);}
    uint32_t output=A32(3);if(output)MEM64(output)=(uint64_t)result;
    RET(1,5);
}

static int admitted_name(const char *name) {
    for (int i=0; admitted[i]; ++i) if (!strcmp(name, admitted[i])) return 1;
    return 0;
}
static void register_selected(const win32hle_shim *module) {
    for (; module->name; ++module) if (admitted_name(module->name)) {
        win32hle_shim one[2] = {{module->name, module->fn}, {NULL, NULL}};
        admitted_count += win32hle_register(one);
    }
}
int tf_startup_register(const pe_image *image, const char *host_path) {
    static char command_line[1024];
    if (!registered) {
        const win32hle_shim overrides[] = {
            {"OutputDebugStringA",output_debug_string},
            {"GetStdHandle",get_standard_handle}, {"SetStdHandle",set_standard_handle},
            {"SetHandleCount",set_handle_count}, {"SetUnhandledExceptionFilter",set_exception_filter},
            {"GetProcessAffinityMask",get_process_affinity}, {"SetProcessAffinityMask",set_process_affinity},
            {"SetThreadAffinityMask",set_thread_affinity}, {"SetThreadPriority",set_thread_priority}, {"SetFilePointerEx",set_file_pointer_ex},
            {"_BinkSetSoundSystem@8",bink_sound_unavailable},{"_BinkSetSoundTrack@8",bink_sound_tracks},
            {"_BinkOpen@8",bink_open_unavailable},
#ifndef TF_ANDROID_AUDIO
            {"dsound.dll#11",direct_sound_unavailable},{"DirectSoundCreate8",direct_sound_unavailable},
#endif
            {"ws2_32.dll#115",winsock_unavailable},
            {"WSAStartup",winsock_unavailable},{"GetSystemDirectoryA",system_directory},
            {"GetWindowsDirectoryA",windows_directory},
            {"ws2_32.dll#52",winsock_lookup1},{"gethostbyname",winsock_lookup1},
            {"ws2_32.dll#51",winsock_lookup3},{"gethostbyaddr",winsock_lookup3},
            {"ws2_32.dll#55",winsock_lookup2},{"ws2_32.dll#56",winsock_lookup2},
            {"ws2_32.dll#111",winsock_last_error},{"WSAGetLastError",winsock_last_error},
            {"ws2_32.dll#112",winsock_set_error},{"WSASetLastError",winsock_set_error},
            {"ws2_32.dll#116",winsock_error0},{"WSACleanup",winsock_error0},
            {"ws2_32.dll#23",winsock_error3},{"socket",winsock_error3},
            {"ws2_32.dll#4",winsock_error3},{"ws2_32.dll#3",winsock_error1},
            {"ws2_32.dll#12",winsock_error3},{"ws2_32.dll#16",winsock_error4},
            {"ws2_32.dll#19",winsock_error4},{"ws2_32.dll#18",winsock_error5},
            {"ws2_32.dll#7",winsock_error5},{"ws2_32.dll#21",winsock_error5},
            {"ws2_32.dll#8",winsock_swap32},{"ws2_32.dll#14",winsock_swap32},
            {"ws2_32.dll#9",winsock_swap16},{"ws2_32.dll#15",winsock_swap16},
            {"ws2_32.dll#10",winsock_inet_addr},{"ws2_32.dll#11",winsock_inet_ntoa},
            {NULL,NULL}
        };
        admitted_count += win32hle_register(overrides);
#ifdef TF_ANDROID_AUDIO
        admitted_count += tf_audio_register();
#endif
        register_selected(win32hle_kernel32); register_selected(win32hle_kernel32_crt);
        register_selected(win32hle_kernel32_ext); register_selected(win32hle_module);
        registered=1;
    }
    if (image && host_path) {
        char guest[1024];
#ifdef TF_ANDROID_AUDIO
        /* The Java session's EXE link resolves to an external SAF descriptor.
         * Keep its guest identity under the C: mount instead of exposing the
         * provider's host path, which may not be directly accessible. */
        snprintf(guest,sizeof guest,"C:\\Transformers.exe");
#else
        hle_guest_path(host_path,guest,sizeof guest);
#endif
        hle_module_add(guest,image->base,image->resource_rva,image->resource_size,1);
        snprintf(command_line,sizeof command_line,"\"%s\"",guest);
        hle_set_command_line(command_line);
    }
    return admitted_count;
}

static uint32_t invoke(const char *name,int count,uint32_t *arguments) {
    uint32_t va=hle_resolve(name); if (!va) hle_fatal("startup self-test missing %s",name);
    return hle_call_guest(va,count,arguments);
}
static void thread_fixture(void) {
    uint32_t block=A32(0),tls=MEM32(block),value=MEM32(block+4);
    uint32_t a[2]={tls,0};
    if(invoke("TlsGetValue",1,a)!=0)RET(0xbad00001u,1);
    a[1]=value;if(!invoke("TlsSetValue",2,a))RET(0xbad00002u,1);
    MEM32(block+8)=invoke("GetCurrentThreadId",0,a);
    a[0]=1;invoke("Sleep",1,a);a[0]=tls;
    if(invoke("TlsGetValue",1,a)!=value)RET(0xbad00003u,1);
    RET(value^0x12345678u,1);
}
static int thread_selftest(void) {
    const win32hle_shim fixture[]={{"TF.Startup.ThreadFixture",thread_fixture},{NULL,NULL}};
    win32hle_register(fixture);
    uint32_t a[6]={0},block=hle_va(hle_alloc(16));if(!block)return 48;
    uint32_t tls=invoke("TlsAlloc",0,a);MEM32(block)=tls;MEM32(block+4)=0xcafebabeu;
    a[0]=tls;a[1]=0x76543210u;if(!invoke("TlsSetValue",2,a))return 49;
    a[0]=0;a[1]=0;a[2]=hle_resolve("TF.Startup.ThreadFixture");a[3]=block;a[4]=4;a[5]=block+12;
    uint32_t thread=invoke("CreateThread",6,a);if(!thread)return 50;
    a[0]=thread;a[1]=0;if(invoke("WaitForSingleObject",2,a)!=258)return 51;
    a[1]=block+8;if(!invoke("GetExitCodeThread",2,a)||MEM32(block+8)!=259)return 52;
    a[0]=thread;if(invoke("ResumeThread",1,a)!=1)return 53;
    a[0]=thread;a[1]=2000;if(invoke("WaitForSingleObject",2,a)!=0)return 54;
    uint32_t tid=MEM32(block+8);if(!tid||tid!=MEM32(block+12))return 55;
    a[1]=block+8;if(!invoke("GetExitCodeThread",2,a)||MEM32(block+8)!=(0xcafebabeu^0x12345678u))return 56;
    a[0]=tls;if(invoke("TlsGetValue",1,a)!=0x76543210u)return 57;
    invoke("TlsFree",1,a);a[0]=thread;invoke("CloseHandle",1,a);hle_free(hle_ptr(block));
    return 0;
}
static int platform_failure_selftest(void) {
    uint32_t a[4]={0},buffer=hle_va(hle_alloc(64));if(!buffer)return 58;
    if(invoke("GetSystemDirectoryA",2,a)!=20)return 59;
    memset(hle_ptr(buffer),0xa5,64);a[0]=buffer;a[1]=19;
    if(invoke("GetSystemDirectoryA",2,a)!=20||MEM8(buffer)!=0xa5)return 60;
    a[1]=64;if(invoke("GetSystemDirectoryA",2,a)!=19||strcmp(hle_ptr(buffer),"C:\\WINDOWS\\SYSTEM32"))return 61;
    a[0]=0x202;a[1]=buffer;if(invoke("WSAStartup",2,a)!=10091)return 62;
    a[0]=buffer;if(invoke("gethostbyname",1,a)!=0||invoke("WSAGetLastError",0,a)!=10093)return 63;
    a[0]=0x12345678u;if(invoke("ws2_32.dll#8",1,a)!=0x78563412u||invoke("ws2_32.dll#9",1,a)!=0x7856u)return 64;
    a[0]=0x0100007fu;uint32_t address=invoke("ws2_32.dll#11",1,a);
    if(!address||strcmp(hle_ptr(address),"127.0.0.1"))return 65;
    a[0]=address;if(invoke("ws2_32.dll#10",1,a)!=0x0100007fu)return 66;
    a[0]=0;a[1]=buffer;a[2]=1;MEM32(buffer)=0xa5a5a5a5u;
    if(invoke("DirectSoundCreate8",3,a)!=0x80040110u||MEM32(buffer)!=0)return 67;
    hle_free(hle_ptr(buffer));return 0;
}
int tf_startup_selftest(void) {
    tf_startup_register(NULL,NULL);
    uint32_t args[8]={0};
    if (invoke("GetACP",0,args)!=1252) return 30;
    uint32_t heap=invoke("HeapCreate",3,args);
    args[0]=heap;args[1]=8;args[2]=64;
    uint32_t block=invoke("HeapAlloc",3,args);if (!block) return 31;
    for (unsigned i=0;i<64;++i) if (MEM8(block+i)) return 32;
    MEM32(block)=0xcafebabe;args[0]=heap;args[1]=0;args[2]=block;args[3]=128;
    uint32_t resized=invoke("HeapReAlloc",4,args);if (!resized || MEM32(resized)!=0xcafebabe) return 33;
    args[2]=resized;if (invoke("HeapSize",3,args)!=128 || !invoke("HeapFree",3,args)) return 34;
    uint32_t tls=invoke("TlsAlloc",0,args);args[0]=tls;args[1]=0x1234;
    if (!invoke("TlsSetValue",2,args) || invoke("TlsGetValue",1,args)!=0x1234 || !invoke("TlsFree",1,args)) return 35;
    args[0]=0;args[1]=1;args[2]=0;args[3]=0;uint32_t event=invoke("CreateEventA",4,args);
    args[0]=event;args[1]=0;if (invoke("WaitForSingleObject",2,args)!=258) return 36;
    if (!invoke("SetEvent",1,args) || invoke("WaitForSingleObject",2,args)!=0) return 37;
    invoke("CloseHandle",1,args);
    args[0]=0xfffffff5u;uint32_t original=invoke("GetStdHandle",1,args);
    args[1]=0x3456;if (!invoke("SetStdHandle",2,args) || invoke("GetStdHandle",1,args)!=0x3456) return 38;
    args[1]=original;invoke("SetStdHandle",2,args);
    args[0]=0x401000;if (invoke("SetUnhandledExceptionFilter",1,args)!=0) return 39;
    args[0]=0;if (invoke("SetUnhandledExceptionFilter",1,args)!=0x401000) return 40;
    char name[96];snprintf(name,sizeof name,".transformers-probe-%u.tmp",(unsigned)getpid());
    uint32_t name_va=hle_guest_strdup(name),buffer=hle_va(hle_alloc(16));
    if(!name_va || !buffer)return 41;
    args[0]=name_va;args[1]=0xc0000000;args[2]=0;args[3]=0;args[4]=2;args[5]=0x80;args[6]=0;
    uint32_t file=invoke("CreateFileA",7,args);if(file==0xffffffffu)return 42;
    MEM32(buffer)=0x12345678;args[0]=file;args[1]=buffer;args[2]=4;args[3]=buffer+8;args[4]=0;
    if(!invoke("WriteFile",5,args) || MEM32(buffer+8)!=4)return 43;
    args[0]=file;args[1]=8;args[2]=1;args[3]=buffer+8;args[4]=0;
    if(!invoke("SetFilePointerEx",5,args) || MEM64(buffer+8)!=0x100000008ull)return 44;
    args[1]=0;args[2]=0;if(!invoke("SetFilePointerEx",5,args))return 45;
    MEM32(buffer)=0;args[1]=buffer;args[2]=4;args[3]=buffer+8;args[4]=0;
    if(!invoke("ReadFile",5,args) || MEM32(buffer)!=0x12345678)return 46;
    invoke("CloseHandle",1,args);args[0]=name_va;if(!invoke("DeleteFileA",1,args))return 47;
    hle_free(hle_ptr(name_va));hle_free(hle_ptr(buffer));
    int threads=thread_selftest();if(threads)return threads;
    int platform=platform_failure_selftest();if(platform)return platform;
    puts("[startup] API self-test passed: heap, TLS, event, handles, filter, 64-bit seek, suspended thread/resume/wait/exit and thread-local TLS; no game executed");
    return 0;
}
