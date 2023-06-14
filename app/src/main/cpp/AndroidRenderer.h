#ifndef ANDROID_RENDERER_H
#define ANDROID_RENDERER_H

#include <stdint.h>
#include <stdbool.h>

extern int button_x[8];
extern int button_y[8];
extern int motion_x[8];
extern int motion_y[8];
extern bool button_down[8];

void GetScreenDimensions(int *x, int *y);

void SetupApplication(void);
void HandleInput(void);

void RenderCircle(int x, int y, float radius, uint32_t color);
void RenderImage(uint32_t *data, int x, int y, int w, int h);

void ClearFrame(void);
void SwapBuffers(void);

void InternalResize(int x, int y);
void SetupBatchInternal(void);

#ifdef __ANDROID__
#include "AndroidDriver.h"
#endif

#endif /* ANDROID_RENDERER_H */
