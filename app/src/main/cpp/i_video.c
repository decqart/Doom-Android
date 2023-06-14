/*-----------------------------------------------------------------------------
 *
 * $Id:$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  DESCRIPTION:
 *   DOOM graphics stuff for X11, UNIX.
 *
 *-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>

#include "i_video.h"
#include "z_zone.h"

#include "tables.h"
#include "doomkeys.h"
#include "doomgeneric.h"

struct FB_BitField {
    uint32_t offset; /* beginning of bitfield */
    uint32_t length; /* length of bitfield */
};

struct FB_ScreenInfo {
    uint32_t xres; /* visible resolution */
    uint32_t yres;

    uint32_t bits_per_pixel; /* guess what */

                            /* >1 = FOURCC */
    struct FB_BitField red; /* bitfield in s_Fb mem if true color, */
    struct FB_BitField green; /* else only length is significant */
    struct FB_BitField blue;
    struct FB_BitField transp; /* transparency */
};

static struct FB_ScreenInfo s_Fb;
int fb_scaling = 1;
int usemouse = 0;

struct color {
    uint32_t b:8;
    uint32_t g:8;
    uint32_t r:8;
    uint32_t a:8;
};

static struct color colors[256];

void I_GetEvent(void);

// The screen buffer; this is modified to draw things to the screen
byte *I_VideoBuffer = NULL;

// If true, game is running as a screensaver
boolean screensaver_mode = False;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen

boolean screenvisible;

// Mouse acceleration
//
// This emulates some of the behavior of DOS mouse drivers by increasing
// the speed when the mouse is moved fast.
//
// The mouse input values are input directly to the game, but when
// the values exceed the value of mouse_threshold, they are multiplied
// by mouse_acceleration to increase the speed.

float mouse_acceleration = 2.0f;
int mouse_threshold = 10;

// Gamma correction level to use

int usegamma = 0;

typedef struct {
    byte r;
    byte g;
    byte b;
} col_t;

// Palette converted to RGB565

static uint16_t rgb565_palette[256];

void cmap_to_rgb565(uint16_t *out, uint8_t *in, int in_pixels)
{
    struct color c;
    uint16_t r, g, b;

    for (int i = 0; i < in_pixels; i++)
    {
        c = colors[*in]; 
        r = ((uint16_t)(c.r >> 3)) << 11;
        g = ((uint16_t)(c.g >> 2)) << 5;
        b = ((uint16_t)(c.b >> 3)) << 0;
        *out = (r | g | b);

        in++;
        for (int j = 0; j < fb_scaling; j++) {
            out++;
        }
    }
}

void cmap_to_fb(uint8_t *out, uint8_t *in, int in_pixels)
{
    struct color c;
    uint32_t pix;
    uint16_t r, g, b;

    for (int i = 0; i < in_pixels; i++)
    {
        c = colors[*in]; /* R:8 G:8 B:8 format! */
        r = (uint16_t)(c.r >> (8 - s_Fb.red.length));
        g = (uint16_t)(c.g >> (8 - s_Fb.green.length));
        b = (uint16_t)(c.b >> (8 - s_Fb.blue.length));
        pix = r << s_Fb.red.offset;
        pix |= g << s_Fb.green.offset;
        pix |= b << s_Fb.blue.offset;

        for (int k = 0; k < fb_scaling; k++) {
            for (int j = 0; j < s_Fb.bits_per_pixel/8; j++) {
                *out = (pix >> (j*8));
                out++;
            }
        }
        in++;
    }
}

void I_InitGraphics(void)
{
    memset(&s_Fb, 0, sizeof(struct FB_ScreenInfo));
    s_Fb.xres = DOOMGENERIC_RESX;
    s_Fb.yres = DOOMGENERIC_RESY;
    s_Fb.bits_per_pixel = 32;

    s_Fb.blue.length = 8;
    s_Fb.green.length = 8;
    s_Fb.red.length = 8;
    s_Fb.transp.length = 8;

    s_Fb.blue.offset = 0;
    s_Fb.green.offset = 8;
    s_Fb.red.offset = 16;
    s_Fb.transp.offset = 24;

    printf("I_InitGraphics: framebuffer: x_res: %d, y_res: %d, bpp: %d\n",
           s_Fb.xres, s_Fb.yres,  s_Fb.bits_per_pixel);

    printf("I_InitGraphics: framebuffer: RGBA: %d%d%d%d, red_off: %d, green_off: %d, blue_off: %d, transp_off: %d\n",
           s_Fb.red.length, s_Fb.green.length, s_Fb.blue.length, s_Fb.transp.length, s_Fb.red.offset, s_Fb.green.offset, s_Fb.blue.offset, s_Fb.transp.offset);

    printf("I_InitGraphics: DOOM screen size: w x h: %d x %d\n", SCREENWIDTH, SCREENHEIGHT);

    fb_scaling = s_Fb.xres / SCREENWIDTH;
    if (s_Fb.yres / SCREENHEIGHT < fb_scaling)
        fb_scaling = s_Fb.yres / SCREENHEIGHT;
    printf("I_InitGraphics: Auto-scaling factor: %d\n", fb_scaling);

    /* Allocate screen to draw to */
    I_VideoBuffer = (byte *) Z_Malloc(SCREENWIDTH * SCREENHEIGHT, PU_STATIC, NULL);  // For DOOM to draw on

    screenvisible = True;
}

