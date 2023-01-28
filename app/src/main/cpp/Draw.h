#ifndef _DRAW_H
#define _DRAW_H

#include <stdint.h>

extern int lastButtonX;
extern int lastButtonY;
extern int buttonDown;
extern int lastMotionX;
extern int lastMotionY;
extern int lastButtonId;
extern int lastMask;

void GetScreenDimensions(int *x, int *y);

void SetupApplication(void);
void HandleInput(void);

void HandleButton(int x, int y, int button, int bDown);
void HandleMotion(int x, int y, int mask);

void DrawCircle(int x, int y, float radius, uint32_t color);
void DrawImage(uint32_t *data, int x, int y, int w, int h);

void ClearFrame(void);
void SwapBuffers(void);

void InternalResize(int x, int y);
void SetupBatchInternal(void);

#ifdef __ANDROID__
#include "AndroidDriver.h"
#endif

#endif /* _DRAW_H */
