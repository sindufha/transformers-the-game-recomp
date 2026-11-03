#include "android_vulkan.h"
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <unistd.h>
#include <cstdio>
#include <string>
#include <stdexcept>
#include <cstdlib>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <cctype>

extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL __real_vkGetInstanceProcAddr(VkInstance,const char*);
namespace {
PFN_vkGetInstanceProcAddr selected_proc=__real_vkGetInstanceProcAddr;
#if defined(__aarch64__)
bool system_has_adreno6xx() {
    auto create=reinterpret_cast<PFN_vkCreateInstance>(__real_vkGetInstanceProcAddr(nullptr,"vkCreateInstance"));
    if(!create)return false;
    VkInstanceCreateInfo ci{};ci.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    VkInstance instance=VK_NULL_HANDLE;if(create(&ci,nullptr,&instance)!=VK_SUCCESS)return false;
    auto destroy=reinterpret_cast<PFN_vkDestroyInstance>(__real_vkGetInstanceProcAddr(instance,"vkDestroyInstance"));
    auto enumerate=reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(__real_vkGetInstanceProcAddr(instance,"vkEnumeratePhysicalDevices"));
    auto properties=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(__real_vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceProperties"));
    bool found=false;
    if(enumerate&&properties){
        VkPhysicalDevice devices[16]{};uint32_t count=16;auto status=enumerate(instance,&count,devices);
        if(status==VK_SUCCESS||status==VK_INCOMPLETE)for(uint32_t i=0;i<count&&i<16;++i){
            VkPhysicalDeviceProperties p{};properties(devices[i],&p);
            if(p.vendorID!=0x5143||!std::strstr(p.deviceName,"Adreno"))continue;
            const char* name=p.deviceName;
            while(*name){
                if(std::isdigit(static_cast<unsigned char>(*name))){char* end=nullptr;long number=std::strtol(name,&end,10);
                    if(number>=600&&number<700)found=true;name=end;
                }else ++name;
            }
            std::fprintf(stderr,"[vulkan] system adapter %s; bundled Turnip eligible=%s\n",p.deviceName,found?"yes":"no");
        }
    }
    if(destroy)destroy(instance,nullptr);
    return found;
}
#endif
}
extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL __wrap_vkGetInstanceProcAddr(VkInstance instance,const char* name) {
    return selected_proc(instance,name);
}
void tf_android_configure_vulkan_driver(const char* game_directory,const char* mode,const char* selected_path) {
    selected_proc=__real_vkGetInstanceProcAddr;
    std::string selection=mode?mode:"auto";
    if(selection!="auto"&&selection!="system"&&selection!="custom")throw std::runtime_error("Invalid GPU driver selection");
    std::string cache=std::string(game_directory)+"/.cache";
    if(mkdir(cache.c_str(),0700)&&errno!=EEXIST)throw std::runtime_error("Cannot create private Vulkan cache");
    setenv("XDG_CACHE_HOME",cache.c_str(),1);
    if(selection=="system"){
        std::fprintf(stderr,"[vulkan] driver mode=system; system loader selected\n");return;
    }
    std::string directory;
    const bool private_requested=selection=="custom";
#if defined(__aarch64__)
    Dl_info info{};
    if(!dladdr(reinterpret_cast<void*>(&__wrap_vkGetInstanceProcAddr),&info)||!info.dli_fname)
        throw std::runtime_error("Cannot locate Android native libraries");
    std::string native=info.dli_fname;auto slash=native.find_last_of('/');
    if(slash==std::string::npos)throw std::runtime_error("Android native library path is not absolute");
    native.resize(slash);
    std::string selected_name;
    const char* driver_name=nullptr;
    if(private_requested){
        if(!selected_path||!*selected_path)throw std::runtime_error("Custom GPU driver is not selected");
        std::string library_root=std::string(game_directory)+"/../drivers/library";
        char* root=realpath(library_root.c_str(),nullptr);char* file=realpath(selected_path,nullptr);
        if(!root||!file){free(root);free(file);throw std::runtime_error("Custom GPU driver is unavailable");}
        std::string allowed=std::string(root)+"/",resolved=file;free(root);free(file);
        struct stat st{};
        if(resolved.compare(0,allowed.size(),allowed)||stat(resolved.c_str(),&st)||!S_ISREG(st.st_mode)||st.st_size<4096||st.st_size>268435456)
            throw std::runtime_error("Custom GPU driver must be an imported private library");
        auto end=resolved.find_last_of('/');directory=resolved.substr(0,end+1);selected_name=resolved.substr(end+1);driver_name=selected_name.c_str();
    }else{
        if(access((native+"/libvulkan_turnip.so").c_str(),F_OK)||!system_has_adreno6xx()){
            std::fprintf(stderr,"[vulkan] system loader selected\n");return;
        }
        directory=native+"/";driver_name="libvulkan_turnip.so";
    }
    void* tools=dlopen((native+"/libadrenotools.so").c_str(),RTLD_NOW|RTLD_LOCAL);
    if(!tools)throw std::runtime_error(std::string("Adreno tools unavailable: ")+dlerror());
    using Open=void*(*)(int,int,const char*,const char*,const char*,const char*,const char*,void**);
    auto open=reinterpret_cast<Open>(dlsym(tools,"adrenotools_open_libvulkan"));
    if(!open)throw std::runtime_error("Adreno tools entry unavailable");
    void* loader=open(RTLD_NOW,1,nullptr,native.c_str(),directory.c_str(),driver_name,nullptr,nullptr);
    if(!loader)throw std::runtime_error("Selected GPU driver failed to load");
    auto proc=reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(loader,"vkGetInstanceProcAddr"));
    if(!proc)throw std::runtime_error("Private Vulkan entry unavailable");
    // Both libraries deliberately live until the separate game process exits.
    selected_proc=proc;
    std::fprintf(stderr,"[vulkan] driver mode=%s library=%s%s; device properties below confirm the loaded driver\n",selection.c_str(),directory.c_str(),driver_name);
#else
    if(private_requested)throw std::runtime_error("Private Adreno driver requires ARM64");
    std::fprintf(stderr,"[vulkan] system loader selected\n");
#endif
}

