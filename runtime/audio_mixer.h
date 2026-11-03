/* PCM software mixing helpers. Samples point to host-owned published storage,
 * never mutable guest memory. The caller owns synchronization and lifetime.
 * No allocation, locking, I/O or Android calls occur in these functions. */
#ifndef TF_AUDIO_MIXER_H
#define TF_AUDIO_MIXER_H
#include <stdint.h>
#include <stddef.h>

typedef struct {
    const uint8_t *samples;
    uint32_t bytes, channels, bits, source_rate, frequency;
    double position; /* Fractional source-frame cursor, not output frames. */
    float gain_l, gain_r;
    int playing, looping;
} TF_AudioVoice;

static inline float tf_audio_sample(const TF_AudioVoice *voice,
                                    uint32_t frame, uint32_t channel) {
    uint32_t sample_bytes=voice->bits/8;
    uint64_t offset=(uint64_t)frame*voice->channels*sample_bytes;
    if(voice->channels==2)offset+=(uint64_t)channel*sample_bytes;
    if(voice->bits==8)return ((int)voice->samples[offset]-128)*(1.0f/128.0f);
    uint32_t word=(uint32_t)voice->samples[offset]
                 |((uint32_t)voice->samples[offset+1]<<8);
    int32_t signed_word=word<32768u?(int32_t)word:(int32_t)word-65536;
    return (float)signed_word*(1.0f/32768.0f);
}

static inline double tf_audio_wrap(double position, uint32_t count) {
    /* Validated positions and the uint32_t frequency/rate ratio keep this
     * quotient in uint64_t range, including a one-frame looping buffer. */
    double result=position-(double)(uint64_t)(position/(double)count)*(double)count;
    // Bound division-rounding at exact wrap points before converting cursor
    // back to an integer sample index on the next render call.
    if(result<0.0)result+=(double)count;
    if(result>=(double)count)result-=(double)count;
    return result;
}

/* Advance a voice across output frames intentionally discarded after an
 * underrun. This consumes no samples and keeps streaming play cursors in sync. */
static inline uint32_t tf_audio_advance_voice(TF_AudioVoice *voice,
                                            uint32_t frames,uint32_t output_rate) {
    if(!voice||!voice->playing||!frames||frames>INT32_MAX||!output_rate)return 0;
    uint32_t frequency=voice->frequency?voice->frequency:voice->source_rate;
    if(!voice->samples||!frequency||(voice->channels!=1&&voice->channels!=2)
       ||(voice->bits!=8&&voice->bits!=16))return 0;
    uint32_t block=voice->channels*(voice->bits/8);
    if(!voice->bytes||voice->bytes%block)return 0;
    uint32_t count=voice->bytes/block;
    double position=voice->position;
    if(!(position>=0.0&&position<=(double)UINT32_MAX))position=0.0;
    if(position>=(double)count){
        if(voice->looping)position=tf_audio_wrap(position,count);
        else{voice->playing=0;voice->position=0;return 0;}
    }
    double step=(double)frequency/(double)output_rate;
    double end=position+step*(double)frames;
    if(end>=(double)count){
        if(voice->looping)voice->position=tf_audio_wrap(end,count);
        else{
            double until_end=((double)count-position)/step;
            uint32_t consumed=until_end>=(double)frames?frames:(uint32_t)until_end;
            if((double)consumed<until_end&&consumed<frames)++consumed;
            voice->playing=0;voice->position=0;return consumed;
        }
    }else voice->position=end;
    return frames;
}

/* Add one voice into an already-cleared/interleaved stereo float output.
 * Frequency zero means the source rate (DSBFREQUENCY_ORIGINAL). Returns the
 * number of output frames mixed. A finished one-shot stops and resets cursor. */
static inline uint32_t tf_audio_mix_voice(TF_AudioVoice *voice, float *output,
                                        uint32_t frames, uint32_t output_rate) {
    if(!voice||!voice->playing||!output||!frames||frames>INT32_MAX||!output_rate)
        return 0;
    uint32_t frequency=voice->frequency?voice->frequency:voice->source_rate;
    if(!voice->samples||!frequency||(voice->channels!=1&&voice->channels!=2)
       ||(voice->bits!=8&&voice->bits!=16))return 0;
    uint32_t block=voice->channels*(voice->bits/8);
    if(!voice->bytes||voice->bytes%block)return 0;
    uint32_t count=voice->bytes/block;
    double position=voice->position;
    if(!(position>=0.0&&position<=(double)UINT32_MAX))position=0.0;
    if(position>=(double)count){
        if(voice->looping)position=tf_audio_wrap(position,count);
        else{voice->playing=0;voice->position=0;return 0;}
    }
    double step=(double)frequency/(double)output_rate;
    for(uint32_t i=0;i<frames;++i){
        uint32_t first=(uint32_t)position,next=first+1;
        if(next==count)next=voice->looping?0:first;
        float fraction=(float)(position-(double)first);
        float left=tf_audio_sample(voice,first,0);
        float next_left=tf_audio_sample(voice,next,0);
        output[i*2]+=(left+(next_left-left)*fraction)*voice->gain_l;
        if(voice->channels==1)output[i*2+1]+=(left+(next_left-left)*fraction)*voice->gain_r;
        else{
            float right=tf_audio_sample(voice,first,1);
            float next_right=tf_audio_sample(voice,next,1);
            output[i*2+1]+=(right+(next_right-right)*fraction)*voice->gain_r;
        }
        position+=step;
        if(position>=(double)count){
            if(voice->looping)position=tf_audio_wrap(position,count);
            else{voice->playing=0;voice->position=0;return i+1;}
        }
    }
    voice->position=position;
    return frames;
}

/* Clamp after all voices are added. Return the number of saturated samples;
 * any non-finite NaN supplied through a bad gain becomes silence. */
static inline uint32_t tf_audio_clip(float *output,uint32_t frames) {
    if(!output||frames>INT32_MAX)return 0;
    uint32_t clipped=0;
    for(uint32_t i=0;i<frames*2;++i){
        float value=output[i];
        if(value>1.0f){output[i]=1.0f;++clipped;}
        else if(value< -1.0f){output[i]= -1.0f;++clipped;}
        else if(value!=value)output[i]=0.0f;
    }
    return clipped;
}
#endif
