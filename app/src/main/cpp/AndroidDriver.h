#ifndef ANDROID_DRIVER_H
#define ANDROID_DRIVER_H

#include <stdio.h>

#ifdef __ANDROID__
#include <android/log.h>
#define printf(...) __android_log_print(ANDROID_LOG_VERBOSE, "Doom", __VA_ARGS__)
#endif

void AndroidMakeFullscreen(void);
FILE *android_fopen(const char *fname, const char *mode);

#endif /* ANDROID_DRIVER_H */
