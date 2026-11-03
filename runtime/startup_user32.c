/* Bounded virtual USER32 and COM-apartment startup model for Transformers.
 * It supplies state and callback delivery; it does not claim a host desktop,
 * draw a message box/icon, instantiate missing COM classes, or change Android
 * accessibility/screensaver settings. Unsupported requests fail explicitly. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "win32hle.h"
#include "startup_user32.h"
#include "startup_user32_state.h"

enum { MAX_CLASSES=32, MAX_WINDOWS=32 };
typedef struct { uint32_t atom,proc,style,instance,cursor; char name[128]; } WindowClass;
typedef struct {
    uint32_t handle,cls,style,exstyle,parent,instance,menu,thread;
    int32_t x,y,width,height;
    int shown,iconic,dirty,destroying;
} Window;
static WindowClass classes[MAX_CLASSES];
static Window windows[MAX_WINDOWS];
static TF_User32Queue queue;
static uint32_t class_count,window_serial,foreground,cursor;
static int screen_width=640,screen_height=480,cursor_x,cursor_y,screen_saver=1;
static uint32_t sticky[2]={8,0},toggle[2]={8,0},filter[6]={24,0,0,0,0,0};
static void (*event_pump)(void);
static uint8_t keys[256];
static pthread_mutex_t input_mutex=PTHREAD_MUTEX_INITIALIZER;
static struct {uint32_t key;int down;} pending_keys[256];
static unsigned pending_count;
/* COM identity follows guest thread IDs, not the host callback thread. */
static struct {uint32_t tid;TF_ComApartment apartment;} apartments[32];

static Window *window(uint32_t h) {
    if(!h)return NULL;
    for(unsigned i=0;i<MAX_WINDOWS;++i)if(windows[i].handle==h)return windows+i;
    return NULL;
}
static int same_class_name(const char *a,const char *b) {
    if(!a || !b)return 0;
    for(unsigned i=0;i<128;++i){unsigned x=(unsigned char)a[i],y=(unsigned char)b[i];
        if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;
        if(x!=y)return 0;if(!x)return 1;}
    return 0;
}
static int copy_name(char *out,const char *name) {
    if(!name)return 0;
    for(unsigned i=0;i<128;++i){out[i]=name[i];if(!name[i])return i!=0;}
    return 0;
}
static int find_class(uint32_t name) {
    for(unsigned i=0;i<class_count;++i)
        if(name<0x10000u ? classes[i].atom==name : same_class_name(classes[i].name,hle_ptr(name)))return (int)i;
    return -1;
}
static uint32_t deliver(Window *w,uint32_t message,uint32_t wp,uint32_t lp) {
    uint32_t a[4]={w->handle,message,wp,lp};
    return hle_call_guest(classes[w->cls].proc,4,a);
}
static uint32_t ticks(void){return hle_ticks_ms();}
void tf_user32_set_display(int width,int height) {
    if(width>0&&width<=32767&&height>0&&height<=32767){screen_width=width;screen_height=height;}
}
void tf_user32_set_pump(void (*pump)(void)){event_pump=pump;}
uint32_t tf_user32_first_window(void) {
    for(unsigned i=0;i<MAX_WINDOWS;++i)if(windows[i].handle&&!windows[i].parent)return windows[i].handle;
    return 0;
}
int tf_user32_post(uint32_t hwnd,uint32_t message,uint32_t wp,uint32_t lp) {
    if(hwnd&&!window(hwnd)){hle_set_last_error(1400);return 0;}
    TF_User32Message m={hwnd,message,wp,lp,ticks(),cursor_x,cursor_y};
    if(!tf_user32_queue_add(&queue,m)){hle_set_last_error(1816);return 0;}return 1;
}
int tf_user32_key_event(uint32_t virtual_key,int down) {
    if(virtual_key>255)return 0;
    pthread_mutex_lock(&input_mutex);
    if(pending_count==256){pthread_mutex_unlock(&input_mutex);return 0;}
    pending_keys[pending_count].key=virtual_key;pending_keys[pending_count].down=down!=0;++pending_count;
    pthread_mutex_unlock(&input_mutex);return 1;
}
int tf_user32_key_down(uint32_t virtual_key){return virtual_key<256&&keys[virtual_key]!=0;}
static void ingest_keys(void) {
    pthread_mutex_lock(&input_mutex);
    /* Leave overflowed keys pending; never silently lose a key-up. */
    unsigned done=0;
    for(;done<pending_count&&queue.count<TF_USER32_QUEUE_CAPACITY;++done){
        uint32_t vk=pending_keys[done].key;int down=pending_keys[done].down,previous=keys[vk];keys[vk]=(uint8_t)down;
        uint32_t lp=1u|(previous?0x40000000u:0)|(down?0:0x80000000u);
        TF_User32Message m={foreground?foreground:tf_user32_first_window(),down?0x100u:0x101u,vk,lp,ticks(),cursor_x,cursor_y};
        tf_user32_queue_add(&queue,m);
    }
    for(unsigned i=done;i<pending_count;++i)pending_keys[i-done]=pending_keys[i];pending_count-=done;
    pthread_mutex_unlock(&input_mutex);
}
/* Public names already declared by the pinned runtime; useful to presenters. */
uint32_t hle_first_hwnd(void){return tf_user32_first_window();}
void hle_set_screen_size(int w,int h){tf_user32_set_display(w,h);}
void hle_set_pump_hook(void (*fn)(void)){tf_user32_set_pump(fn);}
void hle_post_message(uint32_t h,uint32_t m,uint32_t w,uint32_t l){
    if(!tf_user32_post(h,m,w,l))hle_fatal("USER32 host event rejected: hwnd=%08X message=%04X",h,m);
}

