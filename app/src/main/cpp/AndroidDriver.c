#include "AndroidDriver.h"

static struct android_app *gapp;
static int OGLESStarted = 0;
static int android_width, android_height;
static int is_app_paused = 0;

#include <jni.h>
#include <android/asset_manager.h>
#include <android_native_app_glue.h>

#include "Draw.h"

#include <stdio.h>
#include <stdlib.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

EGLNativeWindowType native_window;

static const EGLint config_attr_list[] = {
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_ALPHA_SIZE, 8,
	EGL_BUFFER_SIZE, 32,
	EGL_STENCIL_SIZE, 0,
	EGL_DEPTH_SIZE, 16,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_NONE
};

static EGLint window_attr_list[] = {
	EGL_NONE
};

static const EGLint context_attr_list[] = {
	EGL_CONTEXT_CLIENT_VERSION, 2,
	EGL_NONE
};

EGLDisplay egl_display;
EGLSurface egl_surface;
EGLContext egl_context;

static int LastInternalW, LastInternalH;

void SwapBuffers(void)
{
	if (is_app_paused) return;
	eglSwapBuffers(egl_display, egl_surface);
	android_width = ANativeWindow_getWidth(native_window);
	android_height = ANativeWindow_getHeight(native_window);
	glViewport(0, 0, android_width, android_height);
	if (LastInternalW != android_width || LastInternalH != android_height)
	{
		LastInternalW = android_width;
		LastInternalH = android_height;
		InternalResize(LastInternalW, LastInternalH);
	}
}

void GetScreenDimensions(int *x, int *y)
{
	*x = android_width;
	*y = android_height;
}

void AndroidMakeFullscreen(void)
{
    //Partially based on https://stackoverflow.com/questions/47507714/how-do-i-enable-full-screen-immersive-mode-for-a-native-activity-ndk-app
    const struct JNINativeInterface *env = 0;
    const struct JNINativeInterface **envptr = &env;
    const struct JNIInvokeInterface **jniiptr = gapp->activity->vm;
    const struct JNIInvokeInterface *jnii = *jniiptr;

    jnii->AttachCurrentThread(jniiptr, &envptr, NULL);
    env = (*envptr);

    //Get android.app.NativeActivity, then get getWindow method handle, returns view.Window type
    jclass activityClass = env->FindClass(envptr, "android/app/NativeActivity");
    jmethodID getWindow = env->GetMethodID(envptr, activityClass, "getWindow", "()Landroid/view/Window;");
    jobject window = env->CallObjectMethod(envptr, gapp->activity->clazz, getWindow);

    //Get android.view.Window class, then get getDecorView method handle, returns view.View type
    jclass windowClass = env->FindClass(envptr, "android/view/Window");
    jmethodID getDecorView = env->GetMethodID(envptr, windowClass, "getDecorView", "()Landroid/view/View;");
    jobject decorView = env->CallObjectMethod(envptr, window, getDecorView);

    //Get the flag values associated with systemUIVisibility
    jclass viewClass = env->FindClass(envptr, "android/view/View");
    const int flagLayoutHideNavigation = env->GetStaticIntField(envptr, viewClass, env->GetStaticFieldID(envptr, viewClass, "SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION", "I"));
    const int flagLayoutFullscreen = env->GetStaticIntField(envptr, viewClass, env->GetStaticFieldID(envptr, viewClass, "SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN", "I"));
    const int flagLowProfile = env->GetStaticIntField(envptr, viewClass, env->GetStaticFieldID(envptr, viewClass, "SYSTEM_UI_FLAG_LOW_PROFILE", "I"));
    const int flagHideNavigation = env->GetStaticIntField(envptr, viewClass, env->GetStaticFieldID(envptr, viewClass, "SYSTEM_UI_FLAG_HIDE_NAVIGATION", "I"));
    const int flagFullscreen = env->GetStaticIntField(envptr, viewClass, env->GetStaticFieldID(envptr, viewClass, "SYSTEM_UI_FLAG_FULLSCREEN", "I"));
    const int flagImmersiveSticky = env->GetStaticIntField(envptr, viewClass, env->GetStaticFieldID(envptr, viewClass, "SYSTEM_UI_FLAG_IMMERSIVE_STICKY", "I"));

    jmethodID setSystemUiVisibility = env->GetMethodID(envptr, viewClass, "setSystemUiVisibility", "(I)V");

    //Call the decorView.setSystemUiVisibility(FLAGS)
    env->CallVoidMethod(envptr, decorView, setSystemUiVisibility,
                        (flagLayoutHideNavigation | flagLayoutFullscreen | flagLowProfile | flagHideNavigation | flagFullscreen | flagImmersiveSticky));

    //now set some more flags associated with layoutManager -- note the $ in the class path
    //search for api-versions.xml
    //https://android.googlesource.com/platform/development/+/refs/tags/android-9.0.0_r48/sdk/api-versions.xml

    jclass layoutManagerClass = env->FindClass(envptr, "android/view/WindowManager$LayoutParams");
    const int flag_WinMan_Fullscreen = env->GetStaticIntField(envptr, layoutManagerClass, (env->GetStaticFieldID(envptr, layoutManagerClass, "FLAG_FULLSCREEN", "I")));
    const int flag_WinMan_KeepScreenOn = env->GetStaticIntField(envptr, layoutManagerClass, (env->GetStaticFieldID(envptr, layoutManagerClass, "FLAG_KEEP_SCREEN_ON", "I")));
    const int flag_WinMan_hw_acc = env->GetStaticIntField(envptr, layoutManagerClass, (env->GetStaticFieldID(envptr, layoutManagerClass, "FLAG_HARDWARE_ACCELERATED", "I")));
    //call window.addFlags(FLAGS)
    env->CallVoidMethod(envptr, window, (env->GetMethodID(envptr, windowClass, "addFlags" , "(I)V")), (flag_WinMan_Fullscreen | flag_WinMan_KeepScreenOn | flag_WinMan_hw_acc));

    jnii->DetachCurrentThread(jniiptr);
}

