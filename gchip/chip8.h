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

#ifndef GCHIP_CHIP8__H
#define GCHIP_CHIP8__H

#include "common.h"

#define ROM_SIZE    0x1000
#define STACK_SIZE  32
#define LFONT_SIZE  16 * 5
#define HFONT_SIZE  16 * 10

#define CHIP8_XRES  64
#define CHIP8_YRES  32

#define HCHIP_XRES  64
#define HCHIP_YRES  64

#define SCHIP_XRES  128
#define SCHIP_YRES  64

#define MCHIP_XRES  256
#define MCHIP_YRES  192

#define SYSTEM_CHIP8    0       // standard chip-8 (64 x 32)
#define SYSTEM_HCHIP    1       // hi-resolution chip-8 (64 x 64)
#define SYSTEM_SCHIP    2       // superchip-8 (128 x 64)
#define SYSTEM_MCHIP    3       // megachip-8 (256 x 192)

#define MODE_CASE   0
#define MODE_PTR    1
#define MODE_CACHE  2
#define MODE_DBT    3
#define MODE_TEST   4

#define EXEC_BREAK  (1 << 0)
#define EXEC_DEBUG  (1 << 1)
#define EXEC_SUBSET (1 << 2)

#define OP_X    ((ctx->opcode >> 8) & 0xF)
#define OP_Y    ((ctx->opcode >> 4) & 0xF)
#define OP_N    (ctx->opcode & 0xF)
#define OP_B    (ctx->opcode & 0xFF)
#define OP_T    (ctx->opcode & 0xFFF)
#define OP_24   ((OP_B << 16) | (ctx->rom[ctx->pc] << 8) | ctx->rom[ctx->pc+1])

typedef int (*key_wait_fn)(void *);
typedef int (*snd_ctrl_fn)(void *, int);
typedef int (*set_mode_fn)(void *, int);
typedef int (*vid_sync_fn)(void *);

typedef struct c8_handlers {
    key_wait_fn key_wait;       // wait for user keypad input
    snd_ctrl_fn snd_ctrl;       // enable or disable sound
    set_mode_fn set_mode;       // set machine mode (chip8/schip)
    vid_sync_fn vid_sync;       // synchronize display (for MegaChip)
} c8_handlers_t;

typedef struct c8_context {
    int v[16];                  // general purpose registers [V0, VF]
    int i, sp, pc, dt, st;      // special purpose registers
    int opcode;                 // cache the current instruction
    void *userdata;             // user data passed to event handlers
    c8_handlers_t fn;           // table of event handlers
    long cycles, max_cycles;    // cycle counter and limiter
    int dirty;                  // indicates display has been updated
    int system;                 // keep track of the current system setting
    int mode, exec_flags;       // interpereter mode and execution flags
    int keypad[16];             // hexadecimal keypad states
    int sound_on;               // keep track of beep state
    int stack[STACK_SIZE];      // stack space
    uint8_t *rom;               // program address space
    uint8_t *gfx;               // graphics framebuffer
    int rom_size;               // size of program address space
    int gfx_size;               // size of graphics framebuffer
#ifdef HAVE_SCHIP_SUPPORT
    int hp[8];                  // HP48/RPL registers
#endif // HAVE_SCHIP_SUPPORT
#ifdef HAVE_MCHIP_SUPPORT
    int spr_width, spr_height;  // megachip sprite dimensions
    uint32_t palette[256];      // megachip color palette
#endif // HAVE_MCHIP_SUPPORT
} c8_context_t;

void c8_create_context(c8_context_t **pctx, int mode);
void c8_destroy_context(c8_context_t *ctx);
int  c8_load_file(c8_context_t *ctx, const char *path);
long c8_execute_cycles(c8_context_t *ctx, long cycles);
void c8_update_counters(c8_context_t *ctx, int delta);

void c8_set_system(c8_context_t *ctx, int system);
void c8_set_handlers(c8_context_t *ctx, c8_handlers_t *fn, void *data);
void c8_set_debugger_enabled(c8_context_t *ctx, int enable);
void c8_set_key_state(c8_context_t *ctx, unsigned int index, int state);

void c8_debug_disassemble(const c8_context_t *ctx, char *o, int s);
int  c8_debug_instruction(const c8_context_t *ctx, uint16_t pc);
int  c8_debug_lockstep_test(const char *path);
int  c8_debug_cmp_context(const c8_context_t *a, const c8_context_t *b);
void c8_debug_dump_context(const c8_context_t *ctx);

void gfx_scroll_down(c8_context_t *ctx, int lines);
void gfx_scroll_up(c8_context_t *ctx, int lines);
void gfx_scroll_right(c8_context_t *ctx);
void gfx_scroll_left(c8_context_t *ctx);
int  gfx_draw_sprite(c8_context_t *ctx, int x, int y, int n);

#endif // GCHIP_CHIP8__H

