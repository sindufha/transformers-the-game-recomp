#ifndef TF_D3D9_SHADER_H
#define TF_D3D9_SHADER_H
#include <stdint.h>
#include <stddef.h>

/* D3D token layout, corroborated with the pinned DXSO decoder/tables.
 * SM1 has implicit operand counts; SM2/3 encode the instruction length.
 * Counts exclude the opcode word. This bounds bytecode, not shader semantics. */
static inline int tf_d3d9_shader_version(uint32_t version) {
    uint32_t kind=version>>16,major=(version>>8)&255,minor=version&255;
    if(kind!=0xfffeu&&kind!=0xffffu)return 0;
    if(major==1)return kind==0xfffeu?minor==1:minor<=4;
    return major==2||major==3;
}
static inline int tf_d3d9_shader_words(uint32_t version,uint32_t token,uint32_t *words) {
    if(!tf_d3d9_shader_version(version))return 0;
    uint32_t opcode=token&0xffffu,major=(version>>8)&255;
    if(opcode==0xffffu){if(token!=0xffffu)return 0;*words=0;return 1;}
    if(opcode==0xfffeu){*words=(token>>16)&0x7fffu;return 1;}
    if(opcode==0xfffdu){if(version!=0xffff0104u)return 0;*words=0;return 1;}
    if(major>=2){*words=(token>>24)&15;return 1;}
    static const uint8_t arithmetic[]={
        0,2,3,3,4,3,2,2,3,3,3,3,3,3,2,2,2,3,4,2,3,3,3,3,3,
        1,2,2,0,0,1,2,3,3,4,2,2,4,1,0,1,2,0,0,0,2,2,2,5
    };
    static const uint8_t texture[]={
        1,1,1,2,2,2,2,2,2,2,2,255,3,2,2,2,4,5,2,2,2,2,2,1,4,3,4,2,2,5,3,3,2
    };
    uint32_t count=255;
    if(opcode<sizeof arithmetic)count=arithmetic[opcode];
    else if(opcode>=64&&opcode-64<sizeof texture)count=texture[opcode-64];
    if(count==255)return 0;
    if(version==0xffff0104u&&(opcode==64||opcode==66))++count;
    *words=count;return 1;
}
static inline int tf_d3d9_shader_span(const uint32_t *code,size_t available,size_t *consumed) {
    if(!code||available<2||!tf_d3d9_shader_version(code[0]))return 0;
    size_t at=1;
    while(at<available){
        uint32_t token=code[at],words;
        if(!tf_d3d9_shader_words(code[0],token,&words)||words>available-at-1)return 0;
        if(token==0xffffu){*consumed=at+1;return 1;}
        at+=1+words;
    }
    return 0;
}
#endif
