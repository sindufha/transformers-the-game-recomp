/* Guest32 to native D3D9 adapter. No title-specific addresses or patches.
 * Native interfaces/definitions: pinned DXVK native headers (Wine LGPL notices
 * retained in the dependency). The guest owns 32-bit handles, never pointers
 * to native C++ objects, lock mappings, Android windows, or host stack locals. */
#include "d3d9_bridge.h"
#include "d3d9_math.h"
#include "d3d9_dds.h"
#include "d3d9_bmp.h"
#include "d3d9_shader.h"
#include "frame_stats.h"
#include "com_handle_cache.h"
#include "startup_audio.h"
#include <d3d9.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <dirent.h>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <time.h>

extern "C" IDirect3D9 *Direct3DCreate9(UINT sdk);
extern "C" void tf_android_suspend_device(void*);
extern "C" void tf_android_suspend_swapchain(void*);
namespace {
enum Kind:uint32_t {D3D9=1,Device,Texture,Cube,Surface,VertexBuffer,IndexBuffer,
    VertexShader,PixelShader,Declaration,StateBlock,Query,SwapChain,VolumeTexture,Volume,Buffer,ConstantTable};
constexpr uint32_t counts[]={0,17,119,22,22,17,14,14,5,5,5,6,8,10,22,11,5,27};
// Published ID3DXBuffer IID, Wine include/d3dx9core.h / DirectX SDK ABI.
constexpr GUID buffer_iid={0x8ba5fb08,0x5195,0x40e2,{0xac,0x58,0x0d,0x98,0x9c,0x3a,0x01,0x02}};
constexpr GUID constant_iid={0x9dca3190,0x38b9,0x4fc3,{0x92,0xe3,0x39,0xc6,0xdd,0xfb,0x35,0x8b}};
struct Object {IUnknown *native;uint32_t kind,references,table,guest_window=0;};
std::map<uint32_t,Object> objects;
TF_ComCache object_cache{};
std::map<std::pair<IUnknown*,uint32_t>,uint32_t> addresses;
uintptr_t guest_base;
uint64_t guest_limit;
uint32_t (*allocate_guest)(uint32_t);
void (*release_guest)(uint32_t);
void *(*window_callback)(void);
uint32_t render_scale_percent=100;
TF_FrameStats frame_stats{};
void (*frame_observer)(void)=nullptr;
void (*operation_enter)(void)=nullptr;
void (*operation_leave)(void)=nullptr;
thread_local unsigned operation_depth=0;
struct Operation {
    Operation(){if(!operation_depth&&operation_enter)operation_enter();++operation_depth;}
    ~Operation(){if(--operation_depth==0&&operation_leave)operation_leave();}
};
uint64_t wall_time_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
void record_present(uint64_t begin,HRESULT result) {
    if(FAILED(result))return;
    if(frame_observer)frame_observer();
    uint64_t now=wall_time_ns();timespec cpu{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID,&cpu);
    uint64_t cpu_ns=uint64_t(cpu.tv_sec)*UINT64_C(1000000000)+cpu.tv_nsec;
    TF_FrameReport report{};
    if(tf_frame_stats_update(&frame_stats,now,cpu_ns,now-begin,&report)) {
        tf_audio_service();
        double n=double(report.frames);
        std::fprintf(stderr,"[perf] fps=%.2f frame_ms=%.2f worker_cpu_ms=%.2f present_ms=%.2f draws/frame=%.1f bridge_calls/frame=%.1f\n",
            n*1e9/double(report.wall),double(report.wall)/(n*1e6),
            double(report.cpu)/(n*1e6),double(report.present)/(n*1e6),
            double(report.draws)/n,double(report.calls)/n);
    }
}
[[noreturn]] void unsupported(const std::string& detail) {
    throw std::runtime_error("D3D9 guest bridge: "+detail);
}
void *pointer(uint32_t address,uint64_t bytes=1) {
    if(!address)return nullptr;
    if(!guest_limit||bytes>guest_limit||uint64_t(address)>guest_limit-bytes)
        unsupported("guest memory span outside address space");
    return reinterpret_cast<void*>(guest_base+address);
}
template<class T>T *guest(uint32_t address,uint64_t count=1) {
    if(count>std::numeric_limits<uint64_t>::max()/sizeof(T))unsupported("guest array overflow");
    return static_cast<T*>(pointer(address,count*sizeof(T)));
}
uint32_t read(uint32_t address){if(!address)unsupported("null DWORD input");uint32_t value;std::memcpy(&value,pointer(address,4),4);return value;}
void write(uint32_t address,uint32_t value){if(!address)unsupported("null DWORD output");std::memcpy(pointer(address,4),&value,4);}
uint32_t allocation(uint64_t bytes) {
    if(!bytes||bytes>0x10000000u)unsupported("unsupported/oversized guest allocation");
    uint32_t address=allocate_guest(static_cast<uint32_t>(bytes));
    if(!address)throw std::bad_alloc();pointer(address,bytes);return address;
}
float float_value(uint32_t word){float result;std::memcpy(&result,&word,4);return result;}
uint32_t float_bits(float value){uint32_t result;std::memcpy(&result,&value,4);return result;}
HWND native_window(){return window_callback?window_callback():nullptr;}
Object& find(uint32_t address) {
    if(auto object=static_cast<Object*>(tf_com_cache_get(&object_cache,address))){
        if(!object->references)unsupported("invalid or released COM handle");
        return *object;
    }
    auto found=objects.find(address);
    if(found==objects.end()||!found->second.references)unsupported("invalid or released COM handle");
    tf_com_cache_put(&object_cache,address,&found->second);
    return found->second;
}
template<class T>bool kind_matches(uint32_t kind) {
    if constexpr(std::is_same_v<T,IDirect3D9>)return kind==D3D9;
    if constexpr(std::is_same_v<T,IDirect3DDevice9>)return kind==Device;
    if constexpr(std::is_same_v<T,IDirect3DBaseTexture9>)return kind==Texture||kind==Cube||kind==VolumeTexture;
    if constexpr(std::is_same_v<T,IDirect3DTexture9>)return kind==Texture;
    if constexpr(std::is_same_v<T,IDirect3DCubeTexture9>)return kind==Cube;
    if constexpr(std::is_same_v<T,IDirect3DSurface9>)return kind==Surface;
    if constexpr(std::is_same_v<T,IDirect3DVertexBuffer9>)return kind==VertexBuffer;
    if constexpr(std::is_same_v<T,IDirect3DIndexBuffer9>)return kind==IndexBuffer;
    if constexpr(std::is_same_v<T,IDirect3DVertexShader9>)return kind==VertexShader;
    if constexpr(std::is_same_v<T,IDirect3DPixelShader9>)return kind==PixelShader;
    if constexpr(std::is_same_v<T,IDirect3DVertexDeclaration9>)return kind==Declaration;
    if constexpr(std::is_same_v<T,IDirect3DStateBlock9>)return kind==StateBlock;
    if constexpr(std::is_same_v<T,IDirect3DQuery9>)return kind==Query;
    if constexpr(std::is_same_v<T,IDirect3DSwapChain9>)return kind==SwapChain;
    if constexpr(std::is_same_v<T,IDirect3DVolumeTexture9>)return kind==VolumeTexture;
    if constexpr(std::is_same_v<T,IDirect3DVolume9>)return kind==Volume;
    return false;
}
template<class T>T *native_object(uint32_t address) {
    if(!address)return nullptr;
    auto& object=find(address);
    if(!kind_matches<T>(object.kind))unsupported("COM resource interface mismatch");
    return static_cast<T*>(object.native);
}
uint32_t wrap(IUnknown *native,uint32_t kind) {
    if(!native)return 0;
    if(!kind||kind>ConstantTable)unsupported("unknown COM kind");
    auto key=std::make_pair(native,kind);auto old=addresses.find(key);
    if(old!=addresses.end()){++objects.at(old->second).references;return old->second;}
    uint32_t table=allocation(counts[kind]*4),address=allocation(4);
    for(uint32_t i=0;i<counts[kind];++i) {
        uint32_t method=tf_d3d9_method_address(kind,i);
        if(!method)unsupported("COM methods were not registered before creation");
        write(table+4*i,method);
    }
    write(address,table);objects.emplace(address,Object{native,kind,1,table});addresses.emplace(key,address);
    return address;
}
uint32_t wrap_texture(IDirect3DBaseTexture9 *native) {
    if(!native)return 0;
    switch(native->GetType()){
    case D3DRTYPE_TEXTURE:return wrap(native,Texture);
    case D3DRTYPE_CUBETEXTURE:return wrap(native,Cube);
    case D3DRTYPE_VOLUMETEXTURE:return wrap(native,VolumeTexture);
    default:unsupported("unrecognized base texture type");}
}
void retire(uint32_t address) {
    auto found=objects.find(address);if(found==objects.end())return;
    tf_com_cache_erase(&object_cache,address);
    addresses.erase(std::make_pair(found->second.native,found->second.kind));
    release_guest(found->second.table);release_guest(address);objects.erase(found);
}
// The only structures whose native ABI includes pointer-sized fields are
// marshalled separately. Scalar-only D3D9 structs use the header's pack(4).
static_assert(offsetof(D3DADAPTER_IDENTIFIER9,WHQLLevel)==1096,"guest adapter identifier field layout");
static_assert(sizeof(D3DADAPTER_IDENTIFIER9)>=1100,"native adapter identifier size");
static_assert(sizeof(D3DCAPS9)==304,"guest device capabilities layout");
static_assert(sizeof(D3DSURFACE_DESC)==32,"guest surface description layout");
static_assert(sizeof(D3DVERTEXELEMENT9)==8,"guest vertex element layout");
D3DPRESENT_PARAMETERS present_parameters(uint32_t address) {
    auto p=guest<uint32_t>(address,14);if(!p)unsupported("null presentation parameters");
    D3DPRESENT_PARAMETERS r{};
    r.BackBufferWidth=p[0];r.BackBufferHeight=p[1];r.BackBufferFormat=static_cast<D3DFORMAT>(p[2]);r.BackBufferCount=p[3];
    r.MultiSampleType=static_cast<D3DMULTISAMPLE_TYPE>(p[4]);r.MultiSampleQuality=p[5];r.SwapEffect=static_cast<D3DSWAPEFFECT>(p[6]);
    r.hDeviceWindow=native_window();r.Windowed=TRUE; // Android surface is a host-managed window.
    r.EnableAutoDepthStencil=p[9];r.AutoDepthStencilFormat=static_cast<D3DFORMAT>(p[10]);r.Flags=p[11];
    r.FullScreen_RefreshRateInHz=0;r.PresentationInterval=p[13];return r;
}
uint32_t scaled_dimension(uint32_t value) {
    if(!value||render_scale_percent>=100)return value;
    uint64_t scaled=(uint64_t(value)*render_scale_percent+50)/100;
    return static_cast<uint32_t>(std::max<uint64_t>(2,scaled&~uint64_t(1)));
}
void apply_render_scale(D3DPRESENT_PARAMETERS& p) {
    p.BackBufferWidth=scaled_dimension(p.BackBufferWidth);
    p.BackBufferHeight=scaled_dimension(p.BackBufferHeight);
}
void store_present(uint32_t address,const D3DPRESENT_PARAMETERS& p) {
    auto output=guest<uint32_t>(address,14);if(!output)unsupported("null presentation output");
    output[0]=p.BackBufferWidth;output[1]=p.BackBufferHeight;output[2]=p.BackBufferFormat;output[3]=p.BackBufferCount;
    output[4]=p.MultiSampleType;output[5]=p.MultiSampleQuality;output[6]=p.SwapEffect;
    // +28 is a guest HWND and +32 the guest fullscreen preference: neither is a native pointer.
    output[9]=p.EnableAutoDepthStencil;output[10]=p.AutoDepthStencilFormat;output[11]=p.Flags;
    output[12]=p.FullScreen_RefreshRateInHz;output[13]=p.PresentationInterval;
}
uint32_t iid_kind(const GUID& iid,uint32_t fallback=0) {
    if(iid==__uuidof(IUnknown))return fallback;
    if(iid==constant_iid)return ConstantTable;
    if(iid==buffer_iid)return Buffer;
    if(iid==__uuidof(IDirect3D9))return D3D9;
    if(iid==__uuidof(IDirect3DDevice9))return Device;
    if(iid==__uuidof(IDirect3DTexture9))return Texture;
    if(iid==__uuidof(IDirect3DCubeTexture9))return Cube;
    if(iid==__uuidof(IDirect3DSurface9))return Surface;
    if(iid==__uuidof(IDirect3DVertexBuffer9))return VertexBuffer;
    if(iid==__uuidof(IDirect3DIndexBuffer9))return IndexBuffer;
    if(iid==__uuidof(IDirect3DVertexShader9))return VertexShader;
    if(iid==__uuidof(IDirect3DPixelShader9))return PixelShader;
    if(iid==__uuidof(IDirect3DVertexDeclaration9))return Declaration;
    if(iid==__uuidof(IDirect3DStateBlock9))return StateBlock;
    if(iid==__uuidof(IDirect3DQuery9))return Query;
    if(iid==__uuidof(IDirect3DSwapChain9))return SwapChain;
    if(iid==__uuidof(IDirect3DVolumeTexture9))return VolumeTexture;
    if(iid==__uuidof(IDirect3DVolume9))return Volume;
    if(iid==__uuidof(IDirect3DResource9)||iid==__uuidof(IDirect3DBaseTexture9))return fallback;
    return 0;
}
struct Lock {void *native;uint32_t guest,pitch,row_bytes,rows,flags;};
std::map<uint64_t,Lock> locks;
uint64_t lock_key(uint32_t address,uint32_t subresource){return uint64_t(address)<<32|subresource;}
bool compressed(D3DFORMAT format){return format>=D3DFMT_DXT1&&format<=D3DFMT_DXT5;}
uint32_t row_bytes(D3DFORMAT format,uint32_t width) {
    if(width>0x1000000u)unsupported("texture row exceeds supported limit");
    switch(format){
    case D3DFMT_DXT1:return ((width+3)/4)*8;
    case D3DFMT_DXT2:case D3DFMT_DXT3:case D3DFMT_DXT4:case D3DFMT_DXT5:return ((width+3)/4)*16;
    case D3DFMT_R8G8B8:return width*3;
    case D3DFMT_A8:case D3DFMT_L8:case D3DFMT_P8:return width;
    case D3DFMT_R5G6B5:case D3DFMT_X1R5G5B5:case D3DFMT_A1R5G5B5:case D3DFMT_A4R4G4B4:
    case D3DFMT_X4R4G4B4:case D3DFMT_A8L8:case D3DFMT_L16:case D3DFMT_R16F:case D3DFMT_V8U8:return width*2;
    case D3DFMT_A8R8G8B8:case D3DFMT_X8R8G8B8:case D3DFMT_A8B8G8R8:case D3DFMT_X8B8G8R8:
    case D3DFMT_A2R10G10B10:case D3DFMT_A2B10G10R10:case D3DFMT_G16R16:case D3DFMT_R32F:
    case D3DFMT_G16R16F:case D3DFMT_Q8W8V8U8:case D3DFMT_V16U16:return width*4;
    case D3DFMT_A16B16G16R16:case D3DFMT_A16B16G16R16F:case D3DFMT_G32R32F:return width*8;
    case D3DFMT_A32B32G32R32F:return width*16;
    default:unsupported("lock pixel format "+std::to_string(format)+" is unsupported");}
}
HRESULT stage_pixels(uint32_t address,uint32_t subresource,const D3DSURFACE_DESC& desc,
    const D3DLOCKED_RECT& lock,const RECT *rectangle,uint32_t flags,uint32_t output) {
    RECT r=rectangle?*rectangle:RECT{0,0,static_cast<LONG>(desc.Width),static_cast<LONG>(desc.Height)};
    if(r.left<0||r.top<0||r.right<=r.left||r.bottom<=r.top||uint32_t(r.right)>desc.Width||uint32_t(r.bottom)>desc.Height)
        unsupported("invalid lock rectangle");
    uint32_t width=r.right-r.left,height=r.bottom-r.top;
    if(compressed(desc.Format)&&((r.left%4)||(r.top%4)||(r.right%4&&uint32_t(r.right)!=desc.Width)||(r.bottom%4&&uint32_t(r.bottom)!=desc.Height)))
        unsupported("compressed lock rectangle is not block aligned");
    uint32_t rows=compressed(desc.Format)?(height+3)/4:height,bytes=row_bytes(desc.Format,width);
    if(!lock.pBits||lock.Pitch<=0||uint32_t(lock.Pitch)<bytes)unsupported("invalid native lock pitch/pointer");
    uint32_t temporary=allocation(uint64_t(lock.Pitch)*rows);
    if(!(flags&D3DLOCK_DISCARD))for(uint32_t row=0;row<rows;++row)
        std::memcpy(static_cast<uint8_t*>(pointer(temporary))+uint64_t(row)*lock.Pitch,
            static_cast<const uint8_t*>(lock.pBits)+uint64_t(row)*lock.Pitch,bytes);
    locks.emplace(lock_key(address,subresource),Lock{lock.pBits,temporary,uint32_t(lock.Pitch),bytes,rows,flags});
    write(output,uint32_t(lock.Pitch));write(output+4,temporary);return D3D_OK;
}
void copy_lock(uint32_t address,uint32_t subresource) {
    auto found=locks.find(lock_key(address,subresource));if(found==locks.end())unsupported("unlock without a guest lock");
    const auto& lock=found->second;
    if(!(lock.flags&D3DLOCK_READONLY))for(uint32_t row=0;row<lock.rows;++row)
        std::memcpy(static_cast<uint8_t*>(lock.native)+uint64_t(row)*lock.pitch,
            static_cast<const uint8_t*>(pointer(lock.guest))+uint64_t(row)*lock.pitch,lock.row_bytes);
}
void free_lock(uint32_t address,uint32_t subresource) {
    auto found=locks.find(lock_key(address,subresource));if(found==locks.end())unsupported("missing guest lock");
    release_guest(found->second.guest);locks.erase(found);
}
template<class T,class Desc>HRESULT lock_buffer(uint32_t address,const uint32_t *args) {
    if(!args[3]||locks.count(lock_key(address,0)))return D3DERR_INVALIDCALL;
    pointer(args[3],4);auto buffer=native_object<T>(address);Desc desc{};
    HRESULT result=buffer->GetDesc(&desc);if(FAILED(result))return result;
    uint32_t offset=args[1],length=args[2];if(offset>desc.Size||length>desc.Size-offset)return D3DERR_INVALIDCALL;
    if(!length)length=desc.Size-offset;if(!length)return D3DERR_INVALIDCALL;
    void *native=nullptr;result=buffer->Lock(offset,length,&native,args[4]);if(FAILED(result))return result;
    uint32_t temporary=allocation(length);if(!(args[4]&D3DLOCK_DISCARD))std::memcpy(pointer(temporary,length),native,length);
    locks.emplace(lock_key(address,0),Lock{native,temporary,length,length,1,args[4]});write(args[3],temporary);return result;
}
void validate_shader(uint32_t address) {
    if(!address)unsupported("null shader bytecode");
    uint32_t version=read(address);
    if(!tf_d3d9_shader_version(version))unsupported("invalid or unsupported shader version");
    if(((version>>8)&255)==1)std::fprintf(stderr,"[d3d9] validating legacy shader %08x\n",version);
    uint32_t offset=4;
    while(offset<0x100000){
        if(uint64_t(address)+offset+4>guest_limit)unsupported("shader bytecode exceeds guest address space");
        uint32_t instruction=read(address+offset),opcode=instruction&0xffff;
        uint32_t words;
        if(!tf_d3d9_shader_words(version,instruction,&words))unsupported("invalid shader opcode/length");
        if(opcode==0xffff)return;
        uint64_t size=uint64_t(words+1)*4;
        if(size>0x100000-offset||uint64_t(address)+offset+size>guest_limit)unsupported("oversized shader instruction");
        pointer(address+offset,size);offset+=uint32_t(size);
    }
    unsupported("unterminated or oversized shader bytecode");
}
void validate_arrays(uint32_t kind,uint32_t method,const uint32_t *args) {
    if(kind==Device){
        if(method==43)guest<D3DRECT>(args[2],args[1]);
        if(method==55||method==56)guest<float>(args[2],4);
        if(method==71||method==72)guest<PALETTEENTRY>(args[2],256);
        if((method>=94&&method<=97)||(method>=109&&method<=112))guest<uint32_t>(args[2],uint64_t(args[3])*4);
        if(method==98||method==99||method==113||method==114)guest<uint32_t>(args[2],args[3]);
        if(method==86){
            if(!args[1])unsupported("null vertex declaration");
            bool end=false;for(unsigned i=0;i<65;++i){auto p=guest<D3DVERTEXELEMENT9>(args[1]+8*i);if(p->Stream==0xff){end=true;break;}}
            if(!end)unsupported("unterminated vertex declaration");
        }
        if(method==91||method==106)validate_shader(args[1]);
        if(method==83||method==84){
            uint32_t count=method==83?args[2]:args[4],type=args[1];uint64_t vertices;
            switch(type){case D3DPT_POINTLIST:vertices=count;break;case D3DPT_LINELIST:vertices=uint64_t(count)*2;break;
            case D3DPT_LINESTRIP:vertices=uint64_t(count)+1;break;case D3DPT_TRIANGLELIST:vertices=uint64_t(count)*3;break;
            case D3DPT_TRIANGLESTRIP:case D3DPT_TRIANGLEFAN:vertices=uint64_t(count)+2;break;default:unsupported("invalid primitive type");}
            if(method==83)pointer(args[3],vertices*args[4]);
            else {uint32_t index_bytes=args[6]==D3DFMT_INDEX16?2:args[6]==D3DFMT_INDEX32?4:0;if(!index_bytes)unsupported("invalid UP index format");
                pointer(args[5],vertices*index_bytes);pointer(args[7],uint64_t(args[2]+uint64_t(args[3]))*args[8]);}
        }
    }
    if(method==4&&(kind==Texture||kind==Cube||kind==Surface||kind==VertexBuffer||kind==IndexBuffer||kind==VolumeTexture||kind==Volume))pointer(args[2],args[3]);
    if(method==5&&(kind==Texture||kind==Cube||kind==Surface||kind==VertexBuffer||kind==IndexBuffer||kind==VolumeTexture||kind==Volume)) {
        if(args[3])pointer(args[2],read(args[3]));
    }
    if(kind==Query&&method==7)pointer(args[1],args[2]);
    if((kind==VertexShader||kind==PixelShader||kind==Declaration)&&method==4&&args[2])
        pointer(args[1],uint64_t(read(args[2]))*(kind==Declaration?8:1));
}
struct GuestBuffer: IUnknown {
    uint32_t data,size,refs=1;
    GuestBuffer(uint32_t n):data(allocation(n?n:1)),size(n){}
    HRESULT QueryInterface(REFIID iid,void **out)override{
        if(!out)return E_POINTER;*out=nullptr;if(iid!=__uuidof(IUnknown)&&iid!=buffer_iid)return E_NOINTERFACE;
        *out=this;AddRef();return S_OK;
    }
    ULONG AddRef()override{return ++refs;}
    virtual ~GuestBuffer(){release_guest(data);}
    ULONG Release()override{auto left=--refs;if(!left)delete this;return left;}
};
uint32_t create_buffer(uint32_t size){return wrap(new GuestBuffer(size),Buffer);}
template<class T>struct ComRelease {void operator()(T *p)const{if(p)p->Release();}};
template<class T>using ComOwner=std::unique_ptr<T,ComRelease<T>>;
struct SurfaceUnlock {IDirect3DSurface9 *surface=nullptr;~SurfaceUnlock(){if(surface)surface->UnlockRect();}};
uint32_t save_surface_memory(const uint32_t *args){
    if(!args[0])return E_POINTER;write(args[0],0);
    if(args[1]!=0||!args[2])return D3DERR_INVALIDCALL; // D3DXIFF_BMP
    auto source=native_object<IDirect3DSurface9>(args[2]);D3DSURFACE_DESC desc{};
    HRESULT status=source->GetDesc(&desc);if(FAILED(status))return status;
    if(!desc.Width||!desc.Height||desc.Width>16384||desc.Height>16384)return D3DERR_INVALIDCALL;
    if(desc.Format!=D3DFMT_A8R8G8B8&&desc.Format!=D3DFMT_X8R8G8B8&&desc.Format!=D3DFMT_A8B8G8R8&&desc.Format!=D3DFMT_X8B8G8R8)return D3DERR_INVALIDCALL;
    RECT rect{0,0,LONG(desc.Width),LONG(desc.Height)};if(args[4])rect=*guest<RECT>(args[4]);
    if(rect.left<0||rect.top<0||rect.right<=rect.left||rect.bottom<=rect.top||uint32_t(rect.right)>desc.Width||uint32_t(rect.bottom)>desc.Height)return D3DERR_INVALIDCALL;
    uint32_t width=rect.right-rect.left,height=rect.bottom-rect.top,size=tf_bmp_size(width,height);
    if(!size||size>0x10000000u)return E_OUTOFMEMORY;
    std::vector<unsigned char> encoded(size);
    ComOwner<IDirect3DDevice9> device;ComOwner<IDirect3DSurface9> resolved,readback;
    IDirect3DSurface9 *read_source=source;D3DLOCKED_RECT locked{};
    status=read_source->LockRect(&locked,nullptr,D3DLOCK_READONLY);
    if(FAILED(status)){
        IDirect3DDevice9 *raw_device=nullptr;status=source->GetDevice(&raw_device);device.reset(raw_device);if(FAILED(status))return status;
        if(desc.MultiSampleType!=D3DMULTISAMPLE_NONE){
            IDirect3DSurface9 *raw=nullptr;status=device->CreateRenderTarget(desc.Width,desc.Height,desc.Format,D3DMULTISAMPLE_NONE,0,FALSE,&raw,nullptr);resolved.reset(raw);if(FAILED(status))return status;
            status=device->StretchRect(source,nullptr,resolved.get(),nullptr,D3DTEXF_NONE);if(FAILED(status))return status;read_source=resolved.get();
        }
        IDirect3DSurface9 *raw=nullptr;status=device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&raw,nullptr);readback.reset(raw);if(FAILED(status))return status;
        status=device->GetRenderTargetData(read_source,readback.get());if(FAILED(status))return status;
        read_source=readback.get();status=read_source->LockRect(&locked,nullptr,D3DLOCK_READONLY);if(FAILED(status))return status;
    }
    SurfaceUnlock unlock{read_source};
    if(locked.Pitch<=0||!tf_bmp_encode32(static_cast<const unsigned char*>(locked.pBits),uint64_t(locked.Pitch)*desc.Height,
        desc.Width,desc.Height,uint32_t(locked.Pitch),desc.Format,rect.left,rect.top,width,height,encoded.data(),encoded.size()))return D3DERR_INVALIDCALL;
    status=read_source->UnlockRect();unlock.surface=nullptr;if(FAILED(status))return status;
    auto buffer=std::make_unique<GuestBuffer>(size);std::memcpy(pointer(buffer->data,size),encoded.data(),size);
    uint32_t address=wrap(buffer.get(),Buffer);buffer.release();write(args[0],address);
    std::fprintf(stderr,"[d3dx9] saved surface %ux%u format=%u pool=%u -> BMP %u bytes\n",width,height,unsigned(desc.Format),unsigned(desc.Pool),size);
    return S_OK;
}
// Preserve the exact compiler input for offline compilation with the SDK.
// Unknown requests remain fatal; a capture never substitutes a shader.
std::vector<unsigned char> shader_request(const uint32_t *args) {
    if(!args[0]||!args[1]||args[1]>4*1024*1024)unsupported("invalid HLSL source size");
    if(args[3])unsupported("HLSL include callbacks are not implemented");
    std::vector<unsigned char> bytes{'T','F','H','L','S','L','0','1'};
    auto word=[&](uint32_t n){for(int i=0;i<4;++i)bytes.push_back((n>>(8*i))&255);};
    auto block=[&](uint32_t address,uint32_t size){word(size);auto p=guest<unsigned char>(address,size);bytes.insert(bytes.end(),p,p+size);};
    auto string=[&](uint32_t address){if(!address){word(0xffffffffu);return;}uint32_t n=0;while(n<65536&&*guest<char>(address+n))++n;if(n==65536)unsupported("unterminated HLSL parameter");block(address,n);};
    word(args[6]);block(args[0],args[1]);string(args[4]);string(args[5]);
    uint32_t macros=0;if(args[2]){while(macros<256&&read(args[2]+macros*8))++macros;if(macros==256)unsupported("too many HLSL macros");}
    word(macros);for(uint32_t i=0;i<macros;++i){string(read(args[2]+i*8));string(read(args[2]+i*8+4));}
    return bytes;
}
void capture_shader(const uint32_t *args,const std::vector<unsigned char>& bytes) {
    FILE *out=std::fopen("shader-request.bin.tmp","wb");if(!out)unsupported("cannot capture HLSL request");
    bool ok=std::fwrite(bytes.data(),1,bytes.size(),out)==bytes.size();ok=std::fclose(out)==0&&ok;
    if(!ok||std::rename("shader-request.bin.tmp","shader-request.bin"))unsupported("cannot commit HLSL capture");
    std::fprintf(stderr,"[d3dx] captured exact HLSL request: %zu bytes, flags=%08x, constant-table=%s\n",bytes.size(),args[6],args[9]?"yes":"no");
}
#include "d3d9_constants.inc"
HRESULT upload_dds(IDirect3DBaseTexture9 *texture,const TF_DDSInfo& info,const unsigned char *source,bool cpu_bc) {
    for(uint32_t face=0;face<info.faces;++face)for(uint32_t level=0;level<info.levels;++level){
        TF_DDSLevel layout{};if(!tf_dds_level(&info,face,level,&layout))unsupported("internal DDS subresource error");
        D3DLOCKED_RECT lock{};HRESULT result;
        if(info.faces==6)result=static_cast<IDirect3DCubeTexture9*>(texture)->LockRect(static_cast<D3DCUBEMAP_FACES>(face),level,&lock,nullptr,0);
        else result=static_cast<IDirect3DTexture9*>(texture)->LockRect(level,&lock,nullptr,0);
        if(FAILED(result))return result;
        if(!lock.pBits||lock.Pitch<=0||uint32_t(lock.Pitch)<(cpu_bc?layout.width*4:layout.pitch))unsupported("DDS native lock pitch is too small");
        if(cpu_bc){
            if(!tf_bc_decode(info.format,layout.width,layout.height,source+layout.offset,layout.size,
                static_cast<unsigned char*>(lock.pBits),uint32_t(lock.Pitch),uint64_t(lock.Pitch)*layout.height))unsupported("DDS BC decode failed");
        }else for(uint32_t row=0;row<layout.rows;++row)
            std::memcpy(static_cast<unsigned char*>(lock.pBits)+uint64_t(row)*lock.Pitch,source+layout.offset+uint64_t(row)*layout.pitch,layout.pitch);
        if(info.faces==6)result=static_cast<IDirect3DCubeTexture9*>(texture)->UnlockRect(static_cast<D3DCUBEMAP_FACES>(face),level);
        else result=static_cast<IDirect3DTexture9*>(texture)->UnlockRect(level);
        if(FAILED(result))return result;
    }return S_OK;
}
uint32_t dds_texture(const uint32_t *args,bool extended,bool cube) {
    uint32_t output=extended?args[14]:args[3];if(!output)return E_POINTER;write(output,0);
    if(!args[0]||!args[1]||!args[2])return E_INVALIDARG;
    auto device=native_object<IDirect3DDevice9>(args[0]);auto data=static_cast<const unsigned char*>(pointer(args[1],args[2]));
    TF_DDSInfo info{};std::vector<unsigned char> decoded_bmp;bool bmp=false;
    if(args[2]>=2&&data[0]=='B'&&data[1]=='M'){
        TF_BMPInfo image;if(cube||!tf_bmp_parse(data,args[2],&image))return D3DERR_INVALIDCALL;
        uint64_t size=uint64_t(image.width)*image.height*4;if(size>0x10000000u-128)return E_OUTOFMEMORY;
        decoded_bmp.resize(size_t(size)+128);
        if(!tf_bmp_decode(data,args[2],decoded_bmp.data()+128,image.width*4,size))return D3DERR_INVALIDCALL;
        info={image.width,image.height,1,21,1,4,0};data=decoded_bmp.data();bmp=true;
    }else if(!tf_dds_parse(data,args[2],&info))unsupported("D3DX texture supports bounded DDS/BMP images; this image is unsupported or truncated");
    if((info.faces==6)!=cube)unsupported("DDS image resource type does not match requested texture type");
    DWORD usage=0;D3DPOOL pool=D3DPOOL_MANAGED;
    if(extended){
        auto dimension=[](uint32_t request,uint32_t actual){return request==actual||request==0xffffffffu||request==0xfffffffeu;};
        if(!dimension(args[3],info.width)||!dimension(args[4],info.height))unsupported("D3DX DDS resizing is not implemented");
        if(args[5]&&args[5]!=0xffffffffu&&args[5]!=info.levels)unsupported("D3DX DDS mip chain generation/truncation is not implemented");
        if(args[7]&&args[7]!=0xffffffffu&&args[7]!=0xfffffffdu&&args[7]!=info.format)unsupported("D3DX DDS requested format conversion is not implemented");
        if(args[11]||args[13])unsupported("D3DX DDS color-key/palette conversion is not implemented");
        usage=args[6];pool=static_cast<D3DPOOL>(args[8]);
        if(args[12]){auto p=guest<uint32_t>(args[12],7);p[0]=info.width;p[1]=info.height;p[2]=1;p[3]=info.levels;p[4]=info.format;p[5]=cube?D3DRTYPE_CUBETEXTURE:D3DRTYPE_TEXTURE;p[6]=bmp?0:4;}
    }
    IDirect3DBaseTexture9 *texture=nullptr;HRESULT result;
    auto create=[&](D3DFORMAT format){
        if(cube){IDirect3DCubeTexture9 *value=nullptr;auto status=device->CreateCubeTexture(info.width,info.levels,usage,format,pool,&value,nullptr);texture=value;return status;}
        IDirect3DTexture9 *value=nullptr;auto status=device->CreateTexture(info.width,info.height,info.levels,usage,format,pool,&value,nullptr);texture=value;return status;
    };
    result=create(static_cast<D3DFORMAT>(info.format));bool cpu_bc=false;
    if(FAILED(result)&&info.block_bytes&&!(usage&(D3DUSAGE_RENDERTARGET|D3DUSAGE_DEPTHSTENCIL))){
        // BC formats are optional on Android GPUs. D3DX can choose an available
        // uncompressed destination; the generic guest CreateTexture path still
        // truthfully reports the native backend's support for its exact format.
        if(extended&&args[7]==0xfffffffdu)return result; // FROM_FILE forbids format substitution.
        result=create(D3DFMT_A8R8G8B8);cpu_bc=SUCCEEDED(result);
        if(cpu_bc)std::fprintf(stderr,"[d3dx9] DDS BC%u decoded on CPU to A8R8G8B8 (%ux%u, %u levels)\n",info.format==0x31545844u?1:info.format<=0x33545844u?2:3,info.width,info.height,info.levels);
    }
    if(FAILED(result))return result;
    result=upload_dds(texture,info,data,cpu_bc);if(FAILED(result)){texture->Release();return result;}
    write(output,wrap(texture,cube?Cube:Texture));return S_OK;
}
} // namespace

