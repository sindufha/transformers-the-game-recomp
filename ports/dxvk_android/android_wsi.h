#pragma once
#include <stdint.h>

struct ANativeWindow;
#ifdef __cplusplus
extern "C" {
#endif
/* Retains window until replaced; caller must stop rendering before replacing it. */
void dxvkAndroidSetWindow(struct ANativeWindow* window);
/* Opaque native HWND, distinct from the guest's 32-bit HWND. */
void* dxvkAndroidGetWindowHandle(void);
void dxvkAndroidSetRenderSize(uint32_t width, uint32_t height);
#ifdef __cplusplus
}
#endif