void I_StartFrame(void) {}

void I_StartTic(void)
{
    I_GetEvent();
}

void I_FinishUpdate(void)
{
    int x_offset, y_offset, x_offset_end;
    unsigned char *line_in, *line_out;

    /* Offsets in case FB is bigger than DOOM */
    /* 600 = s_Fb height, 200 screenheight */
    /* 600 = s_Fb height, 200 screenheight */
    /* 2048 =s_Fb width, 320 screenwidth */
    y_offset     = (((s_Fb.yres - (SCREENHEIGHT * fb_scaling)) * s_Fb.bits_per_pixel/8)) / 2;
    x_offset     = (((s_Fb.xres - (SCREENWIDTH  * fb_scaling)) * s_Fb.bits_per_pixel/8)) / 2; // XXX: siglent FB hack: /4 instead of /2, since it seems to handle the resolution in a funny way
    x_offset_end = ((s_Fb.xres - (SCREENWIDTH  * fb_scaling)) * s_Fb.bits_per_pixel/8) - x_offset;

    /* DRAW SCREEN */
    line_in  = (unsigned char *) I_VideoBuffer;
    line_out = (unsigned char *) DG_ScreenBuffer;

    int y = SCREENHEIGHT;

    while (y--)
    {
        for (int i = 0; i < fb_scaling; i++)
        {
            line_out += x_offset;
#ifdef CMAP256
            for (fb_scaling == 1) {
                memcpy(line_out, line_in, SCREENWIDTH); /* fb_width is bigger than Doom SCREENWIDTH... */
            } else {
                //XXX FIXME fb_scaling support!
            }
#else
            //cmap_to_rgb565((void*)line_out, (void*)line_in, SCREENWIDTH);
            cmap_to_fb((void*)line_out, (void*)line_in, SCREENWIDTH);
#endif
            line_out += (SCREENWIDTH * fb_scaling * (s_Fb.bits_per_pixel/8)) + x_offset_end;
        }
        line_in += SCREENWIDTH;
    }

    DG_DrawFrame();
    //s_Fb.xres = DG_WindowWidth;
    //s_Fb.yres = DG_WindowWidth;
}

void I_ReadScreen(byte *scr)
{
    memcpy(scr, I_VideoBuffer, SCREENWIDTH * SCREENHEIGHT);
}

#define GFX_RGB565_R(color) ((0xF800 & color) >> 11)
#define GFX_RGB565_G(color) ((0x07E0 & color) >> 5)
#define GFX_RGB565_B(color) (0x001F & color)

void I_SetPalette(byte *palette)
{
    /* performance boost:
     * map to the right pixel format over here! */

    for (int i = 0; i < 256; ++i)
    {
        colors[i].a = 0;
        colors[i].r = gammatable[usegamma][*palette++];
        colors[i].g = gammatable[usegamma][*palette++];
        colors[i].b = gammatable[usegamma][*palette++];
    }
}

// Given an RGB value, find the closest matching palette index.
int I_GetPaletteIndex(int r, int g, int b)
{
    int best, best_diff, diff;
    col_t color;

    printf("I_GetPaletteIndex\n");

    best = 0;
    best_diff = INT_MAX;

    for (int i = 0; i < 256; ++i)
    {
        color.r = GFX_RGB565_R(rgb565_palette[i]);
        color.g = GFX_RGB565_G(rgb565_palette[i]);
        color.b = GFX_RGB565_B(rgb565_palette[i]);

        diff = (r - color.r) * (r - color.r)
             + (g - color.g) * (g - color.g)
             + (b - color.b) * (b - color.b);

        if (diff < best_diff)
        {
            best = i;
            best_diff = diff;
        }

        if (diff == 0)
            break;
    }

    return best;
}

void I_SetWindowTitle(const char *title)
{
    DG_SetWindowTitle(title);
}

void I_SetGrabMouseCallback(grabmouse_callback_t func) {}

void I_DisplayFPSDots(boolean dots_on) {}
