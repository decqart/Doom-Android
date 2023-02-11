#include "Draw.h"

#include "doomkeys.h"
#include "doomgeneric.h"
#include "doomstat.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#define KEYQUEUE_SIZE 16

int DOOMGENERIC_RESY;
int DOOMGENERIC_RESX;

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

static bool pointer_touched_in(int x, int y, int x2, int y2, int *id)
{
    bool touched = false;
    for (int i = 0; i < 8; ++i)
    {
        touched = (x < button_x[i] && button_x[i] < x2) && (y < button_y[i] && button_y[i] < y2);
        *id = i;
        if (touched) break;
    }
    return touched;
}

static bool pointer_motion_in(int x, int y, int x2, int y2)
{
    bool motion = false;
    for (int i = 0; i < 8; ++i)
    {
        motion = (x < motion_x[i] && motion_x[i] < x2) && (y < motion_y[i] && motion_y[i] < y2);
        if (motion) break;
    }
    return motion;
}

static void addKeyToQueue(int pressed, unsigned char key)
{
    unsigned short keyData = (pressed << 8) | key;

    s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
    s_KeyQueueWriteIndex++;
    s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

int screen_x, screen_y;

void DG_Init(void)
{
    AndroidMakeFullscreen();
    SetupApplication();
    GetScreenDimensions(&screen_x, &screen_y);
    DOOMGENERIC_RESY = screen_y;
    DOOMGENERIC_RESX = DOOMGENERIC_RESY/10*16;
}

void VirtualButton(int x, int y, int id, unsigned char keycode)
{
    static bool pressable[2] = { false, false };
    static bool pressed[2] = { false, false };
    int lw = x + 200;
    int lh = y + 200;
    DrawCircle(x, y, 100, 0x808080ff);

    int idx;
    if (pointer_motion_in(x, y, lw, lh) &&
        pointer_touched_in(x, y, lw, lh, &idx))
    {
        DrawCircle(x, y, 100, 0x4c4c4cff);
        pressable[id] = true;
    }

    if (!pointer_motion_in(x, y, lw, lh))
        pressable[id] = false;

    if (pressed[id]) // after 1 cycle it returns pressed 0
    {
        pressed[id] = false;
        addKeyToQueue(0, keycode);
    }

    if (pressable[id] && !button_down[idx])
    {
        pressable[id] = false;
        pressed[id] = true;
        addKeyToQueue(1, keycode);
    }
}

// event queue
void VirtualJoystick(void)
{
    // make center of joystick do nothing
    static bool forward = false;
    static bool backward = false;
    static bool left = false;
    static bool right = false;
    DrawCircle(screen_x/18, screen_y-300, 100, 0x4c4c4cff);
    int id;
    if (pointer_touched_in(screen_x/18, screen_y-300, screen_x/18+200, screen_y-100, &id))
    {
        DrawCircle(motion_x[id]-80, motion_y[id]-80, 80, 0x808080ff);
        if (motion_y[id] < screen_y-250) {
            if (!forward) {
                addKeyToQueue(1, KEY_UPARROW);
                forward = true;
            }
            if (backward) {
                addKeyToQueue(0, KEY_DOWNARROW);
                backward = false;
            }
        } else if (screen_y-150 < motion_y[id]) {
            if (!backward) {
                addKeyToQueue(1, KEY_DOWNARROW);
                backward = true;
            }
            if (forward) {
                addKeyToQueue(0, KEY_UPARROW);
                forward = false;
            }
        }

        if (motion_x[id] > screen_x/18+150) {
            if (!right) {
                addKeyToQueue(1, KEY_RIGHTARROW);
                right = true;
            }
            if (left) {
                addKeyToQueue(0, KEY_LEFTARROW);
                left = false;
            }
        } else if (screen_x/18+50 > motion_x[id]) {
            if (!left) {
                addKeyToQueue(1, KEY_LEFTARROW);
                left = true;
            }
            if (right) {
                addKeyToQueue(0, KEY_RIGHTARROW);
                right = false;
            }
        } else {
            goto middle;
        }
    }
    else
    {
        DrawCircle(screen_x/16, screen_y-280, 80, 0x808080ff);

        if (forward) {
            addKeyToQueue(0, KEY_UPARROW);
            forward = false;
        }
        if (backward) {
            addKeyToQueue(0, KEY_DOWNARROW);
            backward = false;
        }

        middle:
        if (right) {
            addKeyToQueue(0, KEY_RIGHTARROW);
            right = false;
        }
        if (left) {
            addKeyToQueue(0, KEY_LEFTARROW);
            left = false;
        }
    }
    if (menuactive)
        VirtualButton(screen_x-400, screen_y-300, 0, KEY_ENTER);
    else
        VirtualButton(screen_x-400, screen_y-300, 0, KEY_FIRE);
    VirtualButton(screen_x-250, screen_y-500, 1, KEY_USE);
}

void DG_DrawFrame(void)
{
    ClearFrame();
    DrawImage(DG_ScreenBuffer, screen_x/2-DOOMGENERIC_RESX/2, screen_y/2-DOOMGENERIC_RESY/2, DOOMGENERIC_RESX, DOOMGENERIC_RESY);
    VirtualJoystick();
    HandleInput();
    SwapBuffers();
}

void DG_SleepMs(uint32_t ms)
{
    usleep(ms * 1000);
}

uint32_t DG_GetTicksMs(void)
{
    struct timeval tp;
    struct timezone tzp;

    gettimeofday(&tp, &tzp);

    return (tp.tv_sec * 1000) + (tp.tv_usec / 1000); /* return milliseconds */
}

int DG_GetKey(int *pressed, unsigned char *doomKey)
{
    if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex)
    {
        return 0; //key queue is empty
    }
    else
    {
        unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
        s_KeyQueueReadIndex++;
        s_KeyQueueReadIndex %= KEYQUEUE_SIZE;

        *pressed = keyData >> 8;
        *doomKey = keyData & 0xFF;

        return 1;
    }
}

void DG_SetWindowTitle(const char *title)
{
    (void) title;
}