static void system_parameters(void) {
    uint32_t action=A32(0),size=A32(1),address=A32(2);
    if(action==0x10u){if(!address){hle_set_last_error(87);RET(0,4);}MEM32(address)=(uint32_t)screen_saver;RET(1,4);}
    if(action==0x11u){screen_saver=size!=0;RET(1,4);}
    uint32_t *state=NULL;unsigned expected=0;int set=0;
    switch(action){
    case 0x3a:state=sticky;expected=8;break;case 0x3b:state=sticky;expected=8;set=1;break;
    case 0x34:state=toggle;expected=8;break;case 0x35:state=toggle;expected=8;set=1;break;
    case 0x32:state=filter;expected=24;break;case 0x33:state=filter;expected=24;set=1;break;
    default:hle_set_last_error(120);RET(0,4);
    }
    if(!address||size!=expected||MEM32(address)!=expected){hle_set_last_error(87);RET(0,4);}
    if(set)memcpy(state,hle_ptr(address),expected);else memcpy(hle_ptr(address),state,expected);
    RET(1,4);
}
static void register_class(void) {
    uint32_t va=A32(0);
    if(!va||MEM32(va)!=48u||!MEM32(va+8)||MEM32(va+12)||MEM32(va+16)){
        hle_set_last_error(87);RET(0,1);
    }
    uint32_t name=MEM32(va+40);
    if(name<0x10000u){hle_set_last_error(87);RET(0,1);}
    if(find_class(name)>=0){hle_set_last_error(1410);RET(0,1);}
    if(class_count==MAX_CLASSES){hle_set_last_error(8);RET(0,1);}
    WindowClass *c=classes+class_count;
    if(!copy_name(c->name,hle_ptr(name))){hle_set_last_error(87);RET(0,1);}
    c->atom=0xc000u+class_count;c->proc=MEM32(va+8);c->style=MEM32(va+4);
    c->instance=MEM32(va+20);c->cursor=MEM32(va+28);++class_count;RET(c->atom,1);
}
static int destroy(Window *w) {
    if(!w||w->destroying)return 0;
    uint32_t handle=w->handle;w->destroying=1;
    for(unsigned i=0;i<MAX_WINDOWS;++i)if(windows[i].handle&&windows[i].parent==handle)destroy(windows+i);
    deliver(w,2,0,0);deliver(w,0x82,0,0);
    for(unsigned i=0;i<queue.count;){if(queue.items[i].hwnd==handle){for(unsigned j=i+1;j<queue.count;++j)queue.items[j-1]=queue.items[j];--queue.count;}else ++i;}
    if(foreground==handle)foreground=0;memset(w,0,sizeof *w);return 1;
}
static void create_window(void) {
    uint32_t args[12];for(unsigned i=0;i<12;++i)args[i]=A32((int)i);
    int cls=find_class(args[1]);
    if(cls<0){hle_set_last_error(1407);RET(0,12);}
    if(args[8]&&!window(args[8])){hle_set_last_error(1400);RET(0,12);}
    if(!(classes[cls].style&0x4000u)&&classes[cls].instance!=args[10]){hle_set_last_error(1407);RET(0,12);}
    unsigned i;for(i=0;i<MAX_WINDOWS;++i)if(!windows[i].handle)break;
    if(i==MAX_WINDOWS){hle_set_last_error(8);RET(0,12);}
    Window *w=windows+i;memset(w,0,sizeof *w);w->handle=0x10000u+16u*(++window_serial);
    w->cls=(uint32_t)cls;w->style=args[3];w->exstyle=args[0];w->parent=args[8];w->menu=args[9];w->instance=args[10];w->thread=hle_thread_id();
    w->x=args[4]==0x80000000u?0:(int32_t)args[4];w->y=args[5]==0x80000000u?0:(int32_t)args[5];
    w->width=args[6]==0x80000000u?screen_width:(int32_t)args[6];w->height=args[7]==0x80000000u?screen_height:(int32_t)args[7];
    if(w->width<0||w->height<0){memset(w,0,sizeof *w);hle_set_last_error(87);RET(0,12);}
    uint32_t handle=w->handle,mark=hle_scratch_mark(),va=hle_scratch(48);
    uint32_t cs[12]={args[11],args[10],args[9],args[8],(uint32_t)w->height,(uint32_t)w->width,(uint32_t)w->y,(uint32_t)w->x,args[3],args[2],args[1],args[0]};
    memcpy(hle_ptr(va),cs,48);
    uint32_t ok=deliver(w,0x81,0,va);
    if(!ok||!w->handle){if(w->handle){deliver(w,0x82,0,0);memset(w,0,sizeof *w);}hle_scratch_release(mark);RET(0,12);}
    uint32_t created=deliver(w,1,0,va);hle_scratch_release(mark);
    if(created==0xffffffffu||!w->handle){if(w->handle)destroy(w);RET(0,12);}
    w->shown=(w->style&0x10000000u)!=0;w->dirty=1;
    if(w->shown){deliver(w,0x18,1,0);if(w->handle)deliver(w,5,0,((uint32_t)w->height<<16)|((uint32_t)w->width&0xffffu));}
    RET(w->handle?handle:0,12);
}
static void def_window_proc(void) {
    uint32_t h=A32(0),msg=A32(1);Window *w=window(h);
    if(msg==0x81u)RET(1,4);
    if(msg==0x10u){if(w)destroy(w);RET(0,4);}
    if(msg==0x84u)RET(1,4); /* HTCLIENT: virtual surface has no non-client drawing */
    if(msg==0xfu&&w)w->dirty=0;
    RET(0,4);
}
static void destroy_window(void){Window *w=window(A32(0));if(!w){hle_set_last_error(1400);RET(0,1);}if(w->thread!=hle_thread_id()){hle_set_last_error(5);RET(0,1);}RET(destroy(w),1);}
static void show_window(void) {
    Window *w=window(A32(0));if(!w){hle_set_last_error(1400);RET(0,2);}
    uint32_t cmd=A32(1);int previous=w->shown,shown=cmd!=0;w->shown=shown;
    w->iconic=cmd==2||cmd==6||cmd==7||cmd==11;
    if(shown)w->style|=0x10000000u;else w->style&=~0x10000000u;
    if(previous!=shown)deliver(w,0x18,(uint32_t)shown,0);
    RET(previous,2);
}
static void update_window(void){Window *w=window(A32(0));if(!w){hle_set_last_error(1400);RET(0,1);}if(w->dirty){w->dirty=0;deliver(w,0xf,0,0);}RET(1,1);}
static void move_window(void) {
    Window *w=window(A32(0));if(!w){hle_set_last_error(1400);RET(0,6);}
    int32_t x=(int32_t)A32(1),y=(int32_t)A32(2),width=(int32_t)A32(3),height=(int32_t)A32(4);int repaint=A32(5)!=0;
    if(width<0||height<0){hle_set_last_error(87);RET(0,6);}
    int moved=x!=w->x||y!=w->y,sized=width!=w->width||height!=w->height;w->x=x;w->y=y;w->width=width;w->height=height;
    if(repaint)w->dirty=1;
    if(moved)deliver(w,3,0,((uint32_t)y<<16)|((uint32_t)x&0xffffu));
    if(sized&&w->handle)deliver(w,5,w->iconic?1:0,((uint32_t)height<<16)|((uint32_t)width&0xffffu));
    RET(1,6);
}
static void adjust_window_rect(void) {
    uint32_t va=A32(0),style=A32(1);int menu=A32(2)!=0;
    if(!va){hle_set_last_error(87);RET(0,3);}
    int border=(style&0x40000u)?4:((style&0xc00000u)?1:0),caption=(style&0xc00000u)==0xc00000u?19:0;
    MEM32(va)-=(uint32_t)border;MEM32(va+4)-=(uint32_t)(border+caption+(menu?19:0));MEM32(va+8)+=(uint32_t)border;MEM32(va+12)+=(uint32_t)border;RET(1,3);
}
static void post_quit(void){tf_user32_post(0,0x12,A32(0),0);RETV(1);}
static void post_message(void){RET(tf_user32_post(A32(0),A32(1),A32(2),A32(3)),4);}
static void peek_message(void) {
    uint32_t va=A32(0),hwnd=A32(1),low=A32(2),high=A32(3),flags=A32(4);
    if(!va){hle_set_last_error(87);RET(0,5);}
    if(hwnd&&hwnd!=0xffffffffu&&!window(hwnd)){hle_set_last_error(1400);RET(0,5);}
    if(flags&~3u){hle_set_last_error(87);RET(0,5);}
    if(event_pump)event_pump();
    TF_User32Message m;
    ingest_keys();
    if(!tf_user32_queue_peek(&queue,hwnd,low,high,(flags&1)!=0,&m))RET(0,5);
    memcpy(hle_ptr(va),&m,28);RET(1,5);
}
static void translate_message(void) {
    uint32_t va=A32(0);if(!va){hle_set_last_error(87);RET(0,1);}
    /* Unicode/text input is posted by the Android input adapter. No invented
     * keyboard-layout translation from virtual key codes. */
    uint32_t message=MEM32(va+4);RET(message==0x100||message==0x101||message==0x104||message==0x105,1);
}
static void dispatch_message(void) {
    uint32_t va=A32(0);if(!va){hle_set_last_error(87);RET(0,1);}
    Window *w=window(MEM32(va));if(!w)RET(0,1);
    if(w->thread!=hle_thread_id())hle_fatal("cross-thread USER32 dispatch is unsupported");
    RET(deliver(w,MEM32(va+4),MEM32(va+8),MEM32(va+12)),1);
}
static void send_message(void){Window *w=window(A32(0));if(!w){hle_set_last_error(1400);RET(0,4);}if(w->thread!=hle_thread_id())hle_fatal("cross-thread SendMessageA is unsupported");RET(deliver(w,A32(1),A32(2),A32(3)),4);}
static void set_foreground(void) {
    Window *w=window(A32(0));if(!w){hle_set_last_error(1400);RET(0,1);}
    if(foreground!=w->handle){Window *previous=window(foreground);uint32_t old=foreground;foreground=w->handle;
        if(previous){deliver(previous,6,0,w->handle);if(previous->handle)deliver(previous,8,w->handle,0);}
        if(w->handle){deliver(w,6,1,old);if(w->handle)deliver(w,7,old,0);}}
    RET(1,1);
}
static void is_iconic(void){Window *w=window(A32(0));RET(w&&w->iconic,1);}
static void set_cursor_pos(void){cursor_x=(int32_t)A32(0);cursor_y=(int32_t)A32(1);RET(1,2);}
static void set_cursor(void){uint32_t old=cursor;cursor=A32(0);RET(old,1);}
static void load_cursor(void) {
    uint32_t instance=A32(0),name=A32(1);
    if(!instance&&name>=32512u&&name<=32672u)RET(0x1c000000u|name,2);
    hle_set_last_error(120);RET(0,2); /* custom resource decoding is not present */
}
static void load_icon(void){hle_set_last_error(120);RET(0,2);}
static void message_box(void){fprintf(stderr,"[USER32] MessageBoxA: %s: %s (no Android dialog adapter)\n",A32(2)?ASTR(2):"",A32(1)?ASTR(1):"");hle_set_last_error(120);RET(0,4);}
static void system_metrics(void) {
    switch(A32(0)){
    case 0:case 16:case 78:RET(screen_width,1);case 1:case 17:case 79:RET(screen_height,1);
    case 4:RET(19,1);case 5:case 6:RET(1,1);case 7:case 8:RET(3,1);case 32:case 33:RET(4,1);
    case 11:case 12:case 13:case 14:RET(32,1);case 80:RET(1,1);
    default:RET(0,1); /* unsupported optional capability: absent in virtual display */
    }
}
static TF_ComApartment *apartment(int create) {
    uint32_t tid=hle_thread_id();
    for(unsigned i=0;i<32;++i)if(apartments[i].tid==tid)return &apartments[i].apartment;
    if(create)for(unsigned i=0;i<32;++i)if(!apartments[i].tid){apartments[i].tid=tid;return &apartments[i].apartment;}
    return NULL;
}
static void co_initialize_ex(void){TF_ComApartment *s=apartment(1);RET(s?tf_com_initialize(s,A32(0),A32(1)):0x8007000eu,2);}
static void co_initialize(void){TF_ComApartment *s=apartment(1);RET(s?tf_com_initialize(s,A32(0),2):0x8007000eu,1);}
static void co_uninitialize(void){TF_ComApartment *s=apartment(0);if(s)tf_com_uninitialize(s);RETV(0);}
static void co_create_instance(void){if(A32(4))MEM32(A32(4))=0;else RET(0x80004003u,5);TF_ComApartment *s=apartment(0);RET(s&&s->count?0x80040154u:0x800401f0u,5);}
static void multimedia_time(void){RET(hle_ticks_ms(),0);}

