#ifndef TRANSFORMERS_STARTUP_INPUT_STATE_H
#define TRANSFORMERS_STARTUP_INPUT_STATE_H
#include <stdint.h>

enum {TF_DI_KEYBOARD=1,TF_DI_MOUSE=2,TF_DI_OBJECTS=256,TF_DI_EVENTS=256};
enum {TF_DI_GUID_NULL=0,TF_DI_GUID_KEY=1,TF_DI_GUID_X=2,TF_DI_GUID_Y=3,TF_DI_GUID_Z=4,TF_DI_GUID_BUTTON=5,TF_DI_GUID_OTHER=6};
typedef struct {uint32_t offset,width,index;} TF_DI_Object;
typedef struct {uint32_t size,count;TF_DI_Object objects[TF_DI_OBJECTS];} TF_DI_Format;
typedef struct {uint32_t offset,data,time,sequence,app;} TF_DI_Event;
typedef struct {TF_DI_Event items[TF_DI_EVENTS];uint32_t count,requested,overflow;} TF_DI_Buffer;

/* Returns DIERR_INVALIDPARAM or OBJECTNOTFOUND; consumes 32-bit guest layouts.
 * Known standard formats are ordinary inputs, not special-cased addresses. */
static inline uint32_t tf_di_format(uint32_t kind,const uint32_t header[6],const uint32_t *objects,
                                    const uint8_t *guid_tags,TF_DI_Format *out) {
    if(header[0]!=24||header[1]!=16||!header[3]||header[3]>4096||!header[4]||header[4]>256||!header[5])return 0x80070057u;
    if(header[2]!=1&&header[2]!=2)return 0x80070057u;
    uint8_t occupied[4096]={0},used[256]={0};TF_DI_Format result={0};result.size=header[3];
    for(uint32_t n=0;n<header[4];++n){
        const uint32_t *o=objects+4*n;uint32_t type=o[2],instance=(type>>8)&0xffffu,index=256,width=1;
        int optional=(type&0x80000000u)!=0;uint8_t tag=guid_tags[n];
        if(o[3])return 0x80070057u; /* force/aspect/usage semantics need an implementation */
        if((type&0x0cu)&&!(type&3u)){
            if(kind==TF_DI_KEYBOARD&&(tag==TF_DI_GUID_NULL||tag==TF_DI_GUID_KEY)){
                if(instance==0xffffu){for(instance=0;instance<256&&used[instance];++instance){}}
                if(instance<256)index=instance;
            }else if(kind==TF_DI_MOUSE&&(tag==TF_DI_GUID_NULL||tag==TF_DI_GUID_BUTTON)){
                if(instance==0xffffu){for(instance=0;instance<8&&used[instance+3];++instance){}}
                if(instance<8)index=instance+3;
            }
        }else if(kind==TF_DI_MOUSE&&(type&3u)&&!(type&0x0cu)){
            width=4;
            if(tag>=TF_DI_GUID_X&&tag<=TF_DI_GUID_Z)index=tag-TF_DI_GUID_X;
            else if(tag==TF_DI_GUID_NULL){
                if(instance==0xffffu){for(instance=0;instance<3&&used[instance];++instance){}}
                if(instance<3)index=instance;
            }
        }
        if(index>=256||used[index]){if(optional)continue;return 0x80070002u;}
        if(o[1]>result.size||width>result.size-o[1])return 0x80070057u;
        for(uint32_t b=0;b<width;++b){if(occupied[o[1]+b])return 0x80070057u;occupied[o[1]+b]=1;}
        used[index]=1;result.objects[result.count++]=(TF_DI_Object){o[1],width,index};
    }
    *out=result;return 0;
}
static inline uint32_t tf_di_vk_scan(uint32_t vk) {
    if(vk>255)return 0;
    static const uint8_t letters[26]={0x1e,0x30,0x2e,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,0x31,0x18,0x19,0x10,0x13,0x1f,0x14,0x16,0x2f,0x11,0x2d,0x15,0x2c};
    if(vk>='A'&&vk<='Z')return letters[vk-'A'];
    if(vk>='1'&&vk<='9')return vk-'1'+2;
    if(vk>=0x70&&vk<=0x79)return vk-0x70+0x3b;
    switch(vk){
    case '0':return 0x0b;case 8:return 0x0e;case 9:return 0x0f;case 13:return 0x1c;
    case 16:case 0xa0:return 0x2a;case 0xa1:return 0x36;case 17:case 0xa2:return 0x1d;case 0xa3:return 0x9d;
    case 18:case 0xa4:return 0x38;case 0xa5:return 0xb8;case 19:return 0xc5;case 20:return 0x3a;
    case 27:return 1;case 32:return 0x39;case 33:return 0xc9;case 34:return 0xd1;case 35:return 0xcf;case 36:return 0xc7;
    case 37:return 0xcb;case 38:return 0xc8;case 39:return 0xcd;case 40:return 0xd0;case 44:return 0xb7;case 45:return 0xd2;case 46:return 0xd3;
    case 0x5b:return 0xdb;case 0x5c:return 0xdc;case 0x5d:return 0xdd;
    case 0x60:return 0x52;case 0x61:return 0x4f;case 0x62:return 0x50;case 0x63:return 0x51;case 0x64:return 0x4b;
    case 0x65:return 0x4c;case 0x66:return 0x4d;case 0x67:return 0x47;case 0x68:return 0x48;case 0x69:return 0x49;
    case 0x6a:return 0x37;case 0x6b:return 0x4e;case 0x6d:return 0x4a;case 0x6e:return 0x53;case 0x6f:return 0xb5;
    case 0x7a:return 0x57;case 0x7b:return 0x58;case 0x90:return 0x45;case 0x91:return 0x46;
    case 0xba:return 0x27;case 0xbb:return 0x0d;case 0xbc:return 0x33;case 0xbd:return 0x0c;case 0xbe:return 0x34;
    case 0xbf:return 0x35;case 0xc0:return 0x29;case 0xdb:return 0x1a;case 0xdc:return 0x2b;case 0xdd:return 0x1b;case 0xde:return 0x28;
    default:return 0;
    }
}
static inline int32_t tf_di_add_delta(int32_t value,int32_t delta){
    int64_t sum=(int64_t)value+delta;return sum>0x7fffffff?0x7fffffff:sum<(-0x7fffffff-1)?(-0x7fffffff-1):(int32_t)sum;
}
static inline void tf_di_buffer_push(TF_DI_Buffer *buffer,TF_DI_Event event){
    if(!buffer->requested)return;
    uint32_t capacity=buffer->requested-1;if(capacity>TF_DI_EVENTS)capacity=TF_DI_EVENTS;
    if(!capacity){buffer->overflow=1;return;}
    if(buffer->count==capacity){for(uint32_t i=1;i<buffer->count;++i)buffer->items[i-1]=buffer->items[i];--buffer->count;buffer->overflow=1;}
    buffer->items[buffer->count++]=event;
}
static inline uint32_t tf_di_buffer_take(TF_DI_Buffer *buffer,uint32_t wanted,int peek,TF_DI_Event *out,uint32_t *obtained){
    uint32_t count=wanted<buffer->count?wanted:buffer->count,status=buffer->overflow?1:0;
    if(out)for(uint32_t i=0;i<count;++i)out[i]=buffer->items[i];
    if(!peek){for(uint32_t i=count;i<buffer->count;++i)buffer->items[i-count]=buffer->items[i];buffer->count-=count;buffer->overflow=0;}
    *obtained=count;return status;
}
#endif