extern "C" int tf_android_driver_probe(const char* mode,const char* selected_path){
    char cwd[4096];if(!getcwd(cwd,sizeof cwd))return 230;
    try{
        tf_android_configure_vulkan_driver(cwd,mode,selected_path);
        auto create=reinterpret_cast<PFN_vkCreateInstance>(selected_proc(nullptr,"vkCreateInstance"));
        if(!create)return 231;
        VkInstanceCreateInfo ci{};ci.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;VkInstance instance=VK_NULL_HANDLE;
        if(create(&ci,nullptr,&instance)!=VK_SUCCESS)return 232;
        auto destroy=reinterpret_cast<PFN_vkDestroyInstance>(selected_proc(instance,"vkDestroyInstance"));
        auto enumerate=reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(selected_proc(instance,"vkEnumeratePhysicalDevices"));
        auto properties=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(selected_proc(instance,"vkGetPhysicalDeviceProperties"));
        VkPhysicalDevice devices[16]{};uint32_t count=16;int result=233;
        if(enumerate&&properties){auto status=enumerate(instance,&count,devices);if((status==VK_SUCCESS||status==VK_INCOMPLETE)&&count){
            VkPhysicalDeviceProperties p{};properties(devices[0],&p);
            std::fprintf(stderr,"[driver-test] PASS mode=%s device=%s vendor=%04x driver=%u api=%u; no game, display or input\n",mode,p.deviceName,p.vendorID,p.driverVersion,p.apiVersion);result=0;
        }}
        if(destroy)destroy(instance,nullptr);return result;
    }catch(const std::exception& error){std::fprintf(stderr,"[driver-test] FAIL: %s\n",error.what());return 234;}
}
