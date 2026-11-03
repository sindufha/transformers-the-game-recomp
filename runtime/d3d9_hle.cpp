#include "d3d9_bridge.h"
#include <cstdio>
#include <exception>
extern "C" {
#include "win32hle.h"
}
static void tf_d3d9_hle_method(uint32_t kind,uint32_t method,uint32_t count) {
    // At most 15 DWORDs including 'this' in the base D3D9 interfaces.
    uint32_t arguments[16]={0};if(count>16)hle_fatal("D3D9 invalid stack argument count");
    for(uint32_t i=0;i<count;++i)arguments[i]=A32(i);
    try {uint32_t actual=0,result=tf_d3d9_dispatch(kind,method,arguments,&actual);
        if(actual!=count)hle_fatal("D3D9 internal stdcall mismatch kind %u slot %u",kind,method);
        RET(result,count);
    }catch(const std::exception& error){hle_fatal("%s",error.what());}
    hle_fatal("D3D9 bridge returned after fatal error");
}
#include "d3d9_wrappers.generated.inc"
#include "d3d9_constants_shims.inc"
static void tf_d3d9_buffer_0(){tf_d3d9_hle_method(16,0,3);}
static void tf_d3d9_buffer_1(){tf_d3d9_hle_method(16,1,1);}
static void tf_d3d9_buffer_2(){tf_d3d9_hle_method(16,2,1);}
static void tf_d3d9_buffer_3(){tf_d3d9_hle_method(16,3,1);}
static void tf_d3d9_buffer_4(){tf_d3d9_hle_method(16,4,1);}
static void create_d3d9(){try{auto result=tf_d3d9_create(A32(0));RET(result,1);}catch(const std::exception& error){hle_fatal("%s",error.what());}}
static void d3dx_operation(uint32_t operation,uint32_t count){
    uint32_t arguments[16]={0};for(uint32_t i=0;i<count;++i)arguments[i]=A32(i);
    try{uint32_t actual=0,result=tf_d3dx9_dispatch(operation,arguments,&actual);
        if(actual!=count)hle_fatal("D3DX internal stdcall mismatch operation %u",operation);RET(result,count);
    }catch(const std::exception& error){hle_fatal("%s",error.what());}
}
static void matrix_ortho(){d3dx_operation(0,5);}
static void matrix_multiply(){d3dx_operation(1,3);}
static void create_buffer(){d3dx_operation(2,2);}
static void compile_shader(){d3dx_operation(3,10);}
static void effect_compiler(){d3dx_operation(4,7);}
static void texture_memory(){d3dx_operation(5,4);}
static void texture_memory_ex(){d3dx_operation(6,15);}
static void cube_memory(){d3dx_operation(7,4);}
static void save_surface_memory(){d3dx_operation(8,5);}
extern "C" int tf_d3d9_register(void){
    static int registered=0;if(registered)return 0;
    const win32hle_shim exports[]={
        {"Direct3DCreate9",create_d3d9},{"D3DXMatrixOrthoLH",matrix_ortho},{"D3DXMatrixMultiply",matrix_multiply},
        {"D3DXCreateBuffer",create_buffer},{"D3DXCompileShader",compile_shader},{"D3DXCreateEffectCompiler",effect_compiler},
        {"D3DXCreateTextureFromFileInMemory",texture_memory},{"D3DXCreateTextureFromFileInMemoryEx",texture_memory_ex},
        {"D3DXCreateCubeTextureFromFileInMemory",cube_memory},{"D3DXSaveSurfaceToFileInMemory",save_surface_memory},
        {"TF.D3D9.16.0",tf_d3d9_buffer_0},{"TF.D3D9.16.1",tf_d3d9_buffer_1},{"TF.D3D9.16.2",tf_d3d9_buffer_2},
        {"TF.D3D9.16.3",tf_d3d9_buffer_3},{"TF.D3D9.16.4",tf_d3d9_buffer_4},{nullptr,nullptr}};
    int count=win32hle_register(tf_d3d9_method_shims)+win32hle_register(exports)+win32hle_register(tf_constant_shims);registered=1;return count;
}
extern "C" uint32_t tf_d3d9_method_address(uint32_t kind,uint32_t method){
    char name[48];std::snprintf(name,sizeof name,"TF.D3D9.%u.%u",kind,method);return hle_resolve(name);
}
extern "C" uint32_t tf_d3d9_smoke(uint32_t width,uint32_t height){
    uint32_t temporary=hle_va(hle_alloc(64));if(!temporary)return 0x8007000eu;
    uint32_t arguments[8]={32};uint32_t d3d=hle_call_guest(hle_resolve("Direct3DCreate9"),1,arguments);
    if(!d3d){hle_free(hle_ptr(temporary));return 0x8876086au;}
    // D3DPRESENT_PARAMETERS32: 14 DWORDs; discard, windowed, interval one.
    MEM32(temporary)=width;MEM32(temporary+4)=height;MEM32(temporary+8)=22;
    MEM32(temporary+12)=1;MEM32(temporary+24)=1;MEM32(temporary+32)=1;MEM32(temporary+52)=1;
    arguments[0]=d3d;arguments[1]=0;arguments[2]=1;arguments[3]=0;arguments[4]=0x40;
    arguments[5]=temporary;arguments[6]=temporary+56;
    uint32_t status=hle_call_guest(tf_d3d9_method_address(1,16),7,arguments);
    uint32_t device=MEM32(temporary+56);
    if(!(status&0x80000000u)&&device){
        uint32_t clear[7]={device,0,0,1,0xff15334du,0x3f800000u,0};
        status=hle_call_guest(tf_d3d9_method_address(2,43),7,clear);
        if(!(status&0x80000000u)){
            uint32_t present[5]={device,0,0,0,0};
            status=hle_call_guest(tf_d3d9_method_address(2,17),5,present);
        }
        uint32_t release[1]={device};hle_call_guest(tf_d3d9_method_address(2,2),1,release);
    }
    uint32_t release[1]={d3d};hle_call_guest(tf_d3d9_method_address(1,2),1,release);hle_free(hle_ptr(temporary));
    std::fprintf(stderr,"[d3d9] guest32 CreateDevice/Clear/Present smoke status=%08x\n",status);return status;
}