void SetupApplication(void)
{
	is_app_paused = 0;
    EGLint egl_major, egl_minor;
    EGLConfig config;
    EGLint num_config;

    //This MUST be called before doing any initialization.
    int events;
    while (!OGLESStarted)
    {
        struct android_poll_source *source;
        if (ALooper_pollAll(0, 0, &events, (void**)&source) >= 0)
        {
            if (source != NULL) source->process(gapp, source);
        }
    }

    egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    if (egl_display == EGL_NO_DISPLAY)
    {
        printf("Error: No display found!\n");
		exit(-1);
    }

    if (!eglInitialize(egl_display, &egl_major, &egl_minor))
    {
        printf("Error: eglInitialise failed!\n");
		exit(-1);
    }

    eglChooseConfig(egl_display, config_attr_list, &config, 1,
					&num_config);
	egl_context = eglCreateContext(egl_display, config, EGL_NO_CONTEXT,
								   context_attr_list);

    if (egl_context == EGL_NO_CONTEXT)
    {
        printf("Error: eglCreateContext failed: 0x%08X\n", eglGetError());
        exit(-1);
    }

    if (native_window && !gapp->window)
    {
        printf("WARNING: App restarted without a window. Cannot progress.\n");
        exit(0);
    }

    native_window = gapp->window;
    if (!native_window)
    {
        printf("FAULT: Cannot get window\n");
		exit(-1);
    }
    android_width = ANativeWindow_getWidth(native_window);
    android_height = ANativeWindow_getHeight(native_window);

    egl_surface = eglCreateWindowSurface(egl_display, config, gapp->window, window_attr_list);

    if (egl_surface == EGL_NO_SURFACE)
    {
        printf("Error: eglCreateWindowSurface failed: 0x%08X\n", eglGetError());
		exit(-1);
    }

    if (!eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context))
    {
        printf("Error: eglMakeCurrent() failed: 0x%08X\n", eglGetError());
		exit(-1);
    }

    SetupBatchInternal();
}

