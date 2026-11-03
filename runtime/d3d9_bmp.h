#ifndef TF_D3D9_BMP_H
#define TF_D3D9_BMP_H
/* Bounded BMP/BI_RGB helpers for D3DX save-game preview images. No host ABI,
 * allocation, packed structures, or unaligned scalar accesses are required. */
#include <stdint.h>
#include <stddef.h>
typedef struct {uint32_t width,height,pitch,offset,bits,top_down;} TF_BMPInfo;
static inline uint32_t tf_bmp_u32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static inline uint32_t tf_bmp_u16(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8;}
static inline void tf_bmp_put32(unsigned char *p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(n>>(i*8));}
static inline uint32_t tf_bmp_size(uint32_t width,uint32_t height){
    if(!width||!height||width>16384||height>16384)return 0;
    return 54+((width*3+3)&~3u)*height;
}
/* Source formats: D3DFMT_A8/X8R8G8B8 and A8/X8B8G8R8. Output is a standard
 * 24-bit bottom-up BMP, with DWORD-aligned rows and no undefined padding. */
static inline int tf_bmp_encode32(const unsigned char *source,uint64_t source_size,
    uint32_t width,uint32_t height,uint32_t pitch,uint32_t format,
    uint32_t left,uint32_t top,uint32_t crop_width,uint32_t crop_height,
    unsigned char *output,uint64_t output_size){
    uint32_t size=tf_bmp_size(crop_width,crop_height);
    if(!source||!output||!size||output_size<size||!width||!height||width>16384||height>16384
       ||pitch<width*4||(format!=21&&format!=22&&format!=32&&format!=33)
       ||left>width||top>height||crop_width>width-left||crop_height>height-top
       ||(uint64_t)(height-1)*pitch+(uint64_t)width*4>source_size)return 0;
    for(unsigned i=0;i<54;++i)output[i]=0;
    output[0]='B';output[1]='M';tf_bmp_put32(output+2,size);tf_bmp_put32(output+10,54);
    tf_bmp_put32(output+14,40);tf_bmp_put32(output+18,crop_width);tf_bmp_put32(output+22,crop_height);
    output[26]=1;output[28]=24;tf_bmp_put32(output+34,size-54);
    uint32_t row=(crop_width*3+3)&~3u;
    for(uint32_t y=0;y<crop_height;++y){
        const unsigned char *src=source+(uint64_t)(top+crop_height-1-y)*pitch+(uint64_t)left*4;
        unsigned char *dst=output+54+(uint64_t)y*row;
        for(uint32_t x=0;x<crop_width;++x){
            dst[x*3]=src[x*4+(format>=32?2:0)];dst[x*3+1]=src[x*4+1];dst[x*3+2]=src[x*4+(format>=32?0:2)];
        }
        for(uint32_t x=crop_width*3;x<row;++x)dst[x]=0;
    }return 1;
}
static inline int tf_bmp_parse(const unsigned char *data,uint64_t size,TF_BMPInfo *out){
    if(!data||!out||size<54||data[0]!='B'||data[1]!='M')return 0;
    uint32_t header=tf_bmp_u32(data+14),file_size=tf_bmp_u32(data+2),offset=tf_bmp_u32(data+10);
    uint32_t width=tf_bmp_u32(data+18),signed_height=tf_bmp_u32(data+22),bits=tf_bmp_u16(data+28);
    uint32_t top_down=signed_height>>31,height=top_down?0u-signed_height:signed_height;
    if(header<40||(uint64_t)header+14>size||offset<(uint64_t)header+14||offset>size||file_size>size
       ||!width||!height||width>16384||height>16384||tf_bmp_u16(data+26)!=1
       ||(bits!=24&&bits!=32)||tf_bmp_u32(data+30)!=0)return 0;
    uint32_t pitch=(width*(bits/8)+3)&~3u;
    uint64_t end=(uint64_t)offset+(uint64_t)pitch*height;
    if(end>size||(file_size&&file_size<end))return 0;
    TF_BMPInfo result={width,height,pitch,offset,bits,top_down};*out=result;return 1;
}
/* BI_RGB has no alpha mask: emit opaque BGRA for both 24- and 32-bit files. */
static inline int tf_bmp_decode(const unsigned char *data,uint64_t size,
    unsigned char *output,uint32_t pitch,uint64_t output_size){
    TF_BMPInfo info;if(!output||!tf_bmp_parse(data,size,&info)||pitch<info.width*4
       ||(uint64_t)(info.height-1)*pitch+(uint64_t)info.width*4>output_size)return 0;
    for(uint32_t y=0;y<info.height;++y){
        const unsigned char *src=data+info.offset+(uint64_t)(info.top_down?y:info.height-1-y)*info.pitch;
        unsigned char *dst=output+(uint64_t)y*pitch;
        for(uint32_t x=0;x<info.width;++x){for(unsigned c=0;c<3;++c)dst[x*4+c]=src[x*(info.bits/8)+c];dst[x*4+3]=255;}
    }return 1;
}
#endif
