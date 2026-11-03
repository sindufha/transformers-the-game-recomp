/* Virtual keyboard/mouse DirectInput8A, driven by actual host input callbacks.
 * Guest COM pointers/vtables live in the guest heap; method addresses are HLE
 * tokens. No host pointers are truncated. No joystick/controller is invented.
 * Force feedback, action maps and event handles fail.
 * ABI/reference: Microsoft DirectInput8 docs; sources in docs/CAMERA_INPUT.md. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "win32hle.h"
#include "startup_input.h"
#include "startup_input_state.h"
#include "startup_user32.h"

enum {DI_INVALID=0x80070057u,DI_NOIFACE=0x80004002u,DI_UNSUPPORTED=0x80004001u,
      DI_NOMEM=0x8007000eu,DI_NOTFOUND=0x80070002u,DI_NOTACQUIRED=0x8007000cu,
      DI_ACQUIRED=0x800700aau,DI_NOTBUFFERED=0x80040207u,DI_NOTINIT=0x80070015u};
typedef struct {
    uint32_t va,kind,refs,cooperative,hwnd,acquired,axis_mode;
    uint32_t motion_reports;int32_t reported_axes[3];
    int32_t delta[3];TF_DI_Format format;TF_DI_Buffer buffer;
} InputObject;
static InputObject objects[24];
static uint32_t factory_vtable,device_vtable,sequence;
static uint8_t keyboard[256],seen_virtual[256];
static uint32_t mouse_buttons;
static pthread_mutex_t input_lock=PTHREAD_MUTEX_INITIALIZER;
static const uint32_t iid_unknown[4]={0,0,0xc0u,0x46000000u};
static const uint32_t iid_factory[4]={0xbf798030u,0x4da2483au,0x645d99aau,0x009736edu};
static const uint32_t iid_device[4]={0x54d41080u,0x4833dc15u,0x8f741ba4u,0x7981a373u};
static const uint32_t sys_keyboard[4]={0x6f1d2b61u,0x11cfd5a0u,0x4544c7bfu,0x00005453u};
static const uint32_t sys_mouse[4]={0x6f1d2b60u,0x11cfd5a0u,0x4544c7bfu,0x00005453u};
static const uint32_t key_guid[4]={0x55728220u,0x11cfd33cu,0x4544c7bfu,0x00005453u};
static const uint32_t x_guid[4]={0xa36d02e0u,0x11cfc9f3u,0x4544c7bfu,0x00005453u};
static const uint32_t y_guid[4]={0xa36d02e1u,0x11cfc9f3u,0x4544c7bfu,0x00005453u};
static const uint32_t z_guid[4]={0xa36d02e2u,0x11cfc9f3u,0x4544c7bfu,0x00005453u};
static const uint32_t button_guid[4]={0xa36d02f0u,0x11cfc9f3u,0x4544c7bfu,0x00005453u};

static int span(uint32_t va,uint32_t bytes){return va&&bytes<=0xffffffffu-va;}
static int guid_equal(uint32_t va,const uint32_t guid[4]){return span(va,16)&&!memcmp(hle_ptr(va),guid,16);}
static uint32_t kind_from_guid(uint32_t va){return guid_equal(va,sys_keyboard)?TF_DI_KEYBOARD:guid_equal(va,sys_mouse)?TF_DI_MOUSE:0;}
static uint8_t guid_tag(uint32_t va){
    if(!va)return TF_DI_GUID_NULL;
    const uint32_t *values[]={key_guid,x_guid,y_guid,z_guid,button_guid};
    for(uint8_t i=0;i<5;++i)if(guid_equal(va,values[i]))return i+1;return TF_DI_GUID_OTHER;
}
static InputObject *find_object(uint32_t va){
    for(unsigned i=0;i<24;++i)if(objects[i].va==va&&objects[i].refs)return objects+i;return NULL;
}
static void input_event(InputObject *o,uint32_t index,uint32_t value,uint32_t time,uint32_t seq){
    if(!o->acquired)return;
    for(uint32_t n=0;n<o->format.count;++n)if(o->format.objects[n].index==index){
        TF_DI_Event e={o->format.objects[n].offset,value,time,seq,0};tf_di_buffer_push(&o->buffer,e);return;
    }
}
/* input_lock held. UI callbacks never enter the guest machine. */
static void key_locked(uint32_t scan,int down){
    uint8_t value=down?0x80:0;if(keyboard[scan]==value)return;keyboard[scan]=value;
    uint32_t seq=++sequence,time=hle_ticks_ms();
    for(unsigned i=0;i<24;++i)if(objects[i].va&&objects[i].kind==TF_DI_KEYBOARD)input_event(objects+i,scan,value,time,seq);
}
int tf_input_key_event(uint32_t vk,int down){
    uint32_t scan=tf_di_vk_scan(vk);if(!scan)return 0;
    pthread_mutex_lock(&input_lock);seen_virtual[vk]=1;key_locked(scan,down!=0);pthread_mutex_unlock(&input_lock);return 1;
}
/* Fallback for hosts still injecting only USER32 events. A VK seen through
 * the immediate hook always wins over USER32's potentially pending key-up. */