int tf_user32_register(void) {
    static int registered;
    if(registered)return 0;
    const win32hle_shim shims[]={
        {"SystemParametersInfoA",system_parameters},{"RegisterClassExA",register_class},{"CreateWindowExA",create_window},
        {"DefWindowProcA",def_window_proc},{"DestroyWindow",destroy_window},{"ShowWindow",show_window},
        {"UpdateWindow",update_window},{"MoveWindow",move_window},{"AdjustWindowRect",adjust_window_rect},
        {"PostQuitMessage",post_quit},{"PostMessageA",post_message},{"PeekMessageA",peek_message},
        {"TranslateMessage",translate_message},{"DispatchMessageA",dispatch_message},{"SendMessageA",send_message},
        {"SetForegroundWindow",set_foreground},{"IsIconic",is_iconic},{"GetSystemMetrics",system_metrics},
        {"SetCursorPos",set_cursor_pos},{"SetCursor",set_cursor},{"LoadCursorA",load_cursor},{"LoadIconA",load_icon},{"MessageBoxA",message_box},
        {"CoInitialize",co_initialize},{"CoInitializeEx",co_initialize_ex},{"CoUninitialize",co_uninitialize},{"CoCreateInstance",co_create_instance},
        {"timeGetTime",multimedia_time},
        {NULL,NULL}
    };
    registered=1;return win32hle_register(shims);
}
int tf_user32_selftest(void) {
    TF_User32Queue q={0};TF_User32Message in={0x10010,0x100,65,0,0,0,0},out;
    if(!tf_user32_queue_add(&q,in)||!tf_user32_queue_peek(&q,0,0,0,0,&out)||q.count!=1||out.wparam!=65)return 61;
    if(!tf_user32_queue_peek(&q,in.hwnd,0x100,0x100,1,&out)||q.count)return 62;
    TF_ComApartment s={0};if(tf_com_initialize(&s,0,2)||tf_com_initialize(&s,0,2)!=1||tf_com_initialize(&s,0,0)!=0x80010106u)return 63;
    tf_com_uninitialize(&s);tf_com_uninitialize(&s);if(tf_com_initialize(&s,0,0))return 64;
    puts("[startup] virtual USER32 queue and COM apartment self-test passed; no game executed");return 0;
}
