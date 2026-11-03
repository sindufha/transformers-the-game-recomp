// Published ID3DXConstantTable guest32 stdcall method order.
#define TF_CONSTANT_METHODS(X) \
 X(0,3) X(1,1) X(2,1) X(3,1) X(4,1) X(5,2) X(6,4) X(7,2) \
 X(8,3) X(9,3) X(10,3) X(11,2) X(12,5) X(13,4) X(14,5) \
 X(15,4) X(16,5) X(17,4) X(18,5) X(19,4) X(20,5) X(21,4) \
 X(22,5) X(23,5) X(24,4) X(25,5) X(26,5)
#define TF_CONSTANT_WRAPPER(slot,count) static void tf_constant_##slot(){tf_d3d9_hle_method(17,slot,count);}
TF_CONSTANT_METHODS(TF_CONSTANT_WRAPPER)
#undef TF_CONSTANT_WRAPPER
#define TF_CONSTANT_EXPORT(slot,count) {"TF.D3D9.17." #slot,tf_constant_##slot},
static const win32hle_shim tf_constant_shims[]={TF_CONSTANT_METHODS(TF_CONSTANT_EXPORT) {nullptr,nullptr}};
#undef TF_CONSTANT_EXPORT
#undef TF_CONSTANT_METHODS
