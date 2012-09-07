// gchip - a simple recompiling chip-8 emulator
// Copyright (C) 2011-2012  Garrett Smith.
// 
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2 of the License, or (at your
// option) any later version.
// 
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.
// 
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifdef USE_X11
#   include <X11/Xlib.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <GLES/egl.h>
#include <GLES/gl.h>

#include "cmdline.h"
#include "chip8.h"

#define EGL_CHECK(expr)                                                     \
    expr; {                                                                 \
        EGLint error = eglGetError();                                       \
        if (error != EGL_SUCCESS) {                                         \
            fprintf(stderr, "%s:%d - error 0x%x - %s\n",                    \
                    __FILE__, __LINE__, error, #expr);                      \
            return 0;                                                       \
        }                                                                   \
    }

#define FB_SIZE     (256 * 192 * 4)
#define DISPLAY_X   480
#define DISPLAY_Y   272

typedef struct window_state {
    NativeDisplayType native_display;   // native display handle
    NativeWindowType  native_window;    // native window handle
    EGLDisplay        display;          // EGL display handle
    EGLContext        context;          // EGL context handle
    EGLSurface        surface;          // EGL surface handle
} window_state_t;

typedef struct chip8_thread {
    c8_context_t *context;              // chip8 emulator context
    pthread_t     thread;               // handle to this thread
    unsigned int  speed;                // emulator speed (instructions/second)
    unsigned int  running;              // control thread termination
    uint8_t       fb[FB_SIZE];          // framebuffer
} chip8_thread_t;

typedef struct perf_thread {
    c8_context_t *context;              // chip8 context to monitor
    pthread_t     thread;               // handle to this thread
    unsigned int  running;              // control thread termination
} perf_thread_t;

// -----------------------------------------------------------------------------
// Return the nearest square, pow2 texture dimension >= MAX(width, height).
//
INLINE unsigned int tex_pow2(unsigned int width, unsigned int height)
{
    unsigned int input = MAX(width, height);
    if (0 == (input & (input - 1)))
        return input;

    unsigned int value = 2;
    while (0 != (input >>= 1))
        value <<= 1;
    return value;
}

// -----------------------------------------------------------------------------
// Signal program termination when CTRL+C is pressed.
//
static int g_app_running = 1;
void sig_handler(int id)
{
    g_app_running = 0;
}

// -----------------------------------------------------------------------------
// Callback for emulator key wait events (not handled).
//
int handle_key_wait(void *data)
{
    return 0;
}

// -----------------------------------------------------------------------------
// Callback for emulator sound control events (not handled).
//
int handle_snd_ctrl(void *data, int enable)
{
    return 1;
}

// -----------------------------------------------------------------------------
// Callback for emulator video mode events (not handled).
//
int handle_set_mode(void *data, int system, int width, int height)
{
    return 1;
}

// -----------------------------------------------------------------------------
// Callback for emulator video sync events -- snap the framebuffer.
//
int handle_vid_sync(void *data)
{
    chip8_thread_t *ct = (chip8_thread_t *)data;
    memcpy(ct->fb, ct->context->gfx, ct->context->gfx_size);
    return 1;
}

// -----------------------------------------------------------------------------
// CHIP8 emulator thread.
//
static void *run_chip8_thread(void *data)
{
    // begin emulator execution
    chip8_thread_t *ct = (chip8_thread_t *)data;
    unsigned int cycles_per_tick = (int)(0.5 + ct->speed / (1000.0 / 16.0));

    while (ct->running) {
        c8_execute_cycles(ct->context, cycles_per_tick);
        c8_update_counters(ct->context, 1);
        usleep(16000);
    }
    return NULL;
}

// -----------------------------------------------------------------------------
// Create and initialize the CHIP8 emulator thread.
//
chip8_thread_t *create_chip8_thread(c8_context_t *ctx, unsigned int speed)
{
    chip8_thread_t *ct = (chip8_thread_t *)calloc(1, sizeof(chip8_thread_t));

    // install the event handlers so we get feedback from the emulator
    c8_handlers_t handlers = {
        handle_key_wait,
        handle_snd_ctrl,
        handle_set_mode,
        handle_vid_sync
    };

    c8_set_handlers(ctx, &handlers, ct);

    // populate the rest of the thread structure
    ct->context = ctx;
    ct->running = 1;
    ct->speed   = speed;

    // create and launch the emulator thread
    if (0 != pthread_create(&ct->thread, NULL, run_chip8_thread, ct)) {
        log_err("error: failed to create emulator thread\n");
        SAFE_FREE(ct);
        return NULL;
    }

    return ct;
}

// -----------------------------------------------------------------------------
// Destroy the CHIP8 emulator thread.
//
void destroy_chip8_thread(chip8_thread_t *ct)
{
    ct->running = 0;
    SAFE_FREE(ct);
}

// -----------------------------------------------------------------------------
// Performance monitor thread.
//
static void *run_perf_thread(void *data)
{
    perf_thread_t *pt = (perf_thread_t *)data;
    int prev_cycles = pt->context->cycles;

    for (;;) {
        usleep(1000000);
        if (!pt->running)
            break;

        int curr_cycles = pt->context->cycles;
        log_info("cyles per second: %d\n", curr_cycles - prev_cycles);
        prev_cycles = curr_cycles;
    }
    return NULL;
}

// -----------------------------------------------------------------------------
// Create and initialize the performance monitor thread.
//
perf_thread_t *create_perf_thread(c8_context_t *ctx)
{
    perf_thread_t *pt = (perf_thread_t *)calloc(1, sizeof(perf_thread_t));
    pt->context = ctx;
    pt->running = 1;

    if (0 != pthread_create(&pt->thread, NULL, run_perf_thread, pt)) {
        log_err("failed to create performance report thread\n");
        SAFE_FREE(pt);
        return NULL;
    }
    return pt;
}

// -----------------------------------------------------------------------------
// Destroy the performance monitor thread.
//
void destroy_perf_thread(perf_thread_t *pt)
{
    pt->running = 0;
    SAFE_FREE(pt);
}

#ifdef USE_X11
// -----------------------------------------------------------------------------
Bool WaitForMap(Display *display, XEvent *event, XPointer arg)
{
    if (event->type == MapNotify && event->xmap.window == (*((Window*)arg)))
        return True;
    return False;
}

// -----------------------------------------------------------------------------
Window CreateWindow(const char *title, int x, int y, Display *display, int vid)
{
    int num_items;
    XEvent event;
    XVisualInfo *pv, template;

    template.visualid = vid;
    pv = XGetVisualInfo(display, VisualIDMask, &template, &num_items);
    Window root = RootWindow(display, DefaultScreen(display));
    Colormap colormap = XCreateColormap(display, root, pv->visual, AllocNone);

    XSetWindowAttributes attr;
    attr.colormap         = colormap;
    attr.background_pixel = 0xFFFFFFFF;
    attr.border_pixel     = 0;
    attr.event_mask       = StructureNotifyMask | ExposureMask;

    unsigned long mask = CWBackPixel | CWBorderPixel | CWEventMask | CWColormap;
    Window window = XCreateWindow(display, root, 0, 0, x, y, 0, pv->depth,
                                  InputOutput, pv->visual, mask, &attr);

    XSizeHints hint;
    hint.flags = USPosition;
    hint.x     = 10;
    hint.y     = 10;

    XSetStandardProperties(display, window, title, title, None, 0, 0, &hint);
    XMapWindow(display, window);
    XIfEvent(display, &event, WaitForMap, (char*)&window);
    XSetWMColormapWindows(display, window, &window, 1);
    XFlush(display);

    return window;
}
#endif

// -----------------------------------------------------------------------------
// Create the render surface. Use either the default display or an X11 window.
//
int window_initialize(window_state_t *wnd, const cmdargs_t *args)
{
    EGLConfig cfg = 0;
    EGLint major, minor, num_cfg;

    const EGLint attributes[] = {
        EGL_RED_SIZE,        5,
        EGL_GREEN_SIZE,      6,
        EGL_BLUE_SIZE,       5,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
        EGL_NONE
    };

    wnd->native_display = EGL_DEFAULT_DISPLAY;
    wnd->native_window  = (NativeWindowType)NULL;

#ifdef USE_X11
    wnd->native_display = (NativeDisplayType)XOpenDisplay(NULL);
    if (NULL == wnd->native_display) {
        printf("failed to open display.\n");
        return 0;
    }
#endif

    // initialize the EGL library and create the display
    wnd->display = EGL_CHECK(eglGetDisplay(wnd->native_display));
    EGL_CHECK(eglInitialize(wnd->display, &major, &minor));
    EGL_CHECK(eglChooseConfig(wnd->display, attributes, &cfg, 1, &num_cfg));

    log_info("initialized EGL v%d.%d (%d configs)\n", major, minor, num_cfg);

#ifdef USE_X11
    int id;
    EGL_CHECK(eglGetConfigAttrib(wnd->display, cfg, EGL_NATIVE_VISUAL_ID, &id));

    wnd->native_window = CreateWindow("gchip-egl", DISPLAY_X, DISPLAY_Y, wnd->native_display, id);
    if (!wnd->native_window) {
        log_info("failed to create X11 window\n");
        return 0;
    }
#endif

    // bind EGL to the OpenGL ES API and create the render window
    EGL_CHECK(eglBindAPI(EGL_OPENGL_ES_API));

    wnd->surface = EGL_CHECK(eglCreateWindowSurface(wnd->display, cfg, wnd->native_window, NULL));
    wnd->context = EGL_CHECK(eglCreateContext(wnd->display, cfg, 0, 0));

    EGL_CHECK(eglMakeCurrent(wnd->display, wnd->surface, wnd->surface, wnd->context));
    EGL_CHECK(eglSwapInterval(wnd->display, args->vsync ? 1 : 0));

    return 1;
}

// -----------------------------------------------------------------------------
// Destroy the render surface.
//
void window_shutdown(window_state_t *wnd)
{
#ifdef USE_X11
    Window window = (Window)wnd->native_window;
    Display *display = (Display *)wnd->native_display;
    XWindowAttributes attributes;

    log_info("Shutting down X11 window...\n");
    XGetWindowAttributes(display, window, &attributes);
    XDestroyWindow(display, window);
    XFreeColormap(display, attributes.colormap);
    // XFree(pVisual);
    XCloseDisplay(display);
#endif

    log_info("Shutting down GLES...\n");
    eglDestroyContext(wnd->display, wnd->context);
    eglDestroySurface(wnd->display, wnd->surface);
    eglMakeCurrent(wnd->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate(wnd->display);
}

// -----------------------------------------------------------------------------
// Description.
//
void window_render(window_state_t *wnd, chip8_thread_t *ct)
{
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);

    glViewport(0, 0, DISPLAY_X, DISPLAY_Y);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0.0f, DISPLAY_X, 0.0f, DISPLAY_Y, 0.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);

    const int emu_width = 256, emu_height = 192;
    int tex_dim = tex_pow2(emu_width, emu_height);
    uint8_t *tex = (uint8_t *)calloc(1, sizeof(uint8_t) * 4 * tex_dim * tex_dim);

    log_dbg("tex_dim = %d\n", tex_dim);
    float rx = (float)MCHIP_XRES / tex_dim;
    float ry = (float)MCHIP_YRES / tex_dim;

    float vertices[] = {
         0.0f,      0.0f,       0.0f,   0.0f,   ry,
         0.0f,      DISPLAY_Y,  0.0f,   0.0f,   0.0f,
         DISPLAY_X, 0.0f,       0.0f,   rx,     ry,
         DISPLAY_X, DISPLAY_Y,  0.0f,   rx,     0.0f,
    };
    unsigned int vbo_length = 4 * (sizeof(float) * 5);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vbo_length, vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    ct->context->dirty = 1;
    while (g_app_running) {

        if (ct->context->system == SYSTEM_MCHIP) {
            uint32_t *src32 = (uint32_t *)ct->fb;
            if (ct->context->system == SYSTEM_MCHIP) {
                for (int y = 0; y < MCHIP_YRES; ++y) {
                    uint32_t *line = (uint32_t *)&tex[y * tex_dim * 4];
                    for (int x = 0; x < MCHIP_XRES; ++x) {
                        line[x] = src32[y * MCHIP_XRES + x];
                    }
                }
            }
        }
        else if (ct->context->dirty) {
            // copy the framebuffer into our texture
            ct->context->dirty = 0;
            uint8_t *src = ct->context->gfx;
            for (int y = 0; y < SCHIP_YRES; ++y) {
                uint32_t *line = (uint32_t *)&tex[y * tex_dim * 4];
                for (int x = 0; x < SCHIP_XRES; ++x) {
                    line[x] = src[y * SCHIP_XRES + x] ? 0xFFFFFFFF : 0;
                }
            }
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_dim, tex_dim, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);

        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glVertexPointer(3, GL_FLOAT, sizeof(float) * 5, 0);
        glTexCoordPointer(2, GL_FLOAT, sizeof(float) * 5, (unsigned char*) (sizeof(float) * 3));
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);

        eglSwapBuffers(wnd->display, wnd->surface);
    }
}

// -----------------------------------------------------------------------------
// Program entry point.
//
int main(int argc, char *argv[])
{
    cmdargs_t args;
    c8_context_t *ctx;
    window_state_t wnd;

    // register CTRL+C to exit
    signal(SIGINT, sig_handler);

    // process the command line arguments and check for validity
    if (0 > cmdline_parse(argc, argv, &args))
        cmdline_display_usage();

    // create the emulator context and load the specified rom file
    c8_create_context(&ctx, args.mode);
    if (0 > c8_load_file(ctx, args.rompath)) {
        log_err("failed to load rom\n");
        return 1;
    }

    c8_set_debugger_enabled(ctx, args.debugger);

    // create the display and initialize GLES
    if (!window_initialize(&wnd, &args)) {
        log_err("failed to initialize render window\n");
        return EXIT_FAILURE;
    }

    chip8_thread_t *ct = create_chip8_thread(ctx, args.speed);
    perf_thread_t  *pt = create_perf_thread(ctx);

    window_render(&wnd, ct);

    // free up whatever resources we've allocated
    destroy_perf_thread(pt);
    destroy_chip8_thread(ct);
    c8_destroy_context(ctx);
    window_shutdown(&wnd);

    return EXIT_SUCCESS;
}

