// gchip - a simple recompiling chip-8 emulator
// Copyright (C) 2011  Garrett Smith.
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
#include "util.h"

typedef struct egl_state {
    EGLDisplay disp;
    EGLContext ctx;
    EGLSurface surf;
} egl_state_t;

typedef struct chip8_thread {
    c8_context_t *ctx;          // chip8 emulator context
    pthread_t thread;           // handle to this thread
    int speed;                  // emulator speed (instructions/second)
    int running;                // control thread termination
} chip8_thread_t;

typedef struct perf_thread {
    c8_context_t *ctx;          // chip8 context to monitor
    pthread_t thread;           // handle to this thread
    long cycles_per_second;     // current cycles/s count
    int running;                // control thread termination
} perf_thread_t;

// -----------------------------------------------------------------------------
static int g_app_running = 1;
void sig_handler(int id)
{
    g_app_running = 0;
}

// -----------------------------------------------------------------------------
int handle_key_wait(void *data)
{
    return 0;
}

// -----------------------------------------------------------------------------
int handle_snd_ctrl(void *data, int enable)
{
    return 1;
}

// -----------------------------------------------------------------------------
static void *run_chip8_thread(void *data)
{
    // begin emulator execution
    chip8_thread_t *ct = (chip8_thread_t *)data;
    int cycles_per_tick = (int)(0.5 + ct->speed / (1000.0 / 16.0));

    while (ct->running) {
        c8_execute_cycles(ct->ctx, cycles_per_tick);
        c8_update_counters(ct->ctx, 1);
        usleep(16000);
    }
    return NULL;
}

// -----------------------------------------------------------------------------
chip8_thread_t *create_chip8_thread(c8_context_t *ctx, int speed)
{
    // create and initialize the emulator thread structure
    chip8_thread_t *ct = (chip8_thread_t *)calloc(1, sizeof(chip8_thread_t));

    // install the event handlers so we get feedback from the emulator
    c8_handlers_t handlers;
    handlers.key_wait = handle_key_wait;
    handlers.snd_ctrl = handle_snd_ctrl;
    c8_set_handlers(ctx, &handlers, ct);

    // populate the rest of the thread structure
    ct->ctx = ctx;
    ct->running = 1;
    ct->speed = speed;

    // create and launch the emulator thread
    if (0 != pthread_create(&ct->thread, NULL, run_chip8_thread, ct)) {
        log_err("error: failed to create emulator thread\n");
        SAFE_FREE(ct);
        return NULL;
    }

    return ct;
}

// -----------------------------------------------------------------------------
void destroy_chip8_thread(chip8_thread_t *ct)
{
    ct->running = 0;
    SAFE_FREE(ct);
}

// -----------------------------------------------------------------------------
static void *run_perf_thread(void *data)
{
    perf_thread_t *pt = (perf_thread_t *)data;
    int prev_cycles = pt->ctx->cycles;

    for (;;) {
        usleep(1000000);
        if (!pt->running)
            break;

        int curr_cycles = pt->ctx->cycles;
        log_info("cyles per second: %d\n", curr_cycles - prev_cycles);
        prev_cycles = curr_cycles;
    }
    return NULL;
}

// -----------------------------------------------------------------------------
perf_thread_t *create_perf_thread(c8_context_t *ctx)
{
    perf_thread_t *pt = (perf_thread_t *)calloc(1, sizeof(perf_thread_t));
    pt->ctx = ctx;
    pt->running = 1;

    if (0 != pthread_create(&pt->thread, NULL, run_perf_thread, pt)) {
        log_err("failed to create performance report thread\n");
        SAFE_FREE(pt);
        return NULL;
    }
    return pt;
}

// -----------------------------------------------------------------------------
void destroy_perf_thread(perf_thread_t *pt)
{
    pt->running = 0;
    SAFE_FREE(pt);
}

// -----------------------------------------------------------------------------
int egl_initialize(egl_state_t *g, const cmdargs_t *ca)
{
    EGLConfig cfg = 0;
    EGLint major, minor;
    EGLint num_cfg;

    // get the default display handle
    if (EGL_NO_DISPLAY == (g->disp = eglGetDisplay(EGL_DEFAULT_DISPLAY))) {
        log_err("unable to get display\n");
        return 0;
    }

    // initialize the EGL library and print version
    if (EGL_FALSE == eglInitialize(g->disp, &major, &minor)) {
        log_err("failed to initialize EGL\n");
        return 0;
    }
    log_info("Initialized EGL v%d.%d\n", major, minor);

    // bind EGL to the OpenGL ES API
    if (EGL_FALSE == eglBindAPI(EGL_OPENGL_ES_API)) {
        log_err("failed to bind OpenGL ES API\n");
        return 0;
    }

    // specify a 16-bit RGB-565 color config
    const EGLint KColorRGB565AttribList[] = {
        EGL_RED_SIZE,        5,
        EGL_GREEN_SIZE,      6,
        EGL_BLUE_SIZE,       5,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
        EGL_NONE
    };

    eglChooseConfig(g->disp, KColorRGB565AttribList, &cfg, 1, &num_cfg);
    if (0 == num_cfg) {
        log_err("no matching configs found\n");
        return 0;
    }
    log_info("found %d matching config(s)\n", num_cfg);

    // create the window surface
    if (EGL_NO_SURFACE == (g->surf = eglCreateWindowSurface(g->disp, cfg, (NativeWindowType)0, NULL))) {
        log_err("failed to create window surface\n");
        return 0;
    }

    // create and bind the rendering context
    if (EGL_NO_CONTEXT == (g->ctx = eglCreateContext(g->disp, cfg, 0, 0))) {
        log_err("failed to create rendering context\n");
        return 0;
    }

    if (EGL_FALSE == eglMakeCurrent(g->disp, g->surf, g->surf, g->ctx)) {
        log_err("failed to bind rendering context\n");
        return 0;
    }

    // enable or disable vertical sync
    eglSwapInterval(g->disp, ca->vsync ? 1 : 0);

    return 1;
}

