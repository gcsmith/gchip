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
#include <string.h>
#include <assert.h>
#include "SDL.h"
#include "SDL_thread.h"
#include "audio.h"
#include "chip8.h"
#include "cmdline.h"
#include "graphics.h"

typedef struct chip8_thread {
    c8_context_t *ctx;          // chip8 emulator context
    SDL_Thread *thread;         // handle to this thread
    SDL_mutex *input_lock;      // blocking input lock
    SDL_cond *input_cond;       // blocking input condition
    int keymap[256];            // map SDL key press to hex keypad
    int last_keypress;          // keep track of last hex keypad index
    int speed;                  // emulator speed (instructions/second)
    int running;                // control thread termination
    uint8_t *framebuffer;
} chip8_thread_t;

typedef struct perf_thread {
    c8_context_t *ctx;          // chip8 context to monitor
    SDL_Thread *thread;         // handle to this thread
    SDL_mutex *perf_lock;       // lock for terminating condition
    SDL_cond *perf_cond;        // terminating condition
    long cycles_per_second;     // current cycles/s count
    int running;                // control thread termination
} perf_thread_t;

typedef struct window_state {
    SDL_Window *window;         // handle to SDL window
    SDL_GLContext glcontext;    // handle to GL render context
    int width, height;          // width and height of display window
    int bgcolor, fgcolor;       // background and foreground colors
    int fullscreen;             // fullscreen toggle
} window_state_t;

cmdargs_t g_ca;

// -----------------------------------------------------------------------------
int handle_key_wait(void *data)
{
    chip8_thread_t *ct = (chip8_thread_t *)data;

    // block the emulator until the GUI thread signals a key was pressed
    SDL_LockMutex(ct->input_lock);
    SDL_CondWait(ct->input_cond, ct->input_lock);

    // unlock the input mutex and return the key index to the emulator
    SDL_UnlockMutex(ct->input_lock);
    return ct->last_keypress;
}

// -----------------------------------------------------------------------------
int handle_snd_ctrl(void *data, int enable)
{
    if (enable)
        SDL_PauseAudio(0);
    else
        SDL_PauseAudio(1);
    return 1;
}

// -----------------------------------------------------------------------------
int handle_set_mode(void *data, int system)
{
    return 1;
}

// -----------------------------------------------------------------------------
int handle_vid_sync(void *data)
{
    chip8_thread_t *ct = (chip8_thread_t *)data;
    memcpy(ct->framebuffer, ct->ctx->gfx, ct->ctx->gfx_size);
    return 1;
}

// -----------------------------------------------------------------------------
void init_key_mappings(int *key)
{
    for (int i = 0; i < 256; i++) key[i] = -1;
    key[SDLK_1] = 0x1; key[SDLK_2] = 0x2; key[SDLK_3] = 0x3; key[SDLK_4] = 0xC;
    key[SDLK_q] = 0x4; key[SDLK_w] = 0x5; key[SDLK_e] = 0x6; key[SDLK_r] = 0xD;
    key[SDLK_a] = 0x7; key[SDLK_s] = 0x8; key[SDLK_d] = 0x9; key[SDLK_f] = 0xE;
    key[SDLK_z] = 0xA; key[SDLK_x] = 0x0; key[SDLK_c] = 0xB; key[SDLK_v] = 0xF;
}

// -----------------------------------------------------------------------------
// Execute the emulator on a separate thread from the GUI.
// 
int run_chip8_thread(void *data)
{
    // perform audio initialization
    audio_data_t audio;
    audio_init(&audio);

    // begin execution
    chip8_thread_t *ct = (chip8_thread_t *)data;
    int cycles_per_tick = (int)(0.5 + ct->speed / (1000.0 / 16.0));

    while (ct->running) {
        c8_execute_cycles(ct->ctx, cycles_per_tick);
        c8_update_counters(ct->ctx, 1);
        SDL_Delay(16);
    }

    // perform shutdown
    audio_shutdown(&audio);
    return 0;
}

// -----------------------------------------------------------------------------
chip8_thread_t *create_chip8_thread(c8_context_t *ctx, int speed)
{
    // create and initialize the emulator thread structure
    chip8_thread_t *ct = (chip8_thread_t *)calloc(1, sizeof(chip8_thread_t));
    init_key_mappings(ct->keymap);

    // install the event handlers so we get feedback from the emulator
    c8_handlers_t handlers;
    handlers.key_wait = handle_key_wait;
    handlers.snd_ctrl = handle_snd_ctrl;
    handlers.set_mode = handle_set_mode;
    handlers.vid_sync = handle_vid_sync;
    c8_set_handlers(ctx, &handlers, ct);

    // condition variable used to implement blocking keypad input
    ct->input_lock = SDL_CreateMutex();
    ct->input_cond = SDL_CreateCond();

    // populate the rest of the thread structure
    ct->ctx = ctx;
    ct->running = 1;
    ct->speed = speed;
    ct->framebuffer = malloc(MCHIP_XRES * MCHIP_YRES * 4);

    // create and launch the emulator thread
    if (NULL == (ct->thread = SDL_CreateThread(run_chip8_thread, ct))) {
        log_err("failed to create emulator thread\n");
        SAFE_FREE(ct);
        return NULL;
    }

    return ct;
}

