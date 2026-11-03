#ifndef TRANSFORMERS_STARTUP_USER32_H
#define TRANSFORMERS_STARTUP_USER32_H
#include <stdint.h>
/* A virtual Win32 window and event model. Rendering remains the host's job.
 * Call all hooks under the serialized guest machine lock. The pump may inject
 * events on that same thread; Android callbacks must queue cross-thread work. */
int tf_user32_register(void);
void tf_user32_set_display(int width,int height);
void tf_user32_set_pump(void (*pump)(void));
int tf_user32_post(uint32_t hwnd,uint32_t message,uint32_t wparam,uint32_t lparam);
uint32_t tf_user32_first_window(void);
/* Android UI threads may enqueue keys without taking the guest lock. They
 * are delivered when the guest pumps events; return 0 if the queue is full. */
int tf_user32_key_event(uint32_t virtual_key,int down);
int tf_user32_key_down(uint32_t virtual_key); /* guest machine lock required */
int tf_user32_selftest(void);
#endif
