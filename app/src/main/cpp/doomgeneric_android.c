#include <time.h>

#include "AndroidRenderer.h"

#include "doomkeys.h"
#include "doomgeneric.h"
#include "doomstat.h"

#define KEYQUEUE_SIZE 16

static int screen_x, screen_y;

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

static void addKeyToQueue(int pressed, unsigned char key)
{
    unsigned short keyData = (pressed << 8) | key;

    s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
    s_KeyQueueWriteIndex++;
    s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

static void VirtualButton(int x, int y, int id, unsigned char keycode)
{
    static bool pressed[3] = { false, false, false };
    int lw = x + 200;
    int lh = y + 200;

    if (pressed[id])
        RenderCircle(x, y, 100, 0x4c4c4cff);
    else
        RenderCircle(x, y, 100, 0x808080ff);

    int idx;
    if (pointer_touched_in(x, y, lw, lh, &idx) && !pressed[id])
    {
        addKeyToQueue(1, keycode);
        pressed[id] = true;
    }
    else if (!pointer_touched_in(x, y, lw, lh, &idx) && pressed[id])
    {
        addKeyToQueue(0, keycode);
        pressed[id] = false;
    }
}

static void VirtualJoystick(void)
{
    // make center of joystick do nothing
    static bool forward = false;
    static bool backward = false;
    static bool left = false;
    static bool right = false;
    RenderCircle(screen_x / 18, screen_y - 300, 100, 0x4c4c4cff);
    int id;
    if (pointer_touched_in(screen_x/18, screen_y-300, screen_x/18+200, screen_y-100, &id))
    {
        RenderCircle(motion_x[id] - 80, motion_y[id] - 80, 80, 0x808080ff);
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
        RenderCircle(screen_x / 16, screen_y - 280, 80, 0x808080ff);

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
}

void DG_Init(void)
{
    AndroidMakeFullscreen();
    SetupApplication();
    GetScreenDimensions(&screen_x, &screen_y);
    DOOMGENERIC_RESY = screen_y;
    DOOMGENERIC_RESX = DOOMGENERIC_RESY/10*16;
}

void DG_DrawFrame(void)
{
    ClearFrame();
    RenderImage(DG_ScreenBuffer, screen_x / 2 - DOOMGENERIC_RESX / 2,
                screen_y / 2 - DOOMGENERIC_RESY / 2, DOOMGENERIC_RESX, DOOMGENERIC_RESY);
    VirtualJoystick();

    if (menuactive)
        VirtualButton(screen_x-400, screen_y-300, 0, KEY_ENTER);
    else
        VirtualButton(screen_x-400, screen_y-300, 0, KEY_FIRE);
    VirtualButton(screen_x-250, screen_y-500, 1, KEY_USE);
    VirtualButton(screen_x-250, screen_y-850, 2, KEY_ESCAPE);
    HandleInput();
    SwapBuffers();
}

void DG_SleepMs(uint32_t ms)
{
    struct timespec req = {
            .tv_sec = 0,
            .tv_nsec = (long) ms*1000000
    };
    nanosleep(&req, NULL);
}

uint32_t DG_GetTicksMs(void)
{
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);

    return (tp.tv_sec * 1000) + (tp.tv_nsec / 1000000); /* return milliseconds */
}

int DG_GetKey(int *pressed, unsigned char *doomKey)
{
    if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex)
        return 0; //key queue is empty

    unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
    s_KeyQueueReadIndex++;
    s_KeyQueueReadIndex %= KEYQUEUE_SIZE;

    *pressed = keyData >> 8;
    *doomKey = keyData & 0xFF;

    return 1;
}

void DG_SetWindowTitle(const char *title)
{
    (void) title;
}