// -----------------------------------------------------------------------------
void destroy_chip8_thread(chip8_thread_t *ct)
{
    ct->running = 0;

    // need to fire the input event signal in case the emulator is blocked
    SDL_CondSignal(ct->input_cond);
    SDL_WaitThread(ct->thread, NULL);

    // finally, free up whatever resources we've allocated
    SDL_DestroyMutex(ct->input_lock);
    SDL_DestroyCond(ct->input_cond);

    SAFE_FREE(ct->framebuffer);
    SAFE_FREE(ct);
}

// -----------------------------------------------------------------------------
// Display the number of instructions executed each second.
//
int run_perf_thread(void *data)
{
    perf_thread_t *pt = (perf_thread_t *)data;
    int prev_cycles = pt->ctx->cycles;

    for (;;) {
        SDL_LockMutex(pt->perf_lock);
        SDL_CondWaitTimeout(pt->perf_cond, pt->perf_lock, 1000);
        if (!pt->running)
            break;

        int curr_cycles = pt->ctx->cycles;
        log_info("cyles per second: %d\n", curr_cycles - prev_cycles);
        prev_cycles = curr_cycles;
    }
    return 0;
}

// -----------------------------------------------------------------------------
perf_thread_t *create_perf_thread(c8_context_t *ctx)
{
    perf_thread_t *pt = (perf_thread_t *)calloc(1, sizeof(perf_thread_t));
    pt->ctx = ctx;
    pt->running = 1;

    pt->perf_lock = SDL_CreateMutex();
    pt->perf_cond = SDL_CreateCond();

    pt->thread = SDL_CreateThread(run_perf_thread, pt);
    if (pt->thread)
        return pt;

    log_err("failed to create performance report thread\n");
    SAFE_FREE(pt);
    return NULL;
}

// -----------------------------------------------------------------------------
void destroy_perf_thread(perf_thread_t *pt)
{
    pt->running = 0;

    // signal the thread to terminate
    SDL_CondSignal(pt->perf_cond);
    SDL_WaitThread(pt->thread, NULL);

    // finally, free up whatever resources we've allocated
    SDL_DestroyMutex(pt->perf_lock);
    SDL_DestroyCond(pt->perf_cond);
    SAFE_FREE(pt);
}

// -----------------------------------------------------------------------------
void print_version_info(void)
{
    // SDL compiled and linked versions
    SDL_version cv;
    SDL_VERSION(&cv);
    log_info("SDL compiled version: %d.%d.%d\n", cv.major, cv.minor, cv.patch);

    SDL_version lv;
    SDL_GetVersion(&lv);
    log_info("SDL linked version: %d.%d.%d\n", lv.major, lv.minor, lv.patch);

    // OpenGL context and driver versions
    int major = 0, minor = 0;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
    log_info("GL context version: %d.%d\n", major, minor);

#ifdef GL_VERSION
    const char *gl_ver = (const char *)glGetString(GL_VERSION);
    log_info("GL driver version: %s\n", gl_ver);
#endif

#ifdef GL_SHADING_LANGUAGE_VERSION
    const char *sl_ver = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
    log_info("GLSL driver version: %s\n", sl_ver);
#endif

#ifdef GLEW_VERSION
    const char *glew_ver = (const char *)glewGetString(GLEW_VERSION);
    log_info("GLEW version: %s\n", glew_ver);
#endif
}

