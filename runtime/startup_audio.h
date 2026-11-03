#ifndef TF_STARTUP_AUDIO_H
#define TF_STARTUP_AUDIO_H
#ifdef __cplusplus
extern "C" {
#endif
int tf_audio_register(void);
void tf_audio_service(void);
void tf_audio_shutdown(void);
void tf_audio_focus_gain(float gain);
void tf_audio_set_suspended(int suspended);
int tf_audio_selftest(int device_output);
#ifdef __cplusplus
}
#endif
#endif
