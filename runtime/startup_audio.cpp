/* DirectSound guest32 -> host PCM -> AAudio. Original game code supplies the
 * samples, including WAV/packed-DAT reads. No replacement soundtrack.
 * The realtime callback only consumes a preallocated SPSC ring: it never
 * enters guest code, allocates, logs, or takes the mixer/machine mutex. */
#include "startup_audio.h"
#include "audio_mixer.h"
#include "audio_contract.h"
#include <aaudio/AAudio.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
extern "C" {
#include "win32hle.h"
}

namespace {
constexpr uint32_t OK=0,INVALID=0x80070057u,NOIFACE=0x80004002u,
    NODRIVER=0x88780078u,NOMEM=0x8007000eu,UNSUPPORTED=0x80004001u,
    BADFORMAT=0x88780064u,INVALIDCALL=0x88780032u;
enum {DEVICE=1,BUFFER,LISTENER,SPATIAL,NOTIFY};
constexpr uint32_t counts[]={0,12,24,18,21,4};
const uint32_t unknown_iid[]={0,0,0xc0,0x46000000};
const uint32_t device_iid[]={0x279afa83,0x11ce4981,0x200021a5,0x60e50baf};
const uint32_t device8_iid[]={0xc50a7e93,0x4834f395,0xa97ff69e,0x6609e59d};
const uint32_t buffer_iid[]={0x279afa85,0x11ce4981,0x200021a5,0x60e50baf};
const uint32_t buffer8_iid[]={0x6825a449,0x4d827524,0xe3500f92,0x1eabb36a};
const uint32_t listener_iid[]={0x279afa84,0x11ce4981,0x200021a5,0x60e50baf};
const uint32_t spatial_iid[]={0x279afa86,0x11ce4981,0x200021a5,0x60e50baf};
const uint32_t notify_iid[]={0xb0210783,0x11d089cd,0xa00008af,0x16cd25c9};
struct Vector {float x=0,y=0,z=0;};
struct Listener {
    uint32_t size=64;Vector position,velocity,front{0,0,1},top{0,1,0};
    float distance=1,rolloff=1,doppler=1;
};
struct Spatial {
    uint32_t size=64;Vector position,velocity;
    uint32_t inside=360,outside=360;Vector cone{0,0,1};
    int32_t outside_volume=0;float minimum=1,maximum=1e9f;uint32_t mode=0;
};
static_assert(sizeof(Listener)==64&&sizeof(Spatial)==64,"guest DS3D layout");
struct Storage {
    uint32_t guest=0;std::vector<uint8_t> pcm;
    ~Storage(){if(guest)hle_free(hle_ptr(guest));}
};
struct Notification {uint32_t offset,event;};
struct Core {
    uint32_t kind=0,refs=1,flags=0,views[6]{};
    TF_AudioFormat format{2,44100,16,4,176400};
    TF_AudioVoice voice{};std::shared_ptr<Storage> storage;
    int32_t volume=0,pan=0;uint32_t frequency=44100;
    bool primary=false,locked=false,just_started=false,spatial_pending=false;
    uint64_t rendered_since_start=0;
    TF_AudioLock lock{};Spatial spatial,queued_spatial;
    std::vector<Notification> notifications;
};
struct View {Core* core;uint32_t kind;};
std::map<uint32_t,std::unique_ptr<Core>> cores;
std::map<uint32_t,View> views;
// Loaded sample assets are not playback voices. The game preloads hundreds
// of sources, then duplicates them into its separate 64-voice playback pool.
std::vector<Core*> buffers,active_buffers;
uint32_t vtables[6]{},method_addresses[6][24]{};
char method_names[6][24][40]{};
std::mutex mixer_mutex;
Listener listener,queued_listener;bool listener_pending=false;
float primary_gain=1;
AAudioStream* stream=nullptr;
std::thread mixer_thread;
std::atomic<bool> worker_stop{true};
constexpr uint32_t RING=8192;
float ring[RING*2]{};
std::atomic<uint32_t> ring_read{0},ring_write{0},dropped_frames{0},callbacks{0};
std::atomic<int32_t> stream_error{0};
std::atomic<float> focus_gain{1};
std::atomic<bool> session_suspended{false};
uint32_t output_rate=48000,queue_target=512;
uint64_t mixed_nonzero=0,clipped_samples=0;float mixed_peak=0;
uint64_t sfx_nonzero=0,sfx_plays=0,duplicates=0,create_failures=0,duplicate_failures=0;
float sfx_peak=0;
static_assert(std::atomic<uint32_t>::is_always_lock_free&&std::atomic<float>::is_always_lock_free,"AAudio callback atomics");

bool span(uint32_t va,uint64_t n){return va&&n<=UINT32_MAX-va;}
void* ptr(uint32_t va,uint64_t n){return span(va,n)?hle_ptr(va):nullptr;}
bool guid(uint32_t va,const uint32_t* expected){auto p=ptr(va,16);return p&&!std::memcmp(p,expected,16);}
float number(uint32_t bits){float f;std::memcpy(&f,&bits,4);return f;}
uint32_t bits(float f){uint32_t u;std::memcpy(&u,&f,4);return u;}
bool finite(Vector v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
float length(Vector v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
float decibels(int32_t value){return value<=-10000?0.f:std::pow(10.f,float(value)/2000.f);}
void update_gains(Core& c){
    float gain=decibels(c.volume)*primary_gain,left=1,right=1;
    uint32_t frequency=c.frequency;
    if((c.flags&0x10)&&c.spatial.mode!=2){
        auto& s=c.spatial;Vector delta=s.position;
        if(s.mode==0){delta.x-=listener.position.x;delta.y-=listener.position.y;delta.z-=listener.position.z;}
        float distance=length(delta),pan=0;
        if(distance>1e-9f){
            Vector axis{1,0,0};
            if(s.mode==0){axis={listener.top.y*listener.front.z-listener.top.z*listener.front.y,
                listener.top.z*listener.front.x-listener.top.x*listener.front.z,
                listener.top.x*listener.front.y-listener.top.y*listener.front.x};
                float n=length(axis);if(n>1e-9f){axis.x/=n;axis.y/=n;axis.z/=n;}}
            pan=std::clamp((delta.x*axis.x+delta.y*axis.y+delta.z*axis.z)/distance,-1.f,1.f);
        }
        float minimum=std::max(s.minimum,1e-6f),maximum=std::max(s.maximum,minimum);
        if((c.flags&0x20000)&&distance>=maximum)gain=0;
        float attenuation_distance=std::clamp(distance,minimum,maximum);
        gain*=minimum/(minimum+listener.rolloff*(attenuation_distance-minimum));
        if(distance>1e-9f&&s.outside<360&&s.outside_volume<0){
            float cone_length=length(s.cone);
            if(cone_length>1e-9f){float cosine=std::clamp(-(s.cone.x*delta.x+s.cone.y*delta.y+s.cone.z*delta.z)/(cone_length*distance),-1.f,1.f);
                float angle=std::acos(cosine)*57.2957795f,inside=s.inside*.5f,outside=std::max(inside,s.outside*.5f);
                if(angle>inside){float blend=outside>inside?std::min(1.f,(angle-inside)/(outside-inside)):1.f;gain*=decibels(int32_t(s.outside_volume*blend));}}
        }
        left=std::sqrt(std::max(0.f,(1-pan)*.5f));right=std::sqrt(std::max(0.f,(1+pan)*.5f));
        if(distance>1e-6f&&listener.doppler>0){
            float lv=s.mode==1?0:(listener.velocity.x*delta.x+listener.velocity.y*delta.y+listener.velocity.z*delta.z)/distance;
            float sv=(s.velocity.x*delta.x+s.velocity.y*delta.y+s.velocity.z*delta.z)/distance;
            float factor=std::clamp((343.3f+lv*listener.distance*listener.doppler)/
                std::max(34.33f,343.3f+sv*listener.distance*listener.doppler),.25f,4.f);
            frequency=uint32_t(std::clamp(c.frequency*factor,100.f,200000.f));
        }
    }else{
        if(c.pan<0)right=decibels(c.pan);
        else if(c.pan>0)left=decibels(-c.pan);
    }
    c.voice.gain_l=gain*left;c.voice.gain_r=gain*right;c.voice.frequency=frequency;
}
void queue_event(uint32_t event,std::array<uint32_t,512>& events,uint32_t& count){
    if(!event)return;for(uint32_t i=0;i<count;++i)if(events[i]==event)return;
    if(count<events.size())events[count++]=event;
}
void notifications(Core& c,double old,double advanced,bool was_playing,
                   std::array<uint32_t,512>& events,uint32_t& count){
    uint32_t frames=c.voice.bytes/c.format.block;
    for(auto& n:c.notifications){
        if(n.offset==UINT32_MAX){if(was_playing&&!c.voice.playing)queue_event(n.event,events,count);}
        else if(tf_audio_crossed(old,advanced,frames,n.offset/c.format.block,c.voice.looping,c.just_started&&old==0))
            queue_event(n.event,events,count);
    }
    c.just_started=false;
}
void worker(){
    float mixed[256*2],sfx[256*2];
    while(!worker_stop.load(std::memory_order_acquire)){
        uint32_t write=ring_write.load(std::memory_order_relaxed),read=ring_read.load(std::memory_order_acquire);
        uint32_t queued=write-read;
        if(queued>=queue_target){std::this_thread::sleep_for(std::chrono::milliseconds(1));continue;}
        uint32_t frames=std::min(256u,queue_target-queued),lost=dropped_frames.exchange(0);
        std::fill(mixed,mixed+frames*2,0.f);
        std::fill(sfx,sfx+frames*2,0.f);
        std::array<uint32_t,512> events{};uint32_t event_count=0;
        {
            std::lock_guard<std::mutex> lock(mixer_mutex);
            for(auto* c:active_buffers){
                if(!c->voice.playing)continue;
                update_gains(*c);
                if(lost&&c->rendered_since_start){double old=c->voice.position;bool was=c->voice.playing;
                    uint32_t consumed=tf_audio_advance_voice(&c->voice,lost,output_rate);
                    notifications(*c,old,double(consumed)*c->voice.frequency/output_rate,was,events,event_count);}
                double old=c->voice.position;bool was=c->voice.playing;
                uint32_t consumed=tf_audio_mix_voice(&c->voice,(c->flags&0x100)?mixed:sfx,frames,output_rate);
                c->rendered_since_start+=consumed;
                if(was)notifications(*c,old,double(consumed)*c->voice.frequency/output_rate,was,events,event_count);
            }
            active_buffers.erase(std::remove_if(active_buffers.begin(),active_buffers.end(),
                [](Core* c){return !c->voice.playing;}),active_buffers.end());
            for(uint32_t i=0;i<frames*2;++i){
                float effect=std::abs(sfx[i]);sfx_peak=std::max(sfx_peak,effect);if(effect>0.00001f)++sfx_nonzero;
                mixed[i]+=sfx[i];float amplitude=std::abs(mixed[i]);mixed_peak=std::max(mixed_peak,amplitude);if(amplitude>0.00001f)++mixed_nonzero;}
            clipped_samples+=tf_audio_clip(mixed,frames);
        }
        for(uint32_t f=0;f<frames;++f){uint32_t at=(write+f)&(RING-1);ring[2*at]=mixed[2*f];ring[2*at+1]=mixed[2*f+1];}
        ring_write.store(write+frames,std::memory_order_release);
        // Only ordinary worker code may enter the serialized guest machine.
        // Never hold mixer_mutex here: the game takes machine -> mixer order.
        if(event_count){mach_enter();for(uint32_t i=0;i<event_count;++i)hle_set_event(events[i]);mach_leave();}
    }
}
aaudio_data_callback_result_t callback(AAudioStream*,void*,void* output,int32_t count){
    if(count<=0)return AAUDIO_CALLBACK_RESULT_CONTINUE;
    auto* dest=static_cast<float*>(output);uint32_t frames=uint32_t(count);
    if(session_suspended.load(std::memory_order_relaxed)){
        std::fill(dest,dest+frames*2,0.f);return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }
    uint32_t read=ring_read.load(std::memory_order_relaxed),write=ring_write.load(std::memory_order_acquire);
    uint32_t available=std::min(frames,write-read);float gain=focus_gain.load(std::memory_order_relaxed);
    for(uint32_t f=0;f<available;++f){uint32_t at=(read+f)&(RING-1);dest[2*f]=ring[2*at]*gain;dest[2*f+1]=ring[2*at+1]*gain;}
    std::fill(dest+available*2,dest+frames*2,0.f);
    ring_read.store(read+available,std::memory_order_release);
    if(frames>available)dropped_frames.fetch_add(frames-available,std::memory_order_relaxed);
    callbacks.fetch_add(1,std::memory_order_relaxed);return AAUDIO_CALLBACK_RESULT_CONTINUE;
}
void error_callback(AAudioStream*,void*,aaudio_result_t error){stream_error.store(error,std::memory_order_relaxed);}
void close_sink(){
    worker_stop.store(true,std::memory_order_release);
    // A notification may be waiting for the machine held by this caller.
    hle_block_begin();
    if(mixer_thread.joinable())mixer_thread.join();
    if(stream){AAudioStream_requestStop(stream);AAudioStream_close(stream);stream=nullptr;}
    hle_block_end();
}
bool open_sink(){
    if(stream)return true;
    static bool cleanup_registered=false;
    if(!cleanup_registered){std::atexit(tf_audio_shutdown);cleanup_registered=true;}
    AAudioStreamBuilder* builder=nullptr;
    auto result=AAudio_createStreamBuilder(&builder);if(result!=AAUDIO_OK)return false;
    AAudioStreamBuilder_setDirection(builder,AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setFormat(builder,AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setChannelCount(builder,2);
    AAudioStreamBuilder_setSharingMode(builder,AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setPerformanceMode(builder,AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setUsage(builder,AAUDIO_USAGE_GAME);
    AAudioStreamBuilder_setContentType(builder,AAUDIO_CONTENT_TYPE_MUSIC);
    AAudioStreamBuilder_setDataCallback(builder,callback,nullptr);
    AAudioStreamBuilder_setErrorCallback(builder,error_callback,nullptr);
    result=AAudioStreamBuilder_openStream(builder,&stream);AAudioStreamBuilder_delete(builder);
    if(result!=AAUDIO_OK){stream=nullptr;std::fprintf(stderr,"[audio] AAudio open failed: %s\n",AAudio_convertResultToText(result));return false;}
    if(AAudioStream_getFormat(stream)!=AAUDIO_FORMAT_PCM_FLOAT||AAudioStream_getChannelCount(stream)!=2){AAudioStream_close(stream);stream=nullptr;return false;}
    output_rate=uint32_t(AAudioStream_getSampleRate(stream));
    int32_t burst=AAudioStream_getFramesPerBurst(stream);
    queue_target=std::clamp(burst>0?uint32_t(burst)*2:512u,256u,2048u);
    if(burst>0)AAudioStream_setBufferSizeInFrames(stream,burst*2);
    ring_read=0;ring_write=0;dropped_frames=0;stream_error=0;callbacks=0;
    worker_stop=false;
    try{mixer_thread=std::thread(worker);}catch(...){AAudioStream_close(stream);stream=nullptr;worker_stop=true;return false;}
    result=AAudioStream_requestStart(stream);
    if(result!=AAUDIO_OK){close_sink();return false;}
    std::fprintf(stderr,"[audio] AAudio started: stereo float %u Hz, burst=%d, producer_lead=%u frames\n",output_rate,burst,queue_target);
    return true;
}
void ensure_vtables(){
    if(vtables[DEVICE])return;
    for(uint32_t kind=DEVICE;kind<=NOTIFY;++kind){
        vtables[kind]=hle_va(hle_alloc(counts[kind]*4));if(!vtables[kind])throw std::bad_alloc();
        for(uint32_t slot=0;slot<counts[kind];++slot)MEM32(vtables[kind]+slot*4)=method_addresses[kind][slot];
    }
}
uint32_t view(Core& core,uint32_t kind){
    if(core.views[kind])return core.views[kind];
    uint32_t va=hle_va(hle_alloc(4));if(!va)throw std::bad_alloc();MEM32(va)=vtables[kind];
    try{views.emplace(va,View{&core,kind});}catch(...){hle_free(hle_ptr(va));throw;}
    core.views[kind]=va;return va;
}
uint32_t insert(std::unique_ptr<Core> core){
    uint32_t va=view(*core,core->kind);
    try{cores.emplace(va,std::move(core));}catch(...){views.erase(va);hle_free(hle_ptr(va));throw;}
    return va;
}
void release(Core& core){
    active_buffers.erase(std::remove(active_buffers.begin(),active_buffers.end(),&core),active_buffers.end());
    buffers.erase(std::remove(buffers.begin(),buffers.end(),&core),buffers.end());
    uint32_t canonical=core.views[core.kind];
    for(uint32_t kind=DEVICE;kind<=NOTIFY;++kind)if(core.views[kind]){views.erase(core.views[kind]);hle_free(hle_ptr(core.views[kind]));}
    cores.erase(canonical);
}
uint32_t queued_frames(){
    uint32_t read=ring_read.load(std::memory_order_acquire),write=ring_write.load(std::memory_order_acquire);
    return std::min(write-read,RING);
}
uint32_t play_cursor(Core& c);
uint32_t write_cursor(Core& c){
    uint32_t bytes=c.voice.bytes;if(!bytes)return 0;
    uint32_t play=play_cursor(c);
    uint32_t lead=(c.frequency/100)*c.format.block;
    return (play+(c.voice.playing?lead:0))%bytes;
}
uint32_t play_cursor(Core& c){
    if(!c.voice.bytes)return 0;
    double position=c.voice.position;
    if(c.voice.playing){
        uint32_t queued=uint32_t(std::min(uint64_t(queued_frames()),c.rendered_since_start));
        double lead=double(queued)*c.voice.frequency/output_rate;
        if(c.voice.looping)position=tf_audio_wrap(position+c.voice.bytes/c.format.block-tf_audio_wrap(lead,c.voice.bytes/c.format.block),c.voice.bytes/c.format.block);
        else position=std::max(0.0,position-lead);
    }
    return uint32_t(position)*c.format.block%c.voice.bytes;
}
uint32_t make_buffer(uint32_t desc,uint32_t output,uint32_t outer){
    if(!ptr(output,4))return INVALID;MEM32(output)=0;
    if(outer)return 0x80040110u;
    if(!ptr(desc,20)||(MEM32(desc)!=20&&MEM32(desc)!=36))return INVALID;
    uint32_t flags=MEM32(desc+4),bytes=MEM32(desc+8),format=MEM32(desc+16);
    auto c=std::make_unique<Core>();c->kind=BUFFER;c->flags=flags;c->primary=flags&1;
    if(!c->primary){
        if(!ptr(format,16)||!tf_audio_format(static_cast<uint8_t*>(hle_ptr(format)),16,&c->format))return BADFORMAT;
        if(!bytes||bytes>32*1024*1024||bytes%c->format.block||((flags&0x10)&&c->format.channels!=1))return INVALID;
        // Reserve before publishing a COM object: registry growth must either
        // succeed entirely or leave no orphaned guest view behind.
        if(buffers.size()==buffers.capacity())buffers.reserve(std::max(size_t(64),buffers.size()*2));
        c->storage=std::make_shared<Storage>();c->storage->pcm.assign(bytes,c->format.bits==8?128:0);
        c->storage->guest=hle_va(hle_alloc(bytes));if(!c->storage->guest)return NOMEM;
        std::memcpy(hle_ptr(c->storage->guest),c->storage->pcm.data(),bytes);
        c->voice.samples=c->storage->pcm.data();c->voice.bytes=bytes;c->voice.channels=c->format.channels;
        c->voice.bits=c->format.bits;c->voice.source_rate=c->format.rate;c->voice.frequency=c->format.rate;
        c->voice.gain_l=c->voice.gain_r=1;c->frequency=c->format.rate;
    }
    auto* core=c.get();uint32_t va=insert(std::move(c));if(!core->primary)buffers.push_back(core);
    MEM32(output)=va;static uint32_t logged=0;
    if(logged++<12)std::fprintf(stderr,"[audio] buffer flags=%08x bytes=%u PCM %uch %uHz %ubit\n",flags,bytes,core->format.channels,core->format.rate,core->format.bits);
    return OK;
}
uint32_t query(Core& c,uint32_t iid,uint32_t output){
    if(!ptr(output,4))return INVALID;MEM32(output)=0;
    uint32_t target=0;
    if(guid(iid,unknown_iid))target=c.kind;
    if(c.kind==DEVICE&&(guid(iid,device_iid)||guid(iid,device8_iid)))target=DEVICE;
    if(c.kind==BUFFER){
        if(guid(iid,buffer_iid)||guid(iid,buffer8_iid))target=BUFFER;
        if(c.primary&&(c.flags&0x10)&&guid(iid,listener_iid))target=LISTENER;
        if(!c.primary&&(c.flags&0x10)&&guid(iid,spatial_iid))target=SPATIAL;
        if(!c.primary&&(c.flags&0x100)&&guid(iid,notify_iid))target=NOTIFY;
    }
    if(!target)return NOIFACE;
    MEM32(output)=view(c,target);++c.refs;return OK;
}
uint32_t listener_call(uint32_t method,const uint32_t* a){
    if(method==17){if(listener_pending){listener=queued_listener;listener_pending=false;}
        for(auto* c:buffers)if(c->spatial_pending){c->spatial=c->queued_spatial;c->spatial_pending=false;}return OK;}
    if(method<=9){
        if(method==3){if(!ptr(a[1],64)||MEM32(a[1])!=64)return INVALID;std::memcpy(hle_ptr(a[1]),&listener,64);return OK;}
        if(method==6){if(!ptr(a[1],12)||!ptr(a[2],12))return INVALID;std::memcpy(hle_ptr(a[1]),&listener.front,12);std::memcpy(hle_ptr(a[2]),&listener.top,12);return OK;}
        if(method==7||method==9){if(!ptr(a[1],12))return INVALID;std::memcpy(hle_ptr(a[1]),method==7?&listener.position:&listener.velocity,12);return OK;}
        if(!ptr(a[1],4))return INVALID;MEM32(a[1])=bits(method==4?listener.distance:method==5?listener.doppler:listener.rolloff);return OK;
    }
    uint32_t apply=method==10?a[2]:method==13?a[7]:(method==14||method==16)?a[4]:a[2];if(apply>1)return INVALID;
    Listener next=apply&&listener_pending?queued_listener:listener;
    if(method==10){if(!ptr(a[1],64)||MEM32(a[1])!=64)return INVALID;std::memcpy(&next,hle_ptr(a[1]),64);}
    if(method==11)next.distance=number(a[1]);if(method==12)next.doppler=number(a[1]);
    if(method==13){next.front={number(a[1]),number(a[2]),number(a[3])};next.top={number(a[4]),number(a[5]),number(a[6])};}
    if(method==14)next.position={number(a[1]),number(a[2]),number(a[3])};
    if(method==15)next.rolloff=number(a[1]);if(method==16)next.velocity={number(a[1]),number(a[2]),number(a[3])};
    if(!finite(next.position)||!finite(next.velocity)||!finite(next.front)||!finite(next.top)||!std::isfinite(next.distance)||next.distance<=0||!std::isfinite(next.rolloff)||next.rolloff<0||!std::isfinite(next.doppler)||next.doppler<0)return INVALID;
    if(apply){queued_listener=next;listener_pending=true;}
    else{
        listener=next;
        if(listener_pending){switch(method){
        case 10:queued_listener=next;break;
        case 11:queued_listener.distance=next.distance;break;
        case 12:queued_listener.doppler=next.doppler;break;
        case 13:queued_listener.front=next.front;queued_listener.top=next.top;break;
        case 14:queued_listener.position=next.position;break;
        case 15:queued_listener.rolloff=next.rolloff;break;
        case 16:queued_listener.velocity=next.velocity;break;
        }}
    }
    return OK;
}
uint32_t spatial_call(Core& c,uint32_t method,const uint32_t* a){
    auto& s=c.spatial;
    if(method<=11){
        if(method==3){if(!ptr(a[1],64)||MEM32(a[1])!=64)return INVALID;std::memcpy(hle_ptr(a[1]),&s,64);return OK;}
        if(method==4){if(!ptr(a[1],4)||!ptr(a[2],4))return INVALID;MEM32(a[1])=s.inside;MEM32(a[2])=s.outside;return OK;}
        if(method==5||method==10||method==11){if(!ptr(a[1],12))return INVALID;std::memcpy(hle_ptr(a[1]),method==5?&s.cone:method==10?&s.position:&s.velocity,12);return OK;}
        if(!ptr(a[1],4))return INVALID;MEM32(a[1])=method==6?uint32_t(s.outside_volume):method==7?bits(s.maximum):method==8?bits(s.minimum):s.mode;return OK;
    }
    uint32_t apply=(method==14||method==19||method==20)?a[4]:method==13?a[3]:a[2];if(apply>1)return INVALID;
    Spatial next=apply&&c.spatial_pending?c.queued_spatial:s;
    if(method==12){if(!ptr(a[1],64)||MEM32(a[1])!=64)return INVALID;std::memcpy(&next,hle_ptr(a[1]),64);}
    if(method==13){next.inside=a[1];next.outside=a[2];}
    if(method==14)next.cone={number(a[1]),number(a[2]),number(a[3])};
    if(method==15)next.outside_volume=int32_t(a[1]);if(method==16)next.maximum=number(a[1]);
    if(method==17)next.minimum=number(a[1]);if(method==18)next.mode=a[1];
    if(method==19)next.position={number(a[1]),number(a[2]),number(a[3])};if(method==20)next.velocity={number(a[1]),number(a[2]),number(a[3])};
    if(!finite(next.position)||!finite(next.velocity)||!finite(next.cone)||next.mode>2||next.inside>360||next.outside>360||next.outside_volume>0||next.outside_volume< -10000||!std::isfinite(next.minimum)||!std::isfinite(next.maximum)||next.minimum<=0||next.maximum<=0)return INVALID;
    if(apply){c.queued_spatial=next;c.spatial_pending=true;}
    else{
        s=next;
        if(c.spatial_pending){switch(method){
        case 12:c.queued_spatial=next;break;
        case 13:c.queued_spatial.inside=next.inside;c.queued_spatial.outside=next.outside;break;
        case 14:c.queued_spatial.cone=next.cone;break;
        case 15:c.queued_spatial.outside_volume=next.outside_volume;break;
        case 16:c.queued_spatial.maximum=next.maximum;break;
        case 17:c.queued_spatial.minimum=next.minimum;break;
        case 18:c.queued_spatial.mode=next.mode;break;
        case 19:c.queued_spatial.position=next.position;break;
        case 20:c.queued_spatial.velocity=next.velocity;break;
        }}
    }
    return OK;
}
uint32_t buffer_call(Core& c,uint32_t method,const uint32_t* a){
    uint32_t bytes=c.voice.bytes;
    switch(method){
    case 3:if(!ptr(a[1],20)||MEM32(a[1])!=20)return INVALID;MEM32(a[1]+4)=(c.flags&~4u)|8u;MEM32(a[1]+8)=bytes;MEM32(a[1]+12)=MEM32(a[1]+16)=0;return OK;
    case 4:if(a[1]){if(!ptr(a[1],4))return INVALID;MEM32(a[1])=play_cursor(c);}if(a[2]){if(!ptr(a[2],4))return INVALID;MEM32(a[2])=write_cursor(c);}return OK;
    case 5:{if(a[3]){if(!ptr(a[3],4))return INVALID;MEM32(a[3])=18;}if(!a[1])return a[3]?OK:INVALID;if(a[2]<18||!ptr(a[1],18))return INVALID;
        MEM16(a[1])=1;MEM16(a[1]+2)=c.format.channels;MEM32(a[1]+4)=c.format.rate;MEM32(a[1]+8)=c.format.average;MEM16(a[1]+12)=c.format.block;MEM16(a[1]+14)=c.format.bits;MEM16(a[1]+16)=0;return OK;}
    case 6:case 7:case 8:case 9:if(!ptr(a[1],4))return INVALID;MEM32(a[1])=method==6?uint32_t(c.volume):method==7?uint32_t(c.pan):method==8?c.frequency:c.voice.playing?(1u|(c.voice.looping?4u:0u)):0;return OK;
    case 10:return INVALIDCALL;
    case 11:{if(c.primary||!c.storage||c.locked||!ptr(a[3],4)||!ptr(a[4],4))return INVALID;
        TF_AudioLock range;if(!tf_audio_lock_range(bytes,a[1],a[2],a[7],write_cursor(c),&range))return INVALID;
        if(range.second&&(!ptr(a[5],4)||!ptr(a[6],4)))return INVALID;
        MEM32(a[3])=c.storage->guest+range.offset;MEM32(a[4])=range.first;
        if(a[5]){if(!ptr(a[5],4))return INVALID;MEM32(a[5])=range.second?c.storage->guest:0;}
        if(a[6]){if(!ptr(a[6],4))return INVALID;MEM32(a[6])=range.second;}
        c.lock=range;c.locked=true;return OK;}
    case 12:if(a[1]||a[3]&~1u)return INVALID;
        if(!c.primary&&!c.voice.playing){active_buffers.push_back(&c);c.rendered_since_start=0;}
        if(!c.primary&&!(c.flags&0x100))++sfx_plays;
        c.voice.looping=a[3]&1;c.voice.playing=!c.primary;c.just_started=true;return OK;
    case 13:if(c.primary||!bytes||a[1]>=bytes||a[1]%c.format.block)return INVALID;c.voice.position=a[1]/c.format.block;c.rendered_since_start=0;c.just_started=true;return OK;
    case 14:if(!c.primary||!ptr(a[1],16)||!tf_audio_format(static_cast<uint8_t*>(hle_ptr(a[1])),16,&c.format))return BADFORMAT;c.frequency=c.format.rate;return OK;
    case 15:if(int32_t(a[1])< -10000||int32_t(a[1])>0)return INVALID;c.volume=int32_t(a[1]);if(c.primary)primary_gain=decibels(c.volume);return OK;
    case 16:if(c.flags&0x10)return INVALIDCALL;if(int32_t(a[1])< -10000||int32_t(a[1])>10000)return INVALID;c.pan=int32_t(a[1]);return OK;
    case 17:if(a[1]&&(a[1]<100||a[1]>200000))return INVALID;c.frequency=a[1]?a[1]:c.format.rate;return OK;
    case 18:c.voice.playing=false;active_buffers.erase(std::remove(active_buffers.begin(),active_buffers.end(),&c),active_buffers.end());for(auto& n:c.notifications)if(n.offset==UINT32_MAX)hle_set_event(n.event);return OK;
    case 19:if(!c.locked||!c.storage||a[1]!=c.storage->guest+c.lock.offset||a[2]>c.lock.first||a[4]>c.lock.second||(a[4]&&a[3]!=c.storage->guest))return INVALID;
        std::memcpy(c.storage->pcm.data()+c.lock.offset,hle_ptr(a[1]),a[2]);if(a[4])std::memcpy(c.storage->pcm.data(),hle_ptr(a[3]),a[4]);c.locked=false;return OK;
    case 20:return OK; // Published host storage is never lost by this backend.
    case 21:if(a[1])return UNSUPPORTED;return OK;
    case 22:if(a[2])return UNSUPPORTED;return OK;
    case 23:if(ptr(a[4],4))MEM32(a[4])=0;return NOIFACE;
    default:return INVALID;
    }
}
uint32_t device_call(Core&,uint32_t method,const uint32_t* a){
    switch(method){
    case 3:return make_buffer(a[1],a[2],a[3]);
    case 4:if(!ptr(a[1],96)||MEM32(a[1])!=96)return INVALID;std::memset(hle_ptr(a[1]+4),0,92);MEM32(a[1]+4)=0xf1f;MEM32(a[1]+8)=100;MEM32(a[1]+12)=200000;MEM32(a[1]+16)=1;return OK;
    case 5:{if(!ptr(a[2],4))return INVALID;MEM32(a[2])=0;auto found=views.find(a[1]);if(found==views.end()||found->second.kind!=BUFFER||found->second.core->primary)return INVALID;
        if(buffers.size()==buffers.capacity())buffers.reserve(std::max(size_t(64),buffers.size()*2));
        auto& source=*found->second.core;auto copy=std::make_unique<Core>();copy->kind=BUFFER;copy->flags=source.flags;copy->format=source.format;
        copy->voice=source.voice;copy->voice.position=0;copy->voice.playing=0;copy->storage=source.storage;copy->volume=source.volume;copy->pan=source.pan;copy->frequency=source.frequency;copy->spatial=source.spatial;
        auto* c=copy.get();uint32_t va=insert(std::move(copy));buffers.push_back(c);++duplicates;MEM32(a[2])=va;return OK;}
    case 6:return a[1]&&a[2]>=1&&a[2]<=4?OK:INVALID;
    case 7:return OK;
    case 8:if(!ptr(a[1],4))return INVALID;MEM32(a[1])=4;return OK;
    case 9:return (a[1]&0xff)==4?OK:UNSUPPORTED;
    case 10:return INVALIDCALL;
    case 11:if(!ptr(a[1],4))return INVALID;MEM32(a[1])=1;return OK;
    default:return INVALID;
    }
}
uint32_t dispatch(uint32_t kind,uint32_t method,const uint32_t* a){
    std::lock_guard<std::mutex> lock(mixer_mutex);
    auto found=views.find(a[0]);if(found==views.end()||found->second.kind!=kind)return INVALID;
    Core& c=*found->second.core;
    if(method==0)return query(c,a[1],a[2]);
    if(method==1)return ++c.refs;
    if(method==2){uint32_t left=--c.refs;if(!left)release(c);return left;}
    if(kind==DEVICE)return device_call(c,method,a);
    if(kind==BUFFER)return buffer_call(c,method,a);
    if(kind==LISTENER)return listener_call(method,a);
    if(kind==SPATIAL)return spatial_call(c,method,a);
    if(kind==NOTIFY){
        if(a[1]>256||(a[1]&&!ptr(a[2],uint64_t(a[1])*8)))return INVALID;
        std::vector<Notification> next;
        for(uint32_t i=0;i<a[1];++i){uint32_t offset=MEM32(a[2]+i*8),event=MEM32(a[2]+i*8+4);
            if((offset>=c.voice.bytes&&offset!=UINT32_MAX)||hle_handle_type(event)!=HLE_H_EVENT)return INVALID;next.push_back({offset,event});}
        c.notifications=std::move(next);return OK;
    }
    return INVALID;
}
template<uint32_t Kind,uint32_t Method,uint32_t Count>void method(){
    uint32_t a[8]{};for(uint32_t i=0;i<Count;++i)a[i]=A32(i);
    uint32_t result;try{result=dispatch(Kind,Method,a);}catch(const std::bad_alloc&){result=NOMEM;}
    if constexpr(Kind==DEVICE&&(Method==3||Method==5)){
        if(result&0x80000000u){std::lock_guard<std::mutex> lock(mixer_mutex);
            auto& failures=Method==3?create_failures:duplicate_failures;
            if(++failures<=8)std::fprintf(stderr,"[audio] %s failed=%08x buffers=%zu active=%zu\n",Method==3?"CreateSoundBuffer":"DuplicateSoundBuffer",result,buffers.size(),active_buffers.size());}
    }
    RET(result,Count);
}
template<uint32_t K,uint32_t M,uint32_t N>void register_method(){
    std::snprintf(method_names[K][M],40,"TF.Audio.%u.%u",K,M);
    win32hle_shim entry[]={{method_names[K][M],method<K,M,N>},{nullptr,nullptr}};
    win32hle_register(entry);method_addresses[K][M]=hle_resolve(entry[0].name);
}
#define M(k,m,n) register_method<k,m,n>()
void create(){
    if(!ptr(A32(1),4))RET(INVALID,3);MEM32(A32(1))=0;
    if(A32(2))RET(0x80040110u,3);if(A32(0))RET(NODRIVER,3);
    if(!open_sink())RET(NODRIVER,3);
    try{ensure_vtables();std::lock_guard<std::mutex> lock(mixer_mutex);
        auto core=std::make_unique<Core>();core->kind=DEVICE;MEM32(A32(1))=insert(std::move(core));
    }catch(const std::bad_alloc&){RET(NOMEM,3);}
    std::fprintf(stderr,"[audio] DirectSoundCreate8 -> Android PCM device\n");RET(OK,3);
}
} // namespace

extern "C" int tf_audio_register(){
    static bool registered=false;if(registered)return 0;registered=true;
    M(DEVICE,0,3);M(DEVICE,1,1);M(DEVICE,2,1);M(DEVICE,3,4);M(DEVICE,4,2);M(DEVICE,5,3);M(DEVICE,6,3);M(DEVICE,7,1);M(DEVICE,8,2);M(DEVICE,9,2);M(DEVICE,10,2);M(DEVICE,11,2);
    M(BUFFER,0,3);M(BUFFER,1,1);M(BUFFER,2,1);M(BUFFER,3,2);M(BUFFER,4,3);M(BUFFER,5,4);M(BUFFER,6,2);M(BUFFER,7,2);M(BUFFER,8,2);M(BUFFER,9,2);M(BUFFER,10,3);M(BUFFER,11,8);M(BUFFER,12,4);M(BUFFER,13,2);M(BUFFER,14,2);M(BUFFER,15,2);M(BUFFER,16,2);M(BUFFER,17,2);M(BUFFER,18,1);M(BUFFER,19,5);M(BUFFER,20,1);M(BUFFER,21,4);M(BUFFER,22,4);M(BUFFER,23,5);
    M(LISTENER,0,3);M(LISTENER,1,1);M(LISTENER,2,1);M(LISTENER,3,2);M(LISTENER,4,2);M(LISTENER,5,2);M(LISTENER,6,3);M(LISTENER,7,2);M(LISTENER,8,2);M(LISTENER,9,2);M(LISTENER,10,3);M(LISTENER,11,3);M(LISTENER,12,3);M(LISTENER,13,8);M(LISTENER,14,5);M(LISTENER,15,3);M(LISTENER,16,5);M(LISTENER,17,1);
    M(SPATIAL,0,3);M(SPATIAL,1,1);M(SPATIAL,2,1);M(SPATIAL,3,2);M(SPATIAL,4,3);M(SPATIAL,5,2);M(SPATIAL,6,2);M(SPATIAL,7,2);M(SPATIAL,8,2);M(SPATIAL,9,2);M(SPATIAL,10,2);M(SPATIAL,11,2);M(SPATIAL,12,3);M(SPATIAL,13,4);M(SPATIAL,14,5);M(SPATIAL,15,3);M(SPATIAL,16,3);M(SPATIAL,17,3);M(SPATIAL,18,3);M(SPATIAL,19,5);M(SPATIAL,20,5);
    M(NOTIFY,0,3);M(NOTIFY,1,1);M(NOTIFY,2,1);M(NOTIFY,3,3);
    win32hle_shim exports[]={{"dsound.dll#11",create},{"DirectSoundCreate8",create},{"dsound.dll#1",create},{"DirectSoundCreate",create},{nullptr,nullptr}};
    return win32hle_register(exports)+79;
}
extern "C" void tf_audio_focus_gain(float gain){focus_gain.store(std::isfinite(gain)?std::clamp(gain,0.f,1.f):0.f,std::memory_order_relaxed);}
extern "C" void tf_audio_set_suspended(int suspended){session_suspended.store(suspended!=0,std::memory_order_relaxed);}
extern "C" void tf_audio_service(){
    if(!stream)return;
    int32_t error=stream_error.exchange(0);
    if(error){std::fprintf(stderr,"[audio] stream error %d; reopening output\n",error);close_sink();if(!open_sink())return;}
    uint32_t active=0,effects=0,total=0;uint64_t nonzero,clipped,effect_nonzero,plays,dups,create_errors,duplicate_errors;float peak,effect_peak;
    {std::lock_guard<std::mutex> lock(mixer_mutex);total=uint32_t(buffers.size());
        for(auto* c:active_buffers)if(c->voice.playing){++active;if(!(c->flags&0x100))++effects;}
        nonzero=mixed_nonzero;clipped=clipped_samples;peak=mixed_peak;
        effect_nonzero=sfx_nonzero;effect_peak=sfx_peak;plays=sfx_plays;dups=duplicates;create_errors=create_failures;duplicate_errors=duplicate_failures;
        mixed_nonzero=clipped_samples=sfx_nonzero=sfx_plays=0;mixed_peak=sfx_peak=0;}
    std::fprintf(stderr,"[audio] buffers=%u active=%u sfx=%u plays=%llu duplicates=%llu failures=%llu/%llu sfx_nonzero=%llu sfx_peak=%.3f mixed_nonzero=%llu peak=%.3f clipped=%llu callbacks=%u xruns=%d queued=%u\n",total,active,effects,
        (unsigned long long)plays,(unsigned long long)dups,(unsigned long long)create_errors,(unsigned long long)duplicate_errors,
        (unsigned long long)effect_nonzero,effect_peak,(unsigned long long)nonzero,peak,(unsigned long long)clipped,callbacks.load(),AAudioStream_getXRunCount(stream),queued_frames());
}
extern "C" void tf_audio_shutdown(){
    close_sink();mach_enter();
    {std::lock_guard<std::mutex> lock(mixer_mutex);
        active_buffers.clear();buffers.clear();for(auto& pair:views)hle_free(hle_ptr(pair.first));views.clear();cores.clear();}
    mach_leave();
}

/* Synthetic guest32 COM integration test. Device mode opens a real muted
 * AAudio stream; it never loads a game image or operates an Android screen. */
extern "C" int tf_audio_selftest(int device_output){
    if(!device_output)return INVALID;
    hle_mem_init();tf_audio_register();tf_audio_focus_gain(0);
    uint32_t memory=0;int result=0;
    auto run=[&]()->int {
        mach_enter();memory=hle_va(hle_alloc(4096));mach_leave();if(!memory)return 90;
        auto call=[](uint32_t self,uint32_t slot,std::initializer_list<uint32_t> values){
            uint32_t args[8]={self};uint32_t n=1;for(auto value:values)args[n++]=value;
            return hle_call_guest(MEM32(MEM32(self)+slot*4),int(n),args);
        };
        auto output=memory+64,desc=memory+128,wfx=memory+256,iid=memory+512;
        uint32_t args[]={0,output,0};
        if(hle_call_guest(hle_resolve("DirectSoundCreate8"),3,args)!=OK)return 91;
        uint32_t device=MEM32(output);if(!device)return 92;
        MEM32(memory)=96;if(call(device,4,{memory})!=OK||MEM32(memory+4)!=0xf1f)return 93;
        std::memcpy(hle_ptr(iid),device8_iid,16);
        if(call(device,0,{iid,output})!=OK||MEM32(output)!=device)return 94;
        if(call(device,2,{})!=1)return 95;
        MEM32(desc)=36;MEM32(desc+4)=0x11;MEM32(desc+8)=0;MEM32(desc+16)=0;
        if(call(device,3,{desc,output,0})!=OK)return 96;uint32_t primary=MEM32(output);
        MEM16(wfx)=1;MEM16(wfx+2)=2;MEM32(wfx+4)=44100;MEM32(wfx+8)=176400;MEM16(wfx+12)=4;MEM16(wfx+14)=16;
        if(call(primary,14,{wfx})!=OK)return 97;
        std::memcpy(hle_ptr(iid),listener_iid,16);
        if(call(primary,0,{iid,output})!=OK)return 98;uint32_t listener_view=MEM32(output);
        if(call(listener_view,13,{0,0,bits(1.f),0,bits(1.f),0,1})!=OK||call(listener_view,15,{0,0})!=OK||call(listener_view,17,{})!=OK)return 99;
        if(call(listener_view,8,{memory})!=OK||MEM32(memory)!=0)return 121;
        MEM16(wfx+2)=1;MEM32(wfx+4)=22050;MEM32(wfx+8)=44100;MEM16(wfx+12)=2;
        MEM32(desc+4)=0x1b8;MEM32(desc+8)=176400;MEM32(desc+16)=wfx;
        if(call(device,3,{desc,output,0})!=OK)return 100;uint32_t buffer=MEM32(output);
        std::memcpy(hle_ptr(iid),buffer8_iid,16);
        if(call(buffer,0,{iid,output})!=OK||MEM32(output)!=buffer)return 101;
        std::memcpy(hle_ptr(iid),spatial_iid,16);
        if(call(buffer,0,{iid,output})!=OK)return 102;uint32_t spatial_view=MEM32(output);
        if(call(spatial_view,16,{bits(1e9f),0})||call(spatial_view,17,{bits(1e9f),0})||call(spatial_view,18,{1,0})||call(spatial_view,19,{bits(1.f),0,bits(1.f),0}))return 103;
        auto p1=memory+1024,n1=p1+4,p2=p1+8,n2=p1+12;
        if(call(buffer,11,{0,176400,p1,n1,p2,n2,0})!=OK||MEM32(n1)!=176400||MEM32(n2)!=0)return 104;
        uint32_t samples=MEM32(p1);for(uint32_t i=0;i<88200;++i)MEM16(samples+i*2)=uint16_t(int16_t(i%55<27?4000:-4000));
        if(call(buffer,19,{samples,176400,0,0})!=OK)return 105;
        if(call(device,5,{buffer,output})!=OK)return 106;uint32_t duplicate=MEM32(output);
        if(call(duplicate,17,{11025})!=OK||call(duplicate,8,{memory})!=OK||MEM32(memory)!=11025||call(buffer,8,{memory})!=OK||MEM32(memory)!=22050)return 107;
        if(call(buffer,15,{uint32_t(-600)})!=OK||call(buffer,6,{memory})!=OK||int32_t(MEM32(memory))!= -600)return 108;
        // Reproduce the real preload -> DuplicateSoundBuffer -> Play path.
        // The old 256-object ceiling prevented the game reaching SFX Play.
        std::vector<uint32_t> sources,voices;
        MEM32(desc+4)=0xb8;MEM32(desc+8)=64;
        for(uint32_t i=0;i<384;++i){
            if(call(device,3,{desc,output,0})!=OK)return 122;
            sources.push_back(uint32_t(MEM32(output)));
        }
        if(call(sources[0],11,{0,64,p1,n1,p2,n2,0})!=OK)return 123;
        for(uint32_t i=0;i<32;++i)MEM16(MEM32(p1)+i*2)=uint16_t(int16_t(i<16?4000:-4000));
        if(call(sources[0],19,{MEM32(p1),64,0,0})!=OK)return 124;
        for(uint32_t i=0;i<64;++i){
            if(call(device,5,{sources[0],output})!=OK)return 125;
            uint32_t voice=MEM32(output);voices.push_back(voice);
            if(call(voice,15,{uint32_t(-3600)})||call(voice,12,{0,0,1})||call(voice,12,{0,0,1}))return 126;
        }
        {std::lock_guard<std::mutex> lock(mixer_mutex);
            if(buffers.size()!=450||active_buffers.size()!=64)return 127;}
        std::memcpy(hle_ptr(iid),notify_iid,16);
        if(call(buffer,0,{iid,output})!=OK)return 109;uint32_t notify_view=MEM32(output);
        uint32_t event_args[]={0,0,0,0};uint32_t event=hle_call_guest(hle_resolve("CreateEventA"),4,event_args);if(!event)return 110;
        uint32_t positions=memory+768;
        for(uint32_t i=0;i<4;++i){MEM32(positions+i*8)=i*44100;MEM32(positions+i*8+4)=event;}
        if(call(notify_view,3,{4,positions})!=OK)return 111;
        // Keep the machine while querying the initial cursor so the worker
        // cannot generate any of this voice's samples before the assertion.
        mach_enter();uint32_t initial=call(buffer,12,{0,0,1});
        initial|=call(buffer,4,{memory,0});uint32_t cursor=MEM32(memory);mach_leave();
        if(initial||cursor!=0)return 112;
        uint32_t wait[]={event,1500};
        if(hle_call_guest(hle_resolve("WaitForSingleObject"),2,wait)!=0)return 113;
        if(hle_call_guest(hle_resolve("WaitForSingleObject"),2,wait)!=0)return 114;
        if(call(buffer,4,{memory,0})!=OK||MEM32(memory)<22050||MEM32(memory)>=176400)return 115;
        {std::lock_guard<std::mutex> lock(mixer_mutex);if(!sfx_nonzero)return 128;}
        for(auto voice:voices)if(call(voice,18,{})||call(voice,2,{})!=0)return 129;
        // A naturally finished one-shot must also leave the active list.
        if(call(sources[0],12,{0,0,0}))return 130;
        if(hle_call_guest(hle_resolve("WaitForSingleObject"),2,wait)!=0)return 131;
        if(call(sources[0],9,{memory})||MEM32(memory)!=0)return 132;
        {std::lock_guard<std::mutex> lock(mixer_mutex);
            if(active_buffers.size()!=1||active_buffers[0]->views[BUFFER]!=buffer)return 133;}
        for(auto source:sources)if(call(source,2,{})!=0)return 134;
        {std::lock_guard<std::mutex> lock(mixer_mutex);if(buffers.size()!=2)return 135;}
        // Explicit wrap lock must publish both regions into shared PCM storage.
        if(call(buffer,11,{176396,8,p1,n1,p2,n2,0})!=OK||MEM32(n1)!=4||MEM32(n2)!=4)return 116;
        if(call(buffer,19,{MEM32(p1),4,MEM32(p2),4})!=OK)return 117;
        if(call(buffer,18,{})!=OK||call(buffer,9,{memory})!=OK||MEM32(memory)!=0)return 118;
        uint64_t nonzero;{std::lock_guard<std::mutex> lock(mixer_mutex);nonzero=mixed_nonzero;}
        if(!callbacks.load()||!nonzero||!stream||AAudioStream_getState(stream)!=AAUDIO_STREAM_STATE_STARTED)return 119;
        std::fprintf(stderr,"[audio-test] PASS: real muted AAudio callbacks=%u, mixed_nonzero=%llu, PCM/3D/duplicate/loop cursor/quarter-ring events/wrap unlock; 384 preloaded sources + 64 SFX voices, nonzero shared PCM before mute, repeated Play/Stop/Release/one-shot active-list cleanup\n",callbacks.load(),(unsigned long long)nonzero);
        tf_audio_service();return 0;
    };
    try{result=run();}catch(...){result=120;}
    tf_audio_shutdown();tf_audio_focus_gain(1);if(memory){mach_enter();hle_free(hle_ptr(memory));mach_leave();}
    return result;
}