static void sync_user32(void){
    for(uint32_t vk=0;vk<256;++vk)if(!seen_virtual[vk]){
        uint32_t scan=tf_di_vk_scan(vk);if(scan&&tf_user32_key_down(vk))key_locked(scan,1);
    }
}
int tf_input_mouse_event(int32_t dx,int32_t dy,int32_t wheel,uint32_t buttons){
    if(buttons>255)return 0;
    pthread_mutex_lock(&input_lock);int32_t delta[3]={dx,dy,wheel};uint32_t time=hle_ticks_ms();
    for(unsigned axis=0;axis<3;++axis)if(delta[axis]){
        uint32_t seq=++sequence;
        for(unsigned i=0;i<24;++i)if(objects[i].va&&objects[i].kind==TF_DI_MOUSE&&objects[i].acquired){
            InputObject *o=objects+i;
            o->delta[axis]=tf_di_add_delta(o->delta[axis],delta[axis]);
            input_event(o,axis,(uint32_t)(o->axis_mode?delta[axis]:o->delta[axis]),time,seq);
        }
    }
    uint32_t changed=mouse_buttons^buttons;
    for(unsigned button=0;button<8;++button)if(changed&(1u<<button)){
        uint32_t seq=++sequence,value=buttons&(1u<<button)?0x80u:0;
        for(unsigned i=0;i<24;++i)if(objects[i].va&&objects[i].kind==TF_DI_MOUSE)input_event(objects+i,button+3,value,time,seq);
    }
    mouse_buttons=buttons;pthread_mutex_unlock(&input_lock);return 1;
}
void tf_input_release_all(void){
    pthread_mutex_lock(&input_lock);for(unsigned scan=0;scan<256;++scan)if(keyboard[scan])key_locked(scan,0);
    memset(seen_virtual,1,sizeof seen_virtual);pthread_mutex_unlock(&input_lock);tf_input_mouse_event(0,0,0,0);
}
static void method_name(char *out,size_t size,int factory,uint32_t slot){snprintf(out,size,"TF.DI8.%c.%u",factory?'F':'D',slot);}
static uint32_t method_va(int factory,uint32_t slot){char name[32];method_name(name,sizeof name,factory,slot);return hle_resolve(name);}
static uint32_t ensure_vtable(int factory){
    uint32_t *cached=factory?&factory_vtable:&device_vtable;if(*cached)return *cached;
    uint32_t count=factory?11:32;void *mem=hle_alloc(4*count);if(!mem)return 0;
    *cached=hle_va(mem);for(uint32_t n=0;n<count;++n){uint32_t token=method_va(factory,n);if(!token)hle_fatal("Unregistered DirectInput method %u",n);MEM32(*cached+4*n)=token;}
    return *cached;
}
static uint32_t new_object(uint32_t kind){
    uint32_t table=ensure_vtable(kind==0);if(!table)return 0;
    void *mem=hle_alloc(4);if(!mem)return 0;uint32_t va=hle_va(mem);
    pthread_mutex_lock(&input_lock);unsigned i;for(i=0;i<24;++i)if(!objects[i].va)break;
    if(i==24){pthread_mutex_unlock(&input_lock);hle_free(mem);return 0;}
    memset(objects+i,0,sizeof objects[i]);objects[i].va=va;objects[i].kind=kind;objects[i].refs=1;objects[i].axis_mode=1;
    MEM32(va)=table;pthread_mutex_unlock(&input_lock);return va;
}
static uint32_t device_type(uint32_t kind){return kind==TF_DI_KEYBOARD?0x413u:0x212u;}
static void device_info(uint32_t kind,uint32_t address,uint32_t size){
    memset(hle_ptr(address),0,size);MEM32(address)=size;
    const uint32_t *guid=kind==TF_DI_KEYBOARD?sys_keyboard:sys_mouse;
    memcpy(hle_ptr(address+4),guid,16);memcpy(hle_ptr(address+20),guid,16);MEM32(address+36)=device_type(kind);
    const char *name=kind==TF_DI_KEYBOARD?"Android virtual keyboard":"Android virtual mouse";
    memcpy(hle_ptr(address+40),name,strlen(name)+1);memcpy(hle_ptr(address+300),name,strlen(name)+1);
    if(size==580){MEM16(address+576)=1;MEM16(address+578)=kind==TF_DI_KEYBOARD?6:2;}
}
static int filter_kind(uint32_t filter,uint32_t kind){return filter==0||(filter==2&&kind==TF_DI_MOUSE)||(filter==3&&kind==TF_DI_KEYBOARD)||(filter&0xffu)==(device_type(kind)&0xffu);}
static void create_input(void){
    uint32_t version=A32(1),iid=A32(2),out=A32(3),outer=A32(4);
    if(!span(out,4))RET(DI_INVALID,5);MEM32(out)=0;
    if(outer)RET(0x80040110u,5);
    if(version!=0x800u)RET(DI_INVALID,5);
    if(!guid_equal(iid,iid_factory))RET(DI_NOIFACE,5);
    uint32_t va=new_object(0);MEM32(out)=va;
    fprintf(stderr,"[input] DirectInput8Create version=%x factory=%08x\n",version,va);
    RET(va?0:DI_NOMEM,5);
}
static uint32_t common_method(InputObject *o,uint32_t slot){
    if(slot==0){uint32_t out=A32(2);if(!span(out,4))return DI_INVALID;MEM32(out)=0;
        if(!guid_equal(A32(1),iid_unknown)&&!guid_equal(A32(1),o->kind?iid_device:iid_factory))return DI_NOIFACE;
        ++o->refs;MEM32(out)=o->va;return 0;}
    if(slot==1){if(o->refs==0xffffffffu)return 0xffffffffu;return ++o->refs;}
    uint32_t remaining=--o->refs;if(!remaining){uint32_t va=o->va;memset(o,0,sizeof *o);hle_free(hle_ptr(va));}return remaining;
}
static uint32_t factory_method(InputObject *o,uint32_t slot){
    (void)o;
    if(slot==3){uint32_t out=A32(2);if(!span(out,4))return DI_INVALID;MEM32(out)=0;
        if(A32(3))return 0x80040110u;uint32_t kind=kind_from_guid(A32(1));if(!kind)return DI_NOTFOUND;
        /* new_object takes input_lock; caller invokes factory methods unlocked. */
        uint32_t va=new_object(kind);MEM32(out)=va;
        fprintf(stderr,"[input] CreateDevice %s -> %08x\n",kind==TF_DI_KEYBOARD?"keyboard":"mouse",va);
        return va?0:DI_NOMEM;
    }
    if(slot==4){uint32_t filter=A32(1),callback=A32(2),reference=A32(3),flags=A32(4);
        if(!callback||flags&~0x70101u||filter>0x1cu)return DI_INVALID;
        if(flags&0x100u)return 0; /* no force-feedback device exists */
        uint32_t mark=hle_scratch_mark(),info=hle_scratch(580);
        for(uint32_t kind=TF_DI_MOUSE;kind>=TF_DI_KEYBOARD;--kind)if(filter_kind(filter,kind)){
            device_info(kind,info,580);uint32_t args[2]={info,reference};if(!hle_call_guest(callback,2,args))break;
        }
        hle_scratch_release(mark);return 0;
    }
    if(slot==5)return kind_from_guid(A32(1))?0:DI_NOTFOUND;
    if(slot==8){if(span(A32(3),16))memset(hle_ptr(A32(3)),0,16);else return DI_INVALID;return DI_NOTFOUND;}
    return DI_UNSUPPORTED;
}
static uint32_t set_format(InputObject *o,uint32_t address){
    if(o->acquired)return DI_ACQUIRED;if(!span(address,24))return DI_INVALID;
    uint32_t header[6];memcpy(header,hle_ptr(address),24);
    if(header[4]>256||!span(header[5],header[4]*16u))return DI_INVALID;
    uint32_t data[1024];uint8_t tags[256];memcpy(data,hle_ptr(header[5]),header[4]*16u);
    for(uint32_t i=0;i<header[4];++i)tags[i]=guid_tag(data[4*i]);
    TF_DI_Format format={0};uint32_t result=tf_di_format(o->kind,header,data,tags,&format);
    if(!result){o->format=format;o->axis_mode=header[2]==1?0:1;
        memset(o->delta,0,sizeof o->delta);o->buffer.count=o->buffer.overflow=0;}return result;
}
static uint32_t property(InputObject *o,int set){
    uint32_t prop=A32(1),address=A32(2);if(!span(address,20)||MEM32(address)!=20||MEM32(address+4)!=16||MEM32(address+8)||MEM32(address+12))return DI_INVALID;
    if(set&&o->acquired)return DI_ACQUIRED;
    if(prop==1){if(set){o->buffer.requested=MEM32(address+16);o->buffer.count=o->buffer.overflow=0;}
        else MEM32(address+16)=o->buffer.requested;return 0;}
    if(prop==2){
        if(o->kind!=TF_DI_MOUSE)return DI_UNSUPPORTED;
        if(!set){MEM32(address+16)=o->axis_mode;return 0;}
        uint32_t mode=MEM32(address+16);if(mode>1)return DI_INVALID;
        if(mode!=o->axis_mode){memset(o->delta,0,sizeof o->delta);o->buffer.count=o->buffer.overflow=0;}
        o->axis_mode=mode;
        fprintf(stderr,"[input] mouse axis mode=%s (DIPROP_AXISMODE=%u)\n",mode?"relative":"absolute",mode);
        return 0;
    }
    if(prop==3&&!set){MEM32(address+16)=1;return 0;}
    return DI_UNSUPPORTED;
}
static uint32_t get_state(InputObject *o){
    if(!o->acquired)return DI_NOTACQUIRED;
    uint32_t size=A32(1),out=A32(2);if(size!=o->format.size||!span(out,size))return DI_INVALID;
    sync_user32();memset(hle_ptr(out),0,size);
    if(o->kind==TF_DI_MOUSE&&o->motion_reports<16&&memcmp(o->delta,o->reported_axes,sizeof o->delta)){
        fprintf(stderr,"[input] mouse state mode=%s x=%d y=%d z=%d\n",o->axis_mode?"relative":"absolute",o->delta[0],o->delta[1],o->delta[2]);
        memcpy(o->reported_axes,o->delta,sizeof o->delta);++o->motion_reports;
    }
    for(uint32_t n=0;n<o->format.count;++n){TF_DI_Object item=o->format.objects[n];uint32_t value;
        if(o->kind==TF_DI_KEYBOARD)value=keyboard[item.index];
        else value=item.index<3?(uint32_t)o->delta[item.index]:(mouse_buttons&(1u<<(item.index-3))?0x80u:0);
        if(item.width==4)memcpy(hle_ptr(out+item.offset),&value,4);else MEM8(out+item.offset)=(uint8_t)value;
    }
    if(o->axis_mode)memset(o->delta,0,sizeof o->delta);return 0;
}
static uint32_t get_data(InputObject *o){
    if(!o->acquired)return DI_NOTACQUIRED;if(!o->buffer.requested)return DI_NOTBUFFERED;
    uint32_t stride=A32(1),out=A32(2),count_address=A32(3),flags=A32(4);
    if((stride!=16&&stride!=20)||flags&~1u||!span(count_address,4))return DI_INVALID;
    sync_user32();
    uint32_t wanted=MEM32(count_address),actual=wanted<o->buffer.count?wanted:o->buffer.count;
    if(out&&!span(out,actual*stride))return DI_INVALID;
    TF_DI_Event events[TF_DI_EVENTS];uint32_t status=tf_di_buffer_take(&o->buffer,wanted,flags&1u,events,&actual);
    if(out)for(uint32_t i=0;i<actual;++i)memcpy(hle_ptr(out+i*stride),events+i,stride);
    MEM32(count_address)=actual;return status;
}
static int find_format_index(InputObject *o,uint32_t object,uint32_t how){
    for(uint32_t n=0;n<o->format.count;++n){TF_DI_Object item=o->format.objects[n];
        if(how==1&&item.offset==object)return (int)item.index;
        if(how==2&&(object>>8&0xffffu)==item.index)return (int)item.index;
    }return -1;
}
static void object_info(InputObject *o,uint32_t index,uint32_t out,uint32_t size){
    memset(hle_ptr(out),0,size);MEM32(out)=size;uint32_t offset=index,type=(index<<8)|0x0cu;const uint32_t *guid=key_guid;
    if(o->kind==TF_DI_MOUSE){if(index<3){offset=index*4;type=(index<<8)|1u;guid=index==0?x_guid:index==1?y_guid:z_guid;}
        else{offset=9+index;type=((index-3)<<8)|4u;guid=button_guid;}}
    for(uint32_t n=0;n<o->format.count;++n)if(o->format.objects[n].index==index)offset=o->format.objects[n].offset;
    memcpy(hle_ptr(out+4),guid,16);MEM32(out+20)=offset;MEM32(out+24)=type;
    snprintf(hle_ptr(out+32),260,"%s %u",o->kind==TF_DI_KEYBOARD?"Key":index<3?"Axis":"Button",o->kind==TF_DI_MOUSE&&index>=3?index-3:index);
}
static uint32_t device_method(InputObject *o,uint32_t slot){
    if(slot==3){uint32_t out=A32(1);if(!span(out,4))return DI_INVALID;uint32_t size=MEM32(out);
        if((size!=24&&size!=44)||!span(out,size))return DI_INVALID;memset(hle_ptr(out),0,size);MEM32(out)=size;MEM32(out+4)=5;
        MEM32(out+8)=device_type(o->kind);MEM32(out+12)=o->kind==TF_DI_MOUSE?3:0;MEM32(out+16)=o->kind==TF_DI_MOUSE?8:256;return 0;}
    if(slot==5||slot==6)return property(o,slot==6);
    if(slot==7){if(!o->cooperative||!o->format.size)return DI_NOTINIT;if(o->acquired)return 1;
        for(unsigned i=0;i<24;++i)if(objects[i].va&&objects+i!=o&&objects[i].kind==o->kind&&objects[i].acquired&&((objects[i].cooperative|o->cooperative)&1u))return 0x80070005u;
        o->acquired=1;o->buffer.count=o->buffer.overflow=0;memset(o->delta,0,sizeof o->delta);sync_user32();return 0;}
    if(slot==8){o->acquired=0;o->buffer.count=o->buffer.overflow=0;return 0;}
    if(slot==9)return get_state(o);if(slot==10)return get_data(o);if(slot==11)return set_format(o,A32(1));
    if(slot==12)return A32(1)?DI_UNSUPPORTED:0;
    if(slot==13){uint32_t hwnd=A32(1),flags=A32(2);if(o->acquired)return DI_ACQUIRED;
        if(flags&~31u||((flags&3u)!=1&&(flags&3u)!=2)||((flags&12u)!=4&&(flags&12u)!=8))return DI_INVALID;
        if(!hwnd||hwnd!=tf_user32_first_window())return 0x80070006u;
        o->hwnd=hwnd;o->cooperative=flags;return 0;}
    if(slot==14){uint32_t out=A32(1);if(!span(out,4))return DI_INVALID;uint32_t size=MEM32(out);
        if((size!=292&&size!=316)||!span(out,size))return DI_INVALID;int index=find_format_index(o,A32(2),A32(3));
        if(index<0)return DI_NOTFOUND;object_info(o,(uint32_t)index,out,size);return 0;}
    if(slot==15){uint32_t out=A32(1);if(!span(out,4))return DI_INVALID;uint32_t size=MEM32(out);
        if((size!=560&&size!=580)||!span(out,size))return DI_INVALID;device_info(o->kind,out,size);return 0;}
    if(slot==18){if(span(A32(3),4))MEM32(A32(3))=0;else return DI_INVALID;return DI_UNSUPPORTED;}
    if(slot==19||slot==23)return A32(1)?0:DI_INVALID; /* empty force-feedback enumeration */
    if(slot==25){if(!o->acquired)return DI_NOTACQUIRED;sync_user32();return 0;}
    return DI_UNSUPPORTED;
}
static void enum_objects(InputObject *o){
    uint32_t callback=A32(1),reference=A32(2),flags=A32(3);if(!callback||flags&~0x800000ffu)RET(DI_INVALID,4);
    uint32_t kind=o->kind,mark=hle_scratch_mark(),out=hle_scratch(316),count=kind==TF_DI_KEYBOARD?256:11;
    for(uint32_t index=0;index<count;++index){int axis=kind==TF_DI_MOUSE&&index<3;
        if(flags&&!(flags&(axis?3u:12u)))continue;
        pthread_mutex_lock(&input_lock);object_info(o,index,out,316);pthread_mutex_unlock(&input_lock);
        uint32_t args[2]={out,reference};if(!hle_call_guest(callback,2,args))break;
    }
    hle_scratch_release(mark);RET(0,4);
}
static void dispatch(int factory,uint32_t slot,uint32_t count){
    pthread_mutex_lock(&input_lock);InputObject *o=find_object(A32(0));
    if(!o||(factory?o->kind!=0:o->kind==0)){pthread_mutex_unlock(&input_lock);RET(DI_INVALID,count);}
    if(!factory&&slot==4){pthread_mutex_unlock(&input_lock);enum_objects(o);return;}
    if(factory&&slot>=3){pthread_mutex_unlock(&input_lock);uint32_t result=factory_method(o,slot);RET(result,count);}
    uint32_t result=slot<3?common_method(o,slot):device_method(o,slot);pthread_mutex_unlock(&input_lock);RET(result,count);
}
#define F(slot,count) static void f##slot(void){dispatch(1,slot,count);}
#define D(slot,count) static void d##slot(void){dispatch(0,slot,count);}
F(0,3) F(1,1) F(2,1) F(3,4) F(4,5) F(5,2) F(6,3) F(7,3) F(8,4) F(9,6) F(10,5)
D(0,3) D(1,1) D(2,1) D(3,2) D(4,4) D(5,3) D(6,3) D(7,1) D(8,1) D(9,3) D(10,5)
D(11,2) D(12,2) D(13,3) D(14,4) D(15,2) D(16,3) D(17,4) D(18,5) D(19,4) D(20,3)
D(21,2) D(22,2) D(23,4) D(24,2) D(25,1) D(26,5) D(27,5) D(28,5) D(29,4) D(30,4) D(31,2)
#undef F
#undef D
static void xinput_state(void){RET(A32(0)>3||!span(A32(1),16)?87:1167,2);}
static void xinput_caps(void){RET(A32(0)>3||A32(1)>1||!span(A32(2),20)?87:1167,3);}
static void xinput_set(void){RET(A32(0)>3||!span(A32(1),4)?87:1167,2);}
int tf_input_register(void){
    static int registered;if(registered)return 0;
#define F(n) {"TF.DI8.F." #n,f##n}
#define D(n) {"TF.DI8.D." #n,d##n}
    static const win32hle_shim shims[]={
        {"DirectInput8Create",create_input},{"XInputGetState",xinput_state},{"XInputGetCapabilities",xinput_caps},{"XInputSetState",xinput_set},
        {"xinput1_3.dll#2",xinput_state},{"xinput1_3.dll#3",xinput_set},{"xinput1_3.dll#4",xinput_caps},
        F(0),F(1),F(2),F(3),F(4),F(5),F(6),F(7),F(8),F(9),F(10),
        D(0),D(1),D(2),D(3),D(4),D(5),D(6),D(7),D(8),D(9),D(10),D(11),D(12),D(13),D(14),D(15),
        D(16),D(17),D(18),D(19),D(20),D(21),D(22),D(23),D(24),D(25),D(26),D(27),D(28),D(29),D(30),D(31),{NULL,NULL}};
#undef F
#undef D
    int count=win32hle_register(shims);registered=1;return count;
}
int tf_input_selftest(void){
    TF_DI_Buffer buffer={0};buffer.requested=3;TF_DI_Event event={0,0x80,1,1,0},out[3];uint32_t got;
    tf_di_buffer_push(&buffer,event);event.data=0;event.sequence=2;tf_di_buffer_push(&buffer,event);event.sequence=3;tf_di_buffer_push(&buffer,event);
    if(tf_di_buffer_take(&buffer,3,1,out,&got)!=1||got!=2||out[0].sequence!=2||buffer.count!=2)return 91;
    if(tf_di_buffer_take(&buffer,3,0,out,&got)!=1||got!=2||buffer.count||buffer.overflow)return 92;
    if(tf_di_vk_scan('W')!=0x11||tf_di_vk_scan(37)!=0xcb||tf_di_vk_scan(0x10)!=0x2a)return 93;
    uint32_t header[6]={24,16,2,20,1,1},objects[4]={0,16,0x000004u,0};uint8_t tag=TF_DI_GUID_BUTTON;TF_DI_Format format={0};
    if(tf_di_format(TF_DI_MOUSE,header,objects,&tag,&format)||format.objects[0].index!=3)return 94;
    objects[1]=20;if(tf_di_format(TF_DI_MOUSE,header,objects,&tag,&format)!=DI_INVALID)return 95;
    puts("[input] format bounds, scan codes and buffered input self-test passed; no controller/game execution");return 0;
}