extern "C" void tf_d3d9_configure(void *base,uint64_t limit,uint32_t (*allocate)(uint32_t),
    void (*release)(uint32_t),void *(*window_handle)(void)) {
    Operation operation;
    if(!objects.empty())unsupported("configure called with live objects");
    if(!allocate||!release||!limit||limit>0x100000000ull)unsupported("invalid guest memory configuration");
    guest_base=reinterpret_cast<uintptr_t>(base);guest_limit=limit;allocate_guest=allocate;release_guest=release;window_callback=window_handle;
}
extern "C" void tf_d3d9_set_render_scale(uint32_t percent) {
    render_scale_percent=percent==50||percent==67||percent==100?percent:100;
}
extern "C" void tf_d3d9_set_frame_observer(void (*observer)(void)){frame_observer=observer;}
extern "C" void tf_d3d9_set_operation_guard(void (*enter)(void),void (*leave)(void)){
    operation_enter=enter;operation_leave=leave;
}
extern "C" void tf_d3d9_suspend_presenters(void){
    for(const auto& item:objects)if(item.second.references&&item.second.kind==Device)
        tf_android_suspend_device(static_cast<IDirect3DDevice9*>(item.second.native));
    for(const auto& item:objects)if(item.second.references&&item.second.kind==SwapChain)
        tf_android_suspend_swapchain(static_cast<IDirect3DSwapChain9*>(item.second.native));
    frame_stats={};
}
extern "C" uint32_t tf_d3d9_create(uint32_t sdk) {
    Operation operation;
    if(!allocate_guest)unsupported("bridge has not been configured");
    auto native=Direct3DCreate9(sdk);std::fprintf(stderr,"[d3d9] native Direct3DCreate9 SDK %u -> %s\n",sdk,native?"object":"null");
    return wrap(native,D3D9);
}
extern "C" void tf_d3d9_shutdown(void) {
    Operation operation;
    for(auto& entry:locks)release_guest(entry.second.guest);locks.clear();
    // Releasing only references acquired through the bridge avoids assuming
    // ownership of native resources held internally by other COM objects.
    for(auto it=objects.rbegin();it!=objects.rend();++it)for(uint32_t n=0;n<it->second.references;++n)it->second.native->Release();
    while(!objects.empty())retire(objects.begin()->first);
}
#include "d3d9_gpu_trace.inc"
extern "C" uint32_t tf_d3d9_dispatch(uint32_t kind,uint32_t method,const uint32_t *args,uint32_t *argument_count) {
    Operation operation_guard;
    if(!args||!argument_count||kind<1||kind>ConstantTable||method>=counts[kind])unsupported("invalid COM dispatch parameters");
    ++frame_stats.calls;
    if(kind==Device&&method>=81&&method<=84)++frame_stats.draws;
    auto& entry=find(args[0]);if(entry.kind!=kind)unsupported("COM vtable kind does not match handle");
    auto object=entry.native;
    if(method==0){
        *argument_count=3;if(!args[1]||!args[2])return E_POINTER;auto& iid=*guest<GUID>(args[1]);
        uint32_t target=iid_kind(iid,kind);if(!target){write(args[2],0);unsupported("QueryInterface for an unmapped interface");}
        void *result=nullptr;HRESULT status=object->QueryInterface(iid,&result);
        write(args[2],SUCCEEDED(status)?wrap(static_cast<IUnknown*>(result),target):0);return status;
    }
    if(method==1){*argument_count=1;object->AddRef();return ++entry.references;}
    if(method==2){
        *argument_count=1;auto first=locks.lower_bound(uint64_t(args[0])<<32);
        if(first!=locks.end()&&uint32_t(first->first>>32)==args[0])unsupported("Release of a resource with an outstanding lock");
        auto left=--entry.references;auto native_left=object->Release();
        // A texture subresource can delegate native references to its owner.
        // Keep a dormant identity wrapper while internal references remain.
        if(!left&&!native_left)retire(args[0]);return left;
    }
    if(kind==Buffer){auto b=static_cast<GuestBuffer*>(object);*argument_count=1;if(method==3)return b->data;if(method==4)return b->size;}
    if(kind==ConstantTable)return constant_dispatch(static_cast<GuestConstants*>(object),method,args,argument_count);
    validate_arrays(kind,method,args);
    if(kind==D3D9){auto d3d=static_cast<IDirect3D9*>(object);
        if(method==3){*argument_count=2;unsupported("RegisterSoftwareDevice callback");}
        if(method==5){*argument_count=4;if(!args[3])return D3DERR_INVALIDCALL;
            D3DADAPTER_IDENTIFIER9 identifier{};auto result=d3d->GetAdapterIdentifier(args[1],args[2],&identifier);
            if(SUCCEEDED(result))std::memcpy(pointer(args[3],1100),&identifier,1100);return result;}
        if(method==15){*argument_count=2;
            // HMONITOR is opaque. Never truncate the native host's pointer.
            // Host Win32 monitor APIs are not part of this bridge.
            if(d3d->GetAdapterMonitor(args[1]))unsupported("native monitor handle requires a Win32 monitor bridge");return 0;}
        if(method==16){
            *argument_count=7;if(!args[5]||!args[6])return D3DERR_INVALIDCALL;write(args[6],0);
            auto window=native_window();if(!window)return D3DERR_DEVICELOST;
            auto parameters=present_parameters(args[5]);apply_render_scale(parameters);IDirect3DDevice9 *device=nullptr;
            HRESULT result=d3d->CreateDevice(args[1],static_cast<D3DDEVTYPE>(args[2]),window,args[4],&parameters,&device);
            store_present(args[5],parameters);uint32_t address=SUCCEEDED(result)?wrap(device,Device):0;
            if(address)objects.at(address).guest_window=args[3];write(args[6],address);
            std::fprintf(stderr,"[d3d9] CreateDevice %ux%u scale=%u%% status=%08x\n",parameters.BackBufferWidth,parameters.BackBufferHeight,render_scale_percent,uint32_t(result));return result;
        }
    }
    if(kind==Device){auto device=static_cast<IDirect3DDevice9*>(object);
        // CreateDevice/Reset expose the reduced backbuffer dimensions to the
        // guest. GetViewport and SetViewport must use those same coordinates;
        // scaling SetViewport again shrinks the image into the top-left corner.
        if(method==47&&render_scale_percent<100){
            *argument_count=2;if(!args[1])return D3DERR_INVALIDCALL;
            auto viewport=guest<D3DVIEWPORT9>(args[1]);
            HRESULT result=device->SetViewport(viewport);
            static unsigned logged=0;
            if(logged<8){
                ++logged;
                std::fprintf(stderr,"[d3d9] viewport %u,%u %ux%u passed unchanged, status=%08x\n",
                    viewport->X,viewport->Y,viewport->Width,viewport->Height,uint32_t(result));
            }
            return result;
        }
        if(method>=81&&method<=84&&gpu_draw_trace_enabled()){
            constexpr uint32_t draw_arguments[]={4,7,5,9};*argument_count=draw_arguments[method-81];
            return gpu_trace_draw(device,method,args);
        }
        if(method==9){*argument_count=2;if(!args[1])return D3DERR_INVALIDCALL;pointer(args[1],16);D3DDEVICE_CREATION_PARAMETERS p{};
            HRESULT result=device->GetCreationParameters(&p);if(SUCCEEDED(result)){write(args[1],p.AdapterOrdinal);write(args[1]+4,p.DeviceType);
            write(args[1]+8,entry.guest_window);write(args[1]+12,p.BehaviorFlags);}return result;}
        if(method==13){*argument_count=3;if(!args[1]||!args[2])return D3DERR_INVALIDCALL;auto p=present_parameters(args[1]);apply_render_scale(p);IDirect3DSwapChain9 *swap=nullptr;
            HRESULT result=device->CreateAdditionalSwapChain(&p,&swap);store_present(args[1],p);write(args[2],SUCCEEDED(result)?wrap(swap,SwapChain):0);return result;}
        if(method==16){*argument_count=2;if(!native_window())return D3DERR_DEVICELOST;auto p=present_parameters(args[1]);apply_render_scale(p);auto result=device->Reset(&p);store_present(args[1],p);return result;}
        if(method==17){*argument_count=5;if(!native_window())return D3DERR_DEVICELOST;
            uint64_t begin=wall_time_ns();
            auto result=device->Present(guest<RECT>(args[1]),guest<RECT>(args[2]),native_window(),guest<RGNDATA>(args[4]));
            record_present(begin,result);return result;}
        if(method==80){*argument_count=1;unsupported("GetNPatchMode float return requires an x87 shim");}
    }
    if(kind==SwapChain){auto swap=static_cast<IDirect3DSwapChain9*>(object);
        if(method==3){*argument_count=6;if(!native_window())return D3DERR_DEVICELOST;
            uint64_t begin=wall_time_ns();
            auto result=swap->Present(guest<RECT>(args[1]),guest<RECT>(args[2]),native_window(),guest<RGNDATA>(args[4]),args[5]);
            record_present(begin,result);return result;}
        if(method==9){*argument_count=2;if(!args[1])return D3DERR_INVALIDCALL;D3DPRESENT_PARAMETERS p{};auto result=swap->GetPresentParameters(&p);
            if(SUCCEEDED(result)){std::memset(pointer(args[1],56),0,56);store_present(args[1],p);write(args[1]+32,TRUE);}return result;}
    }
    if(kind==Surface&&method==11){*argument_count=3;if(!args[1]||!args[2])return E_POINTER;auto& iid=*guest<GUID>(args[1]);uint32_t target=iid_kind(iid);
        if(!target)unsupported("GetContainer interface is not mapped");void *result=nullptr;auto status=static_cast<IDirect3DSurface9*>(object)->GetContainer(iid,&result);
        write(args[2],SUCCEEDED(status)?wrap(static_cast<IUnknown*>(result),target):0);return status;}
    if(kind==Surface&&(method==15||method==16))unsupported("surface GetDC/ReleaseDC requires a GDI bridge");
    if((kind==Texture&&method==19)||(kind==Cube&&method==19)||(kind==Surface&&method==13)){
        uint32_t level=kind==Surface?0:args[1],face=kind==Cube?args[1]:0;if(kind==Cube)level=args[2];
        uint32_t output=kind==Texture?args[2]:kind==Cube?args[3]:args[1];
        uint32_t rectangle=kind==Texture?args[3]:kind==Cube?args[4]:args[2],flags=kind==Texture?args[4]:kind==Cube?args[5]:args[3];
        uint32_t subresource=kind==Cube?face*32+level:level;*argument_count=kind==Texture?5:kind==Cube?6:4;
        if(!output||level>=32||face>=6||locks.count(lock_key(args[0],subresource)))return D3DERR_INVALIDCALL;pointer(output,8);
        const RECT *rect=guest<RECT>(rectangle);D3DSURFACE_DESC desc{};D3DLOCKED_RECT lock{};HRESULT result;
        if(kind==Texture){auto t=static_cast<IDirect3DTexture9*>(object);result=t->GetLevelDesc(level,&desc);if(FAILED(result))return result;result=t->LockRect(level,&lock,rect,flags);}
        else if(kind==Cube){auto t=static_cast<IDirect3DCubeTexture9*>(object);result=t->GetLevelDesc(level,&desc);if(FAILED(result))return result;result=t->LockRect(static_cast<D3DCUBEMAP_FACES>(face),level,&lock,rect,flags);}
        else {auto s=static_cast<IDirect3DSurface9*>(object);result=s->GetDesc(&desc);if(FAILED(result))return result;result=s->LockRect(&lock,rect,flags);}
        return FAILED(result)?result:stage_pixels(args[0],subresource,desc,lock,rect,flags,output);
    }
    if((kind==Texture&&method==20)||(kind==Cube&&method==20)||(kind==Surface&&method==14)){
        *argument_count=kind==Texture?2:kind==Cube?3:1;uint32_t subresource=kind==Texture?args[1]:kind==Cube?args[1]*32+args[2]:0;
        copy_lock(args[0],subresource);HRESULT result=kind==Texture?static_cast<IDirect3DTexture9*>(object)->UnlockRect(args[1]):
            kind==Cube?static_cast<IDirect3DCubeTexture9*>(object)->UnlockRect(static_cast<D3DCUBEMAP_FACES>(args[1]),args[2]):static_cast<IDirect3DSurface9*>(object)->UnlockRect();
        free_lock(args[0],subresource);return result;
    }
    if((kind==VertexBuffer||kind==IndexBuffer)&&method==11){*argument_count=5;return kind==VertexBuffer?lock_buffer<IDirect3DVertexBuffer9,D3DVERTEXBUFFER_DESC>(args[0],args):lock_buffer<IDirect3DIndexBuffer9,D3DINDEXBUFFER_DESC>(args[0],args);}
    if((kind==VertexBuffer||kind==IndexBuffer)&&method==12){*argument_count=1;copy_lock(args[0],0);auto result=kind==VertexBuffer?static_cast<IDirect3DVertexBuffer9*>(object)->Unlock():static_cast<IDirect3DIndexBuffer9*>(object)->Unlock();free_lock(args[0],0);return result;}
    if((kind==VolumeTexture&&(method==18||method==19||method==20))||(kind==Volume&&(method==7||method==9||method==10)))
        unsupported("volume pointer/lock marshalling is not implemented");
#include "d3d9_methods.generated.inc"
    unsupported("unimplemented kind "+std::to_string(kind)+" slot "+std::to_string(method));
}
extern "C" uint32_t tf_d3dx9_dispatch(uint32_t operation,const uint32_t *args,uint32_t *argument_count) {
    Operation operation_guard;
    switch(operation){
    case 0:*argument_count=5;if(!args[0])return 0;tf_matrix_ortho_lh(guest<float>(args[0],16),float_value(args[1]),float_value(args[2]),float_value(args[3]),float_value(args[4]));return args[0];
    case 1:*argument_count=3;if(!args[0]||!args[1]||!args[2])return 0;tf_matrix_multiply(guest<float>(args[0],16),guest<float>(args[1],16),guest<float>(args[2],16));return args[0];
    case 2:*argument_count=2;if(!args[1])return E_POINTER;write(args[1],create_buffer(args[0]));return S_OK;
    case 3:*argument_count=10;return compiled_shader(args);
    case 4:*argument_count=7;unsupported("D3DXCreateEffectCompiler effect compiler is unavailable");
    case 5:*argument_count=4;return dds_texture(args,false,false);
    case 6:*argument_count=15;return dds_texture(args,true,false);
    case 7:*argument_count=4;return dds_texture(args,false,true);
    case 8:*argument_count=5;return save_surface_memory(args);
    default:unsupported("invalid D3DX operation");}
}
#include "d3d9_image_test.inc"