// -----------------------------------------------------------------------------
// Create an SDL window of the specified dimensions and initialize GL/GLEW.
//
int create_window(SDL_Window **window, SDL_GLContext *context, cmdargs_t *ca)
{
    assert(NULL != window);
    assert(NULL != context);
    assert(NULL != ca);

    log_info("Initializing SDL subsystems...\n");
    if (0 > SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        log_err("SDL initialization failed (%s)\n", SDL_GetError());
        return -1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    int flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    if (ca->fullscreen)
        flags |= SDL_WINDOW_FULLSCREEN;

    // create the application window
    *window = SDL_CreateWindow(gchip_desc, SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED, ca->width, ca->height, flags);
    if (NULL == *window) {
        log_err("failed to create SDL window (%s)\n", SDL_GetError());
        return -1;
    }

    // create the opengl rendering context and initialize GLEW
    log_info("Initializing GL context...\n");
    *context = SDL_GL_CreateContext(*window);
    if (NULL == *context) {
        log_err("failed to create GL context (%s)\n", SDL_GetError());
        return -1;
    }

    log_info("Initializing GLEW...\n");
    GLenum rc = glewInit();
    if (GLEW_OK != rc) {
        log_err("failed to initialize GLEW (%s)\n", glewGetErrorString(rc));
        return -1;
    }

    // enable or disable vertical sync
    SDL_GL_SetSwapInterval(ca->vsync ? 1 : 0);

    print_version_info();
    return 0;
}

// -----------------------------------------------------------------------------
// Process any pending SDL window events.
//
int process_window_events(chip8_thread_t *ct, window_state_t *ws)
{
    SDL_Event event;
    SDL_bool fullscreen;
    int keypad_index;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_WINDOWEVENT:
            // adjust the opengl viewport if the window is resized
            switch (event.window.event) {
            case SDL_WINDOWEVENT_RESIZED:
                glViewport(0, 0, event.window.data1, event.window.data2);
                SDL_GL_SwapWindow(ws->window);
                break;
            }
            break;
        case SDL_KEYDOWN:
            if (event.key.keysym.mod & KMOD_ALT) {
                if (event.key.keysym.sym == SDLK_RETURN) {
                    // toggle between windowed and fullscreen modes
                    ws->fullscreen = !ws->fullscreen;
                    fullscreen = ws->fullscreen ? SDL_TRUE : SDL_FALSE;
                    SDL_SetWindowFullscreen(ws->window, fullscreen);
                }

                if (event.key.keysym.sym == SDLK_F4) {
                    // terminate the application when ALT+F4 pressed
                    return 0;
                }
            }
            keypad_index = ct->keymap[event.key.keysym.sym & 0xFF];
            if (keypad_index >= 0) {
                // valid keypad input - update the emulator context
                c8_set_key_state(ct->ctx, keypad_index, 1);

                // signal the emulator in case its waiting for input
                ct->last_keypress = keypad_index;
                SDL_CondSignal(ct->input_cond);
            }
            break;
        case SDL_KEYUP:
            keypad_index = ct->keymap[event.key.keysym.sym & 0xFF];
            if (keypad_index >= 0)
                c8_set_key_state(ct->ctx, keypad_index, 0);
            break;
        case SDL_QUIT:
            return 0;
        }
    }
    return 1;
}

// -----------------------------------------------------------------------------
// Main GUI loop, process events and perform rendering.
//
int window_event_pump(chip8_thread_t *ct, window_state_t *ws)
{
    graphics_t gfx;
    graphics_init(&gfx, ws->width, ws->height, ws->bgcolor, ws->fgcolor);

    c8_context_t *ctx = ct->ctx;
    ctx->dirty = 1;
    while (process_window_events(ct, ws)) {
        // only update the destination surface when the display has changed

        if (ctx->system == SYSTEM_MCHIP) {
            graphics_update(&gfx, ct->framebuffer, ctx->system);
        }
        else if (ctx->dirty) {
            ctx->dirty = 0;
            graphics_update(&gfx, ctx->gfx, ctx->system);
        }

        // render the surface and swap buffers
        graphics_render(&gfx, ctx->system);
        SDL_GL_SwapWindow(ws->window);
    }

    return 1;
}

// -----------------------------------------------------------------------------
void exit_cleanup(void)
{
    cmdline_destroy(&g_ca);
    SDL_Quit();
}

// -----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    cmdargs_t *ca = &g_ca;
    atexit(exit_cleanup);

    // process the command line arguments and check for validity
    if (0 > cmdline_parse(argc, argv, ca))
        cmdline_display_usage();

    // special case: perform lockstep execution test (CASE & DBT)
    if (ca->mode == MODE_TEST) {
        log_info("comparing interpreter and binary translator...\n");
        return c8_debug_lockstep_test(ca->rompath);
    }

    // create the emulator context and load the specified rom file
    c8_context_t *ctx;
    c8_create_context(&ctx, ca->mode);
    if (0 > c8_load_file(ctx, ca->rompath)) {
        log_err("failed to load rom \"%s\"\n", ca->rompath);
        return 1;
    }

    c8_set_debugger_enabled(ctx, ca->debugger);

    // create and display the application window
    SDL_Window *window;
    SDL_GLContext glcontext;
    if (0 > create_window(&window, &glcontext, ca))
        return 1;

    // create the emulator and performance monitoring threads
    chip8_thread_t *ct = create_chip8_thread(ctx, ca->speed);
    perf_thread_t *pt = create_perf_thread(ctx);

    // begin processing of window events
    window_state_t ws;
    ws.window     = window;
    ws.glcontext  = glcontext;
    ws.fullscreen = ca->fullscreen;
    ws.bgcolor    = ca->bgcolor;
    ws.fgcolor    = ca->fgcolor;
    ws.width      = ca->width;
    ws.height     = ca->height;
    window_event_pump(ct, &ws);

    // free up whatever resources we've allocated
    destroy_perf_thread(pt);
    destroy_chip8_thread(ct);
    c8_destroy_context(ctx);
    return 0;
}

