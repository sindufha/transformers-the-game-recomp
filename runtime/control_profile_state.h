#ifndef TF_CONTROL_PROFILE_STATE_H
#define TF_CONTROL_PROFILE_STATE_H
#include "control_profile.h"
typedef int (*TF_ControlRead)(void*,uint32_t,void*,uint32_t);
typedef struct {
    uint32_t frontend,scene,actor,flags,animation,paused,pending,inhibit;
    uint32_t pause_menu,mission_menu,start_menu,start_phase,confirm_kind,confirm_phase,save_menu,profile;
    uint32_t loading_active,loading_enabled,loading_ready,loading_menu;
} TF_ControlState;
static inline int tf_control_word(TF_ControlRead read,void *ctx,uint32_t va,uint32_t *out){
    unsigned char p[4];if(!va||va>UINT32_MAX-3||!read(ctx,va,p,4))return 0;
    *out=(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;return 1;
}
static inline int tf_control_field(TF_ControlRead read,void *ctx,uint32_t base,uint32_t offset,uint32_t *out){
    return base>=0x10000&&base<=UINT32_MAX-offset-3&&tf_control_word(read,ctx,base+offset,out);
}
static inline int tf_control_prompt(TF_ControlRead read,void *ctx,uint32_t root,
    uint32_t child_offset,uint32_t expected_vtable,uint32_t phase_offset,
    uint32_t expected_phase,uint32_t active_offset,uint32_t *phase){
    uint32_t child=0,vtable=0;unsigned char active=0;
    return tf_control_field(read,ctx,root,child_offset,&child)
        &&tf_control_field(read,ctx,child,0,&vtable)&&vtable==expected_vtable
        &&tf_control_field(read,ctx,child,phase_offset,phase)&&*phase==expected_phase
        &&child<=UINT32_MAX-active_offset&&read(ctx,child+active_offset,&active,1)&&active!=0;
}
/* The exact accepted PE owns these addresses. Named original script bindings
 * RobotForm/CarForm/Transforming and visible GUI controller writes establish
 * their meaning; no transform key or touch-local form guess is used. */
static inline TF_ControlState tf_control_decode(TF_ControlRead read,void *ctx){
    TF_ControlState s={0};unsigned char visible=0;
    if(!read||!tf_control_word(read,ctx,0x71318c,&s.frontend)
       ||!tf_control_word(read,ctx,0x7183a8,&s.scene))return s;
    uint32_t name=0;unsigned char text[9]={0};int valid_scene=0,front_scene=0;
    if(tf_control_field(read,ctx,s.scene,0,&name)&&name&&read(ctx,name,text,1)&&text[0]){
        valid_scene=1;
        if(read(ctx,name,text,9)){
            const unsigned char expected[9]={'f','r','o','n','t','e','n','d',0};front_scene=1;
            for(unsigned i=0;i<9;++i){unsigned char c=text[i];if(c>='A'&&c<='Z')c+=32;if(c!=expected[i])front_scene=0;}
        }
    }
    if(s.frontend||front_scene){s.profile=TF_CONTROL_MENU;return s;}
    if(!read(ctx,0x713677,&visible,1))return s;s.pause_menu=visible!=0;
    // Original save UI owns a separate active byte. Chapter completion sets it
    // at 498359 and waits in phase20 until it clears (498367). Its save-file
    // dialogs still need navigation while the ordinary pause/menu flags are off.
    if(read(ctx,0x71437a,&visible,1))s.save_menu=visible!=0;
    // The original loading loop 5166B0 owns engine flag 0x80 (set 51674A,
    // cleared 516DFF). It draws XtoContinue at 516C2F only after these two
    // DWORDs permit it (516B45-516B5B); the loading worker sets ready at
    // 516416. Actor/menu/pause state alone cannot identify this input wait.
    uint32_t engine_flags=0;
    if(tf_control_word(read,ctx,0x6fb1d0,&engine_flags))s.loading_active=(engine_flags&0x80)!=0;
    if(s.loading_active&&tf_control_word(read,ctx,0x713464,&s.loading_enabled)
       &&tf_control_word(read,ctx,0x713468,&s.loading_ready))
        s.loading_menu=s.loading_enabled!=0&&s.loading_ready!=0;
    uint32_t root=0,panel=0,vtable=0;
    if(tf_control_word(read,ctx,0x713648,&root)&&tf_control_field(read,ctx,root,0x20,&panel)
       &&tf_control_field(read,ctx,panel,0,&vtable)&&vtable==0x6872d0
       &&panel<=UINT32_MAX-12&&read(ctx,panel+12,&visible,1))s.mission_menu=visible!=0;
    uint32_t chapter=0;
    if(tf_control_field(read,ctx,root,0x4c,&chapter)
       &&tf_control_field(read,ctx,chapter,0,&vtable)&&vtable==0x687358
       &&tf_control_field(read,ctx,chapter,0x28,&s.start_phase)
       &&chapter<=UINT32_MAX-0x2c&&read(ctx,chapter+0x2c,&visible,1))
        s.start_menu=visible!=0&&s.start_phase==9;
    // ChapterTextScreen renders its Start Mission prompt outside the normal
    // group-visibility path. Phase 9 explicitly waits for continue/Enter;
    // loading/fade phases and cleanup are not interactive menu states.
    // ChapterCompleteInitial phase 22 waits for Continue/Enter (4982c6).
    // SubMissionCompleteTitle phase 12 waits for its Continue callback (48db00).
    // MissionFailedTitleScreen has a different base layout: phase +30, active
    // +34, and prompt +35. Its phase 7 waits for input only after that prompt.
    uint32_t phase=0,failed=0;unsigned char prompt=0;
    if(tf_control_prompt(read,ctx,root,0x3c,0x687388,0x28,22,0x2c,&phase))s.confirm_kind=1;
    else if(tf_control_prompt(read,ctx,root,0x78,0x686fb0,0x28,12,0x2c,&phase))s.confirm_kind=3;
    else if(tf_control_prompt(read,ctx,root,0x40,0x6873c0,0x30,7,0x34,&phase)
        &&tf_control_field(read,ctx,root,0x40,&failed)&&failed<=UINT32_MAX-0x35
        &&read(ctx,failed+0x35,&prompt,1)&&prompt)s.confirm_kind=2;
    if(s.confirm_kind)s.confirm_phase=phase;
    if(s.pause_menu||s.mission_menu||s.start_menu||s.confirm_kind||s.save_menu||s.loading_menu){s.profile=TF_CONTROL_MENU;return s;}
    if(s.loading_active)return s;
    if(!valid_scene||!tf_control_word(read,ctx,0x6fb0b0,&s.paused)
       ||!tf_control_word(read,ctx,0x6fb0b4,&s.pending)
       ||!tf_control_word(read,ctx,0x7622cc,&s.inhibit)
       ||s.paused||s.pending||s.inhibit)return s;
    uint32_t config=0,config_word=0;
    if(!tf_control_word(read,ctx,0x770614,&s.actor)
       ||!tf_control_field(read,ctx,s.actor,0x518,&config)
       ||!tf_control_field(read,ctx,config,0,&config_word)
       ||!tf_control_field(read,ctx,s.actor,0x628,&s.flags)
       ||!tf_control_field(read,ctx,s.actor,0x1b24,&s.animation))return s;
    // The form bit already changes during conversion. Keep the corresponding
    // controls usable through its animation; Script RobotForm/CarForm predicates
    // also reject transition animations, which is too late for a touch layout.
    s.profile=(s.flags&0x40000)?TF_CONTROL_CAR:TF_CONTROL_ROBOT;return s;
}
#endif
