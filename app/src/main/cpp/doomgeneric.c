#include <stdlib.h>

#include "doomgeneric.h"

#ifndef __ANDROID__
uint32_t DG_ScreenBuffer[DOOMGENERIC_RESX * DOOMGENERIC_RESY];
#else
int DOOMGENERIC_RESY;
int DOOMGENERIC_RESX;

uint32_t *DG_ScreenBuffer = NULL;
#endif

void DG_Create(void)
{
    DG_Init();
#ifdef __ANDROID__
    DG_ScreenBuffer = malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
#endif
}