int32_t handle_input(struct android_app *app, AInputEvent *event)
{
	if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION)
	{
		int action = AMotionEvent_getAction(event);
		int whichSource = action >> 8;
		action &= AMOTION_EVENT_ACTION_MASK;
		size_t pointerCount = AMotionEvent_getPointerCount(event);

		for (size_t i = 0; i < pointerCount; ++i)
		{
			int x, y, index;
			x = (int) AMotionEvent_getX(event, i);
			y = (int) AMotionEvent_getY(event, i);
			index = AMotionEvent_getPointerId(event, i);

			if (action == AMOTION_EVENT_ACTION_POINTER_DOWN || action == AMOTION_EVENT_ACTION_DOWN)
			{
				int id = index;
				if (action == AMOTION_EVENT_ACTION_POINTER_DOWN && id != whichSource) continue;
				HandleButton(x, y, id, 1);
				ANativeActivity_showSoftInput(gapp->activity, ANATIVEACTIVITY_SHOW_SOFT_INPUT_FORCED);
			}
			else if (action == AMOTION_EVENT_ACTION_POINTER_UP || action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_CANCEL)
			{
				int id = index;
				if (action == AMOTION_EVENT_ACTION_POINTER_UP && id != whichSource) continue;
				HandleButton(x, y, id, 0);
			}
			else if (action == AMOTION_EVENT_ACTION_MOVE)
			{
				HandleMotion(x, y, index);
			}
		}
		return 1;
	}
    //KEY INPUT IS SIMPLY BROKEN
	return 0;
}

void HandleInput(void)
{
	int events;
	struct android_poll_source *source;
	while (ALooper_pollAll(0, 0, &events, (void**)&source) >= 0)
	{
		if (source != NULL) source->process(gapp, source);
	}
}

void handle_cmd(struct android_app *app, int32_t cmd)
{
	switch (cmd)
	{
	    case APP_CMD_INIT_WINDOW:
		    //When returning from a back button suspension, this isn't called.
		    if (!OGLESStarted)
		    {
			    OGLESStarted = 1;
			    printf("Got start event\n");
		    }
		    else
		    {
			    SetupApplication();
		    }
            break;
		case APP_CMD_TERM_WINDOW:
			if (egl_display != EGL_NO_DISPLAY) {
				eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
				if (egl_context != EGL_NO_CONTEXT)
					eglDestroyContext(egl_display, egl_context);
				if (egl_surface != EGL_NO_SURFACE)
					eglDestroySurface(egl_display, egl_surface);
				eglTerminate(egl_display);

				egl_display = EGL_NO_DISPLAY;
				egl_context = EGL_NO_CONTEXT;
				egl_surface = EGL_NO_SURFACE;
				is_app_paused = 1;
			}
			break;
        case APP_CMD_DESTROY:
            //This gets called initially after back.
            ANativeActivity_finish(gapp->activity);
            break;
	    default:
			break;
	}
}

static int android_read(void *cookie, char *buf, int size)
{
	return AAsset_read((AAsset *) cookie, buf, size);
}

static int android_write(void *cookie, const char *buf, int size)
{
	return 0; // can't provide write access to the apk
}

static fpos_t android_seek(void *cookie, fpos_t offset, int whence)
{
	return AAsset_seek((AAsset *) cookie, offset, whence);
}

static int android_close(void *cookie)
{
	AAsset_close((AAsset *) cookie);
	return 0;
}

FILE *android_fopen(const char *path, const char *mode)
{
	if (mode[0] == 'w') return NULL;

	AAsset *asset = AAssetManager_open(gapp->activity->assetManager, path, 0);
	if (!asset) return NULL;

	return funopen(asset, android_read, android_write, android_seek, android_close);
}

__attribute__((unused))
void android_main(struct android_app *app)
{
	int main(int argc, char **argv);
	char *argv[] = { "main", 0 };

	gapp = app;
	app->onAppCmd = handle_cmd;
	app->onInputEvent = handle_input;

	main(1, argv);
}
