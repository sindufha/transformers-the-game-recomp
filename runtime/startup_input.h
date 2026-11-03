#ifndef TRANSFORMERS_STARTUP_INPUT_H
#define TRANSFORMERS_STARTUP_INPUT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int tf_input_register(void);
/* Host callbacks are thread-safe. VKs use the Win32 virtual-key namespace.
 * Send the same keyboard event to tf_user32_key_event for window messages. */
int tf_input_key_event(uint32_t virtual_key,int down);
/* Relative deltas; wheel uses Win32 units (120 per notch). buttons is the
 * current absolute mask: left=1, right=2, middle=4, then buttons 4..8. */
int tf_input_mouse_event(int32_t dx,int32_t dy,int32_t wheel,uint32_t buttons);
void tf_input_release_all(void);
int tf_input_selftest(void);
#ifdef __cplusplus
}
#endif
#endif