// -----------------------------------------------------------------------------
void egl_shutdown(egl_state_t *g)
{
    log_info("Shutting down GLES...\n");
    eglDestroyContext(g->disp, g->ctx);
    eglDestroySurface(g->disp, g->surf);
    eglMakeCurrent(g->disp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate(g->disp);
}

// -----------------------------------------------------------------------------
void render_loop(egl_state_t *g, chip8_thread_t *ct)
{
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);

    //glBlendFunc(GL_ONE, GL_SRC_COLOR);
    glViewport(0, 0, 480, 272);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0.0f, 480.0f, 0.0f, 272.0f, 0.0f, 1.0f);
    // glFrustumf(-8.0f, 8.0f, -12.0f, 12.0f, -8.0f, 20.0f); 

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);

    const int emu_width = 256, emu_height = 192;
    const float display_width = 480, display_height = 272;

    int tex_dim = tex_pow2(emu_width, emu_height);
    uint8_t *tex = (uint8_t *)calloc(1, sizeof(uint8_t) * 4 * tex_dim * tex_dim);

    log_dbg("tex_dim = %d\n", tex_dim);
    float rx = (float)MCHIP_XRES / tex_dim;
    float ry = (float)MCHIP_YRES / tex_dim;

    float vertices[] = {
         0.0f,  0.0f,  0.0f,  0.0f,  ry,
         0.0f,  display_height,  0.0f,  0.0f, 0.0f,
         display_width,  0.0f,  0.0f,  rx,  ry,
         display_width,  display_height,  0.0f,  rx, 0.0f,
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

    ct->ctx->dirty = 1;
    while (g_app_running) {

        if (ct->ctx->dirty) {
            // copy the framebuffer into our texture
            uint32_t *src32 = (uint32_t *)ct->ctx->gfx;
            if (ct->ctx->system == SYSTEM_MCHIP) {
                for (int y = 0; y < MCHIP_YRES; ++y) {
                    uint32_t *line = (uint32_t *)&tex[y * tex_dim * 4];
                    for (int x = 0; x < MCHIP_XRES; ++x) {
                        line[x] = src32[y * MCHIP_XRES + x];
                    }
                }
            }
            else {
                uint8_t *src = ct->ctx->gfx;
                for (int y = 0; y < SCHIP_YRES; ++y) {
                    uint32_t *line = (uint32_t *)&tex[y * tex_dim * 4];
                    for (int x = 0; x < SCHIP_XRES; ++x) {
                        line[x] = src[y * SCHIP_XRES + x] ? 0xFFFFFFFF : 0;
                    }
                }
            }

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                    tex_dim, tex_dim, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex);

            ct->ctx->dirty = 0;
        }

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

        eglSwapBuffers(g->disp, g->surf);
    }
}

// -----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    // register CTRL+C to exit
    signal(SIGINT, sig_handler);

    // process the command line arguments and check for validity
    cmdargs_t ca;
    if (0 > cmdline_parse(argc, argv, &ca))
        cmdline_display_usage();

    // create the emulator context and load the specified rom file
    c8_context_t *ctx;
    c8_create_context(&ctx, ca.mode);
    if (0 > c8_load_file(ctx, ca.rompath)) {
        log_err("failed to load rom\n");
        return 1;
    }

    c8_set_debugger_enabled(ctx, ca.debugger);

    // create the display and initialize GLES
    egl_state_t gl;
    if (!egl_initialize(&gl, &ca)) {
        log_err("failed to initialize GLES\n");
        return EXIT_FAILURE;
    }

    chip8_thread_t *ct = create_chip8_thread(ctx, ca.speed);
    perf_thread_t *pt = create_perf_thread(ctx);

    render_loop(&gl, ct);

    // free up whatever resources we've allocated
    destroy_perf_thread(pt);
    destroy_chip8_thread(ct);
    c8_destroy_context(ctx);
    egl_shutdown(&gl);
    return EXIT_SUCCESS;
}

