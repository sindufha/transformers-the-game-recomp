#ifndef TF_D3D9_MATH_H
#define TF_D3D9_MATH_H
/* D3DX row-vector matrices, with alias-safe outputs. No host SIMD dependency. */
static inline void tf_matrix_multiply(float *out,const float *a,const float *b) {
    float r[16];
    for(unsigned row=0;row<4;++row) for(unsigned column=0;column<4;++column) {
        float sum=0;
        for(unsigned k=0;k<4;++k) sum+=a[row*4+k]*b[k*4+column];
        r[row*4+column]=sum;
    }
    for(unsigned i=0;i<16;++i)out[i]=r[i];
}
static inline void tf_matrix_ortho_lh(float *out,float width,float height,float near_z,float far_z) {
    for(unsigned i=0;i<16;++i)out[i]=0;
    out[0]=2.0f/width;out[5]=2.0f/height;out[10]=1.0f/(far_z-near_z);
    out[14]=-near_z/(far_z-near_z);out[15]=1;
}
#endif
