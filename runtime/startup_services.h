#ifndef TRANSFORMERS_STARTUP_SERVICES_H
#define TRANSFORMERS_STARTUP_SERVICES_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* game_dir must be the host's existing private app game directory. All shell
 * directory creation is restricted to its documents/ subtree. NULL registers
 * the non-filesystem APIs without configuring a documents location. */
int tf_services_register(const char *game_dir); /* -1: private folder setup failed */
int tf_services_random(void *output,uint32_t count); /* getrandom only */
int tf_services_selftest(void);
#ifdef __cplusplus
}
#endif
#endif
