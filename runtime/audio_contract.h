/* Bounds and cursor contracts shared by the guest DirectSound adapter.
 * Plain C helpers; no allocations, host pointers retained, or OS calls. */
#ifndef TF_AUDIO_CONTRACT_H
#define TF_AUDIO_CONTRACT_H
#include <stdint.h>
#include <stddef.h>

typedef struct { uint32_t channels,rate,bits,block,average; } TF_AudioFormat;
typedef struct { uint32_t offset,first,second; } TF_AudioLock;

static inline uint32_t tf_audio_u16(const unsigned char *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8);
}
static inline uint32_t tf_audio_u32(const unsigned char *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)
          |((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}

/* PCM's cbSize is ignored by WAVEFORMATEX, so its 16-byte base is sufficient.
 * Out remains unchanged when any field is invalid. */
static inline int tf_audio_format(const unsigned char *wfx,uint32_t bytes,
                                  TF_AudioFormat *out) {
    if(!wfx||!out||bytes<16||tf_audio_u16(wfx)!=1)return 0;
    TF_AudioFormat value;
    value.channels=tf_audio_u16(wfx+2);value.rate=tf_audio_u32(wfx+4);
    value.average=tf_audio_u32(wfx+8);value.block=tf_audio_u16(wfx+12);
    value.bits=tf_audio_u16(wfx+14);
    if((value.channels!=1&&value.channels!=2)||(value.bits!=8&&value.bits!=16)
       ||value.rate<100||value.rate>200000)return 0;
    if(value.block!=value.channels*(value.bits/8)
       ||value.average!=value.rate*value.block)return 0;
    *out=value;return 1;
}

/* Flags: 0=explicit byte offset, 1=FROMWRITECURSOR, 2=ENTIREBUFFER.
 * The effective byte region can wrap once; neither addition nor multiplication
 * of guest-controlled offset/length is required. Out is unchanged on failure. */
static inline int tf_audio_lock_range(uint32_t bufferbytes,uint32_t offset,
        uint32_t length,uint32_t flags,uint32_t writecursor,TF_AudioLock *out) {
    if(!out||!bufferbytes||flags>2)return 0;
    if(flags==2){offset=0;length=bufferbytes;}
    else if(flags==1)offset=writecursor;
    if(offset>=bufferbytes||!length||length>bufferbytes)return 0;
    TF_AudioLock value;
    value.offset=offset;
    value.first=length<bufferbytes-offset?length:bufferbytes-offset;
    value.second=length-value.first;
    *out=value;return 1;
}

/* old_frame is a normalized fractional cursor in [0,total_frames).
 * Stop events are a separate contract, not an offset>=total_frames here.
 * A newly-started buffer reports offset zero even without rendered frames;
 * all other crossings use the half-open interval old<offset<=new. */
static inline int tf_audio_crossed(double old_frame,double advanced_frames,
        uint32_t total_frames,uint32_t offset_frame,int looping,int just_started) {
    if(!total_frames||offset_frame>=total_frames
       ||!(old_frame>=0.0&&old_frame<(double)total_frames)
       ||!(advanced_frames>=0.0&&advanced_frames<=(double)UINT64_MAX))return 0;
    if(offset_frame==0&&just_started)return 1;
    double offset=(double)offset_frame;
    if(!looping)return offset>old_frame&&advanced_frames>=offset-old_frame;
    if(advanced_frames>=(double)total_frames)return 1;
    double distance=offset>old_frame?offset-old_frame
                                  :(double)total_frames-old_frame+offset;
    return advanced_frames>=distance;
}
#endif
