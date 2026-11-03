#ifndef TRANSFORMERS_STARTUP_H
#define TRANSFORMERS_STARTUP_H
#include "pe_loader.h"
int tf_startup_register(const pe_image *image, const char *host_path);
int tf_startup_selftest(void);
#endif
