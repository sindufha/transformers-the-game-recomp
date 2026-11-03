#ifndef TF_CONTROL_PROFILE_H
#define TF_CONTROL_PROFILE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum {TF_CONTROL_HIDDEN=0,TF_CONTROL_MENU=1,TF_CONTROL_ROBOT=2,TF_CONTROL_CAR=3};
void tf_control_profile_init(void);
void tf_control_profile_sample(void); /* serialized guest frame only */
uint32_t tf_control_profile_current(void); /* atomic; Android UI may read */
int tf_control_profile_selftest(void);
void tf_control_memory_begin(void);
int tf_control_memory_read(void*,uint32_t,void*,uint32_t);
#ifdef __cplusplus
}
#endif
#endif
