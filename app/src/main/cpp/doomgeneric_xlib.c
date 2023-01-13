#include "doomkeys.h"

#include "doomgeneric.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>

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
    case XK_a:
		key = KEY_LEFTARROW;
		break;
    case XK_d:
		key = KEY_RIGHTARROW;
		break;
    case XK_w:
		key = KEY_UPARROW;
		break;
    case XK_s:
		key = KEY_DOWNARROW;
		break;
    case XK_Control_L:
    case XK_Control_R:
		key = KEY_FIRE;
		break;
    case XK_e:
		key = KEY_USE;
		break;
    case XK_Shift_L:
    case XK_Shift_R:
		key = KEY_RSHIFT;
		break;
	default:
		key = tolower(key);
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
        printf("Failed to create window");
        XCloseDisplay(display);
        exit(-1);
    }

    XSelectInput(display, window, KeyPressMask | KeyReleaseMask);

    XMapWindow(display, window);

    window_gc = XCreateGC(display, window, 0, NULL);

    XSetForeground(display, window_gc, whiteColor);

    XkbSetDetectableAutoRepeat(display, 1, 0);

    s_Image = XCreateImage(display, DefaultVisual(display, screen), depth, ZPixmap, 0, (char *) DG_ScreenBuffer, DOOMGENERIC_RESX, DOOMGENERIC_RESY, 32, 0);
}

void DG_DrawFrame(void)
{
    for (int i = 0; i < 16; ++i) {
        printf("queeue = %d\n", s_KeyQueue[i]);
    }
    if (display)
    {
        while (XPending(display) > 0)
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
        }
        XSizeHints *size_hint = XAllocSizeHints();

        size_hint->flags = PMinSize | PMaxSize;
        size_hint->min_width = DOOMGENERIC_RESX;
        size_hint->min_height = DOOMGENERIC_RESY;
        size_hint->max_width = DOOMGENERIC_RESX;
        size_hint->max_height = DOOMGENERIC_RESY;
    
        XSetWMSizeHints(display, window, size_hint, XA_WM_NORMAL_HINTS);
        XFree(size_hint);

        XPutImage(display, window, window_gc, s_Image, 0, 0, 0, 0, DOOMGENERIC_RESX, DOOMGENERIC_RESY);
    }
}

void DG_SleepMs(uint32_t ms)
{
    usleep(ms * 1000);
}

uint32_t DG_GetTicksMs(void)
{
    struct timeval  tp;
    struct timezone tzp;

    gettimeofday(&tp, &tzp);

    return (tp.tv_sec * 1000) + (tp.tv_usec / 1000); /* return milliseconds */
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
