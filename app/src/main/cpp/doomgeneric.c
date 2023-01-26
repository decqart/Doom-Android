#include "doomgeneric.h"

uint32_t *DG_ScreenBuffer = 0;

void DG_Create(void)
{
#ifndef __ANDROID__
	DG_ScreenBuffer = malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
	DG_Init();
#else
	DG_Init();
	DG_ScreenBuffer = malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
#endif
}
