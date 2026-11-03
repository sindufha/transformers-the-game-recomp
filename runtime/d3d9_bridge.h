#ifndef TF_D3D9_BRIDGE_H
#define TF_D3D9_BRIDGE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Call under the guest machine lock, before IAT binding. Guest addresses are
 * offsets from base (base==NULL for the 32-bit flat-memory runtime). Host COM
 * pointers and native window handles never appear in guest memory. */
void tf_d3d9_configure(void *base, uint64_t guest_limit,
    uint32_t (*allocate)(uint32_t), void (*release)(uint32_t),
    void *(*native_window_handle)(void));
void tf_d3d9_set_render_scale(uint32_t percent);
void tf_d3d9_set_frame_observer(void (*observer)(void));
/* Optional host lifecycle serialization. Both callbacks are installed before
 * guest execution. Suspend is called only with exclusive host ownership. */
void tf_d3d9_set_operation_guard(void (*enter)(void),void (*leave)(void));
void tf_d3d9_suspend_presenters(void);
int tf_d3d9_register(void);
/* Real guest32 registry/vtable -> native DXVK -> Android surface smoke. */
uint32_t tf_d3d9_smoke(uint32_t width,uint32_t height);
/* Synthetic save-image COM regression, no GPU/window/game entry. */
int tf_d3d9_image_selftest(void);
void tf_d3d9_shutdown(void);
/* Internal ABI shared with generated HLE wrappers. Fatal/unsupported calls
 * throw std::runtime_error; the HLE boundary converts these to hle_fatal. */
uint32_t tf_d3d9_create(uint32_t sdk);
uint32_t tf_d3d9_dispatch(uint32_t kind, uint32_t method,
    const uint32_t *arguments, uint32_t *argument_count);
uint32_t tf_d3dx9_dispatch(uint32_t operation, const uint32_t *arguments,
    uint32_t *argument_count);
uint32_t tf_d3d9_method_address(uint32_t kind,uint32_t method);
#ifdef __cplusplus
}
#endif
#endif
