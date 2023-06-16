#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>

#include "doomkeys.h"
#include "doomgeneric.h"

//XWindowAttributes win_attr;
static Display *display = NULL;
static Window  window;
static int     screen = 0;
static GC      window_gc = 0;
static XImage  *s_Image = NULL;

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int   s_KeyQueueWriteIndex = 0;
static unsigned int   s_KeyQueueReadIndex = 0;

static unsigned char convertToDoomKey(unsigned int key)
{
    switch (key)
    {
    case XK_Return:
        key = KEY_ENTER;
        break;
    case XK_Escape:
        key = KEY_ESCAPE;
        break;
    case XK_A:
    case XK_a:
        key = KEY_LEFTARROW;
        break;
    case XK_D:
    case XK_d:
        key = KEY_RIGHTARROW;
        break;
    case XK_W:
    case XK_w:
        key = KEY_UPARROW;
        break;
    case XK_S:
    case XK_s:
        key = KEY_DOWNARROW;
        break;
    case XK_Control_L:
    case XK_Control_R:
        key = KEY_FIRE;
        break;
    case XK_E:
    case XK_e:
        key = KEY_USE;
        break;
    case XK_Shift_L:
    case XK_Shift_R:
        key = KEY_RSHIFT;
        break;
    default:
        break;
    }

    return key;
}

static void addKeyToQueue(int pressed, unsigned int keyCode)
{
    unsigned char key = convertToDoomKey(keyCode);

    unsigned short keyData = (pressed << 8) | key;

    s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
    s_KeyQueueWriteIndex++;
    s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

void DG_Init(void)
{
    memset(s_KeyQueue, 0, KEYQUEUE_SIZE * sizeof(unsigned short));

    // window creation
    display = XOpenDisplay(NULL);
    if (display == NULL)
    {
        puts("Failed to initialize display");
        exit(1);
    }

    screen = DefaultScreen(display);

    int blackColor = BlackPixel(display, screen);
    int whiteColor = WhitePixel(display, screen);

    XSetWindowAttributes attr;
    memset(&attr, 0, sizeof(XSetWindowAttributes));
    attr.event_mask = ExposureMask | KeyPressMask;
    attr.background_pixel = BlackPixel(display, screen);

    int depth = DefaultDepth(display, screen);

    window = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, DOOMGENERIC_RESX, DOOMGENERIC_RESY, 0, blackColor, blackColor);

    if (window == None)
    {
        puts("Failed to create window");
        XCloseDisplay(display);
        exit(1);
    }

    Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wm_delete_window, 1);

    XSelectInput(display, window, KeyPressMask | KeyReleaseMask);

    XMapWindow(display, window);

    window_gc = XCreateGC(display, window, 0, NULL);

    XSetForeground(display, window_gc, whiteColor);

    XkbSetDetectableAutoRepeat(display, 1, 0);

    XSizeHints *size_hint = XAllocSizeHints();

    size_hint->flags = PMinSize | PMaxSize;
    size_hint->min_width = DOOMGENERIC_RESX;
    size_hint->min_height = DOOMGENERIC_RESY;
    size_hint->max_width = DOOMGENERIC_RESX;
    size_hint->max_height = DOOMGENERIC_RESY;

    XSetWMSizeHints(display, window, size_hint, XA_WM_NORMAL_HINTS);
    XFree(size_hint);

    s_Image = XCreateImage(display, DefaultVisual(display, screen), depth, ZPixmap, 0, (char *) DG_ScreenBuffer, DOOMGENERIC_RESX, DOOMGENERIC_RESY, 32, 0);
}

void DG_DrawFrame(void)
{
    if (display == NULL) return;

    while (XPending(display))
    {
        XEvent ev;
        XNextEvent(display, &ev);
        if (ev.type == KeyPress)
        {
            KeySym sym = XkbKeycodeToKeysym(display, ev.xkey.keycode, 0, ev.xkey.state & ShiftMask ? 1 : 0);
            addKeyToQueue(1, sym);
        }
        else if (ev.type == KeyRelease)
        {
            KeySym sym = XkbKeycodeToKeysym(display, ev.xkey.keycode, 0, ev.xkey.state & ShiftMask ? 1 : 0);
            addKeyToQueue(0, sym);
        }
        else if (ev.type == ClientMessage)
        {
            XDestroyWindow(display, window);
            XCloseDisplay(display);
            exit(0);
        }
    }

    /*
    XGetWindowAttributes(display, window, &win_attr);
    if (DG_WindowWidth != win_attr.width || DG_WindowHeight != win_attr.height)
    {
        DG_WindowWidth = win_attr.width;
        DG_WindowHeight = win_attr.height;
        DG_ScreenBuffer = realloc(DG_ScreenBuffer, DG_WindowWidth*DG_WindowHeight*4);
        XFree(s_Image);
        int depth = DefaultDepth(display, screen);
        s_Image = XCreateImage(display, DefaultVisual(display, screen), depth, ZPixmap, 0, (char *) DG_ScreenBuffer, DG_WindowWidth, DG_WindowHeight, 32, 0);
    }
    */

    XPutImage(display, window, window_gc, s_Image, 0, 0, 0, 0, DOOMGENERIC_RESX, DOOMGENERIC_RESY);
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
    {
        //key queue is empty
        return 0;
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
    XStoreName(display, window, title);
}
