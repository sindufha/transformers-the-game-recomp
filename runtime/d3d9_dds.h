#ifndef TF_D3D9_DDS_H
#define TF_D3D9_DDS_H
/* Bounded legacy DDS layout and BC1/2/3 (D3DFMT_DXT1..5) decode helpers.
 * Integer-only, unaligned-safe, independent from the native D3D9 ABI. */
#include <stdint.h>
#include <stddef.h>
typedef struct TF_DDSInfo {uint32_t width,height,levels,format,faces,bytes_per_pixel,block_bytes;} TF_DDSInfo;
typedef struct TF_DDSLevel {uint32_t width,height,pitch,rows;uint64_t offset,size;} TF_DDSLevel;
static inline uint32_t tf_dds_u32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static inline int tf_dds_level(const TF_DDSInfo *info,uint32_t face,uint32_t level,TF_DDSLevel *out){
    if(face>=info->faces||level>=info->levels)return 0;uint64_t offset=128;
    for(uint32_t f=0;f<=face;++f)for(uint32_t m=0;m<info->levels;++m){
        uint32_t w=info->width>>m,h=info->height>>m;if(!w)w=1;if(!h)h=1;
        uint32_t pitch=info->block_bytes?((w+3)/4)*info->block_bytes:w*info->bytes_per_pixel;
        uint32_t rows=info->block_bytes?(h+3)/4:h;uint64_t size=(uint64_t)pitch*rows;
        if(f==face&&m==level){out->width=w;out->height=h;out->pitch=pitch;out->rows=rows;out->offset=offset;out->size=size;return 1;}
        offset+=size;
    }return 0;
}
static inline int tf_dds_parse(const unsigned char *data,uint64_t size,TF_DDSInfo *out){
    if(!data||!out||size<128||tf_dds_u32(data)!=0x20534444u||tf_dds_u32(data+4)!=124||tf_dds_u32(data+76)!=32)return 0;
    TF_DDSInfo r={tf_dds_u32(data+16),tf_dds_u32(data+12),tf_dds_u32(data+28),0,1,0,0};
    if(!r.width||!r.height||r.width>16384||r.height>16384||tf_dds_u32(data+24)>1)return 0;
    if(!r.levels)r.levels=1;uint32_t maximum=1,extent=r.width>r.height?r.width:r.height;
    while(extent>1){++maximum;extent>>=1;}if(r.levels>maximum)return 0;
    uint32_t caps2=tf_dds_u32(data+112);if(caps2&0x200000u)return 0;
    if(caps2&0x200u){if(r.width!=r.height||(caps2&0xfc00u)!=0xfc00u)return 0;r.faces=6;}
    else if(caps2&0xfc00u)return 0;
    uint32_t flags=tf_dds_u32(data+80),fourcc=tf_dds_u32(data+84),bits=tf_dds_u32(data+88);
    uint32_t red=tf_dds_u32(data+92),green=tf_dds_u32(data+96),blue=tf_dds_u32(data+100),alpha=tf_dds_u32(data+104);
    if(flags&4){
        if(fourcc<0x31545844u||fourcc>0x35545844u||((fourcc-0x31545844u)&0x00ffffffu))return 0;
        r.format=fourcc;r.block_bytes=fourcc==0x31545844u?8:16;
    }else if((flags&0x40)&&bits==32&&red==0x00ff0000u&&green==0x0000ff00u&&blue==0x000000ffu&&(alpha==0||alpha==0xff000000u)){
        r.format=alpha?21:22;r.bytes_per_pixel=4;
    }else if((flags&0x40)&&bits==32&&red==0x000000ffu&&green==0x0000ff00u&&blue==0x00ff0000u&&(alpha==0||alpha==0xff000000u)){
        r.format=alpha?32:33;r.bytes_per_pixel=4;
    }else if((flags&0x40)&&bits==24&&red==0x00ff0000u&&green==0x0000ff00u&&blue==0x000000ffu&&!alpha){r.format=20;r.bytes_per_pixel=3;
    }else if((flags&0x40)&&bits==16&&red==0xf800&&green==0x07e0&&blue==0x001f&&!alpha){r.format=23;r.bytes_per_pixel=2;
    }else if((flags&0x40)&&bits==16&&red==0x7c00&&green==0x03e0&&blue==0x001f&&(alpha==0||alpha==0x8000)){r.format=alpha?25:24;r.bytes_per_pixel=2;
    }else if((flags&0x40)&&bits==16&&red==0x0f00&&green==0x00f0&&blue==0x000f&&(alpha==0||alpha==0xf000)){r.format=alpha?26:30;r.bytes_per_pixel=2;
    }else if((flags&0x20000)&&bits==8&&red==0xff&&!alpha){r.format=50;r.bytes_per_pixel=1;
    }else if((flags&2)&&bits==8&&alpha==0xff){r.format=28;r.bytes_per_pixel=1;
    }else return 0;
    if(r.bytes_per_pixel&&(tf_dds_u32(data+8)&8u)&&tf_dds_u32(data+20)!=r.width*r.bytes_per_pixel)return 0;
    TF_DDSLevel last;if(!tf_dds_level(&r,r.faces-1,r.levels-1,&last)||last.offset>size||last.size>size-last.offset)return 0;
    *out=r;return 1;
}
static inline uint32_t tf_bc_565(uint32_t word){
    uint32_t r=(word>>11)&31,g=(word>>5)&63,b=word&31;
    r=(r<<3)|(r>>2);g=(g<<2)|(g>>4);b=(b<<3)|(b>>2);return 0xff000000u|(r<<16)|(g<<8)|b;
}
static inline uint32_t tf_bc_mix(uint32_t a,uint32_t b,uint32_t wa,uint32_t wb,uint32_t denominator){
    uint32_t result=0xff000000u;
    for(unsigned shift=0;shift<=16;shift+=8)result|=((((a>>shift)&255)*wa+((b>>shift)&255)*wb)/denominator)<<shift;
    return result;
}
static inline void tf_bc_decode_block(uint32_t format,const unsigned char *source,uint32_t out[16]){
    const unsigned char *color=source+(format==0x31545844u?0:8);
    uint32_t a=(uint32_t)color[0]|(uint32_t)color[1]<<8,b=(uint32_t)color[2]|(uint32_t)color[3]<<8;
    uint32_t palette[4]={tf_bc_565(a),tf_bc_565(b),0,0};
    if(a>b||format!=0x31545844u){palette[2]=tf_bc_mix(palette[0],palette[1],2,1,3);palette[3]=tf_bc_mix(palette[0],palette[1],1,2,3);}
    else {palette[2]=tf_bc_mix(palette[0],palette[1],1,1,2);palette[3]=0;}
    uint32_t indices=tf_dds_u32(color+4);for(unsigned i=0;i<16;++i)out[i]=palette[(indices>>(2*i))&3];
    if(format==0x32545844u||format==0x33545844u){
        for(unsigned i=0;i<16;++i){uint32_t alpha=(source[i/2]>>((i&1)*4))&15;out[i]=(out[i]&0xffffffu)|((alpha*17)<<24);}
    }else if(format==0x34545844u||format==0x35545844u){
        uint32_t alpha[8]={source[0],source[1],0,0,0,0,0,0};
        if(alpha[0]>alpha[1])for(unsigned i=1;i<=6;++i)alpha[i+1]=(alpha[0]*(7-i)+alpha[1]*i)/7;
        else {for(unsigned i=1;i<=4;++i)alpha[i+1]=(alpha[0]*(5-i)+alpha[1]*i)/5;alpha[6]=0;alpha[7]=255;}
        uint64_t bits=0;for(unsigned i=0;i<6;++i)bits|=(uint64_t)source[i+2]<<(8*i);
        for(unsigned i=0;i<16;++i)out[i]=(out[i]&0xffffffu)|(alpha[(bits>>(3*i))&7]<<24);
    }
}
/* Output is little-endian D3DFMT_A8R8G8B8 (BGRA bytes). */
static inline int tf_bc_decode(uint32_t format,uint32_t width,uint32_t height,const unsigned char *source,
    uint64_t source_size,unsigned char *output,uint32_t output_pitch,uint64_t output_size){
    if(format<0x31545844u||format>0x35545844u||((format-0x31545844u)&0x00ffffffu)||!width||!height||width>16384||height>16384||!source||!output||output_pitch<width*4)return 0;
    uint32_t bytes=format==0x31545844u?8:16,bw=(width+3)/4,bh=(height+3)/4;
    if((uint64_t)bw*bh*bytes>source_size||(uint64_t)output_pitch*(height-1)+(uint64_t)width*4>output_size)return 0;
    for(uint32_t by=0;by<bh;++by)for(uint32_t bx=0;bx<bw;++bx){
        uint32_t pixels[16];tf_bc_decode_block(format,source+((uint64_t)by*bw+bx)*bytes,pixels);
        for(uint32_t y=0;y<4&&by*4+y<height;++y)for(uint32_t x=0;x<4&&bx*4+x<width;++x){
            uint32_t pixel=pixels[y*4+x];unsigned char *p=output+(uint64_t)(by*4+y)*output_pitch+(bx*4+x)*4;
            p[0]=(unsigned char)pixel;p[1]=(unsigned char)(pixel>>8);p[2]=(unsigned char)(pixel>>16);p[3]=(unsigned char)(pixel>>24);
        }
    }return 1;
}
#endif
