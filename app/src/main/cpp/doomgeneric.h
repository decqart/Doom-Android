#ifndef DOOM_GENERIC
#define DOOM_GENERIC

#include <stdlib.h>
#include <stdint.h>

#ifdef __ANDROID__
extern int DOOMGENERIC_RESY;
extern int DOOMGENERIC_RESX;
#else
#define DOOMGENERIC_RESX 640
#define DOOMGENERIC_RESY 400
#endif

extern uint32_t *DG_ScreenBuffer;

void DG_Init(void);
void DG_DrawFrame(void);
void DG_SleepMs(uint32_t ms);
uint32_t DG_GetTicksMs(void);
int DG_GetKey(int *pressed, unsigned char *key);
void DG_SetWindowTitle(const char *title);

#endif /* DOOM_GENERIC */
