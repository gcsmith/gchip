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
#include <time.h>
#include <assert.h>
#include "chip8.h"
#include "xlat.h"

extern long c8_execute_cycles_ptr(c8_context_t *ctx, long cycles);
extern long c8_execute_cycles_case(c8_context_t *ctx, long cycles);
extern long c8_execute_cycles_cache(c8_context_t *ctx, long cycles);
extern long c8_execute_cycles_dbt(c8_context_t *ctx, long cycles);
extern void init_dispatch_tables(void);

// font table taken from Cowgod's Chip-8 Technical Reference v1.0
static const uint8_t lfont_rom[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, 0x20, 0x60, 0x20, 0x20, 0x70, // 0, 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, 0xF0, 0x10, 0xF0, 0x10, 0xF0, // 2, 3
    0x90, 0x90, 0xF0, 0x10, 0x10, 0xF0, 0x80, 0xF0, 0x10, 0xF0, // 4, 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, 0xF0, 0x10, 0x20, 0x40, 0x40, // 6, 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, 0xF0, 0x90, 0xF0, 0x10, 0xF0, // 8, 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, 0xE0, 0x90, 0xE0, 0x90, 0xE0, // A, B
    0xF0, 0x80, 0x80, 0x80, 0xF0, 0xE0, 0x90, 0x90, 0x90, 0xE0, // C, D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, 0xF0, 0x80, 0xF0, 0x80, 0x80  // E, F
};

#ifdef HAVE_SCHIP_SUPPORT
// extended font table taken from Doomulation's Chip-8 documentation
static const uint8_t hfont_rom[160] = { 
    0xF0, 0xF0, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0xF0, 0xF0, // 0
    0x20, 0x20, 0x60, 0x60, 0x20, 0x20, 0x20, 0x20, 0x70, 0x70, // 1
    0xF0, 0xF0, 0x10, 0x10, 0xF0, 0xF0, 0x80, 0x80, 0xF0, 0xF0, // 2
    0xF0, 0xF0, 0x10, 0x10, 0xF0, 0xF0, 0x10, 0x10, 0xF0, 0xF0, // 3
    0x90, 0x90, 0x90, 0x90, 0xF0, 0xF0, 0x10, 0x10, 0x10, 0x10, // 4
    0xF0, 0xF0, 0x80, 0x80, 0xF0, 0xF0, 0x10, 0x10, 0xF0, 0xF0, // 5
    0xF0, 0xF0, 0x80, 0x80, 0xF0, 0xF0, 0x90, 0x90, 0xF0, 0xF0, // 6
    0xF0, 0xF0, 0x10, 0x10, 0x20, 0x20, 0x40, 0x40, 0x40, 0x40, // 7
    0xF0, 0xF0, 0x90, 0x90, 0xF0, 0xF0, 0x90, 0x90, 0xF0, 0xF0, // 8
    0xF0, 0xF0, 0x90, 0x90, 0xF0, 0xF0, 0x10, 0x10, 0xF0, 0xF0, // 9
    0xF0, 0xF0, 0x90, 0x90, 0xF0, 0xF0, 0x90, 0x90, 0x90, 0x90, // A
    0xE0, 0xE0, 0x90, 0x90, 0xE0, 0xE0, 0x90, 0x90, 0xE0, 0xE0, // B
    0xF0, 0xF0, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xF0, 0xF0, // C
    0xE0, 0xE0, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0xE0, 0xE0, // D
    0xF0, 0xF0, 0x80, 0x80, 0xF0, 0xF0, 0x80, 0x80, 0xF0, 0xF0, // E
    0xF0, 0xF0, 0x80, 0x80, 0xF0, 0xF0, 0x80, 0x80, 0x80, 0x80  // F
};
#endif

// -----------------------------------------------------------------------------
void c8_create_context(c8_context_t **pctx, int mode)
{
    c8_context_t *ctx = (c8_context_t *)low_calloc(sizeof(c8_context_t));

    // initialize program counter and stack pointer
    ctx->pc = 0x200;
    ctx->sp = 0;

    ctx->exec_flags = 0;
    ctx->mode = mode;
    ctx->cycles = 0;
    ctx->max_cycles = 0;
    ctx->dirty = 0;
    ctx->system = SYSTEM_CHIP8;

    // start with a standard ROM size, but we may need to increase for MCHIP
    ctx->rom_size = ROM_SIZE;
    ctx->rom = (uint8_t *)low_calloc(ctx->rom_size);

    // start with a standard FB size, but we may need to increase for MCHIP
    ctx->gfx_size = SCHIP_XRES * SCHIP_YRES;
    ctx->gfx = (uint8_t *)low_calloc(ctx->gfx_size);

    // copy the font rom into th]is context's memory
    memcpy((void *)ctx->rom, lfont_rom, LFONT_SIZE);

#ifdef HAVE_SCHIP_SUPPORT
    memcpy((void *)(ctx->rom + LFONT_SIZE), hfont_rom, HFONT_SIZE);
#endif

    // use system clock to set the random seed (for rand instruction)
    srand((unsigned int)time(NULL));

#ifdef HAVE_PTR_INTERPRETER
    init_dispatch_tables();
#endif

    *pctx = ctx;
}

// -----------------------------------------------------------------------------
void c8_destroy_context(c8_context_t *ctx)
{
    low_free(ctx->gfx);
    low_free(ctx->rom);
    low_free(ctx);
}

// -----------------------------------------------------------------------------
int c8_load_file(c8_context_t *ctx, const char *path)
{
    FILE *fp;
    size_t length, bytes_read;

    assert(NULL != ctx);
    assert(NULL != path);

    // attempt to open the specified rom path
    if (NULL == (fp = fopen(path, "rb")))
        return -1;

    // determine the file length
    fseek(fp, 0, SEEK_END);
    length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    log_info("Loading %ld byte rom \"%s\"...\n", length, path);
    if (length > ctx->rom_size) {
#ifdef HAVE_MCHIP_SUPPORT
        // MegaChip programs can use 24-bit addressing with I
        log_info("Exceeded standard ROM size, assuming MegaChip.\n");
        ctx->rom_size = length + 0x200;
        ctx->rom = (uint8_t *)low_realloc(ctx->rom, ctx->rom_size);
#else
        // if not MegaChip, there should be no reason for a ROM of this size
        log_err("ROM size exceeds size of program address space.\n");
        fclose(fp);
        return -1;
#endif
    }

    bytes_read = fread((char *)(ctx->rom + 0x200), 1, length, fp);
    fclose(fp);

    return (length == bytes_read) ? 0 : -1;
}

// -----------------------------------------------------------------------------
void c8_set_system(c8_context_t *ctx, int system)
{
    switch (system)
    {
    default:
        assert(!"invalid system specified in c8_set_system");
        // fall through for release builds
    case SYSTEM_CHIP8:
        log_dbg("Setting system to CHIP8.\n");
        ctx->system = SYSTEM_CHIP8;
        ctx->fn.set_mode(ctx, SYSTEM_CHIP8, CHIP8_XRES, CHIP8_YRES);
        break;
#ifdef HAVE_HCHIP_SUPPORT
    case SYSTEM_HCHIP:
        log_dbg("Setting system to HCHIP.\n");
        ctx->system = SYSTEM_HCHIP;
        ctx->fn.set_mode(ctx, SYSTEM_HCHIP, HCHIP_XRES, HCHIP_YRES);
        break;
#endif
#ifdef HAVE_SCHIP_SUPPORT
    case SYSTEM_SCHIP:
        log_dbg("Setting system to SCHIP.\n");
        ctx->system = SYSTEM_SCHIP;
        ctx->fn.set_mode(ctx, SYSTEM_SCHIP, SCHIP_XRES, SCHIP_YRES);
        break;
#endif
#ifdef HAVE_MCHIP_SUPPORT
    case SYSTEM_MCHIP:
        log_dbg("Setting system to MCHIP.\n");
        ctx->system = SYSTEM_MCHIP;
        if (ctx->gfx_size < MCHIP_XRES * MCHIP_YRES * 4) {
            log_dbg("Allocating new framebuffer for MegaChip mode.\n");
            ctx->gfx_size = MCHIP_XRES * MCHIP_YRES * 4;
            ctx->gfx = (uint8_t *)low_realloc(ctx->gfx, ctx->gfx_size);
        }
        ctx->fn.set_mode(ctx, SYSTEM_MCHIP, MCHIP_XRES, MCHIP_YRES);
        break;
#endif
    }
}

// -----------------------------------------------------------------------------
void c8_set_handlers(c8_context_t *ctx, c8_handlers_t *fn, void *data)
{
    assert(NULL != fn);
    assert(NULL != data);

    ctx->fn = *fn;
    ctx->userdata = data;
}

// -----------------------------------------------------------------------------
void c8_set_debugger_enabled(c8_context_t *ctx, int enable)
{
    assert(NULL != ctx);

    if (enable)
        ctx->exec_flags |= EXEC_DEBUG;
    else
        ctx->exec_flags &= ~EXEC_DEBUG;
}

// -----------------------------------------------------------------------------
void c8_set_key_state(c8_context_t *ctx, unsigned int index, int state)
{
    assert(NULL != ctx);
    assert(index < 16);

    log_spew("keypad[%d] = %d\n", index, state);
    ctx->keypad[index] = state;
}

// -----------------------------------------------------------------------------
long c8_execute_cycles(c8_context_t *ctx, long cycles)
{
    assert(NULL != ctx);

    switch (ctx->mode) {
    default:
        assert(!"invalid mode specified in c8_execute_cycles");
        // fall through for release builds
#ifdef HAVE_CASE_INTERPRETER
    case MODE_CASE:
        return c8_execute_cycles_case(ctx, cycles);
#endif
#ifdef HAVE_PTR_INTERPRETER
    case MODE_PTR:
        return c8_execute_cycles_ptr(ctx, cycles);
#endif
#ifdef HAVE_CACHE_INTERPRETER
    case MODE_CACHE:
        return c8_execute_cycles_cache(ctx, cycles);
#endif
#ifdef HAVE_RECOMPILER
    case MODE_DBT:
        return c8_execute_cycles_dbt(ctx, cycles);
#endif
    }
    return -1;
}

// -----------------------------------------------------------------------------
void c8_update_counters(c8_context_t *ctx, int delta)
{
    assert(NULL != ctx);
    assert(NULL != ctx->fn.snd_ctrl);

    // update the delay counter
    if (delta >= ctx->dt) ctx->dt = 0;
    else ctx->dt -= delta;

    // update the sound counter
    if (delta >= ctx->st) {
        if (ctx->sound_on) {
            // on -> off
            ctx->sound_on = 0;
            ctx->fn.snd_ctrl(ctx->userdata, 0);
        }
        ctx->st = 0;
    }
    else {
        if (!ctx->sound_on) {
            // off -> on
            ctx->sound_on = 1;
            ctx->fn.snd_ctrl(ctx->userdata, 1);
        }
        ctx->st -= delta;
    }
}

// -----------------------------------------------------------------------------
// Draw an 8xN or 8x16 sprite in CHIP8 mode.
int gfx_draw_chip8_sprite(c8_context_t *ctx, int x, int y, int n)
{
    int j, b, collision = 0, i = ctx->i;

    // handle the special case: when N==0, draw 8x16 sprite
    if (!n) n = 16;

    for (j = 0; j < n; ++j) {
        uint8_t data = ctx->rom[i++];
        int y_pos = ((y + j) & 0x1F) << 8;
        for (b = 0; b < 8; ++b) {
            int offset = y_pos + (((x + b) & 0x3F) << 1);
            assert(offset < ctx->gfx_size);
            if (data & 0x80) {
                ctx->gfx[offset] ^= 1;
                ctx->gfx[offset + 1] ^= 1;
                ctx->gfx[offset + SCHIP_XRES] ^= 1;
                ctx->gfx[offset + SCHIP_XRES + 1] ^= 1;
                if (!ctx->gfx[offset]) collision = 1;
            }
            data <<= 1;
        }
    }
    return collision;
}

#ifdef HAVE_HCHIP_SUPPORT
// -----------------------------------------------------------------------------
// Draw an 8xN or 8x16 sprite in HCHIP (HiRes) mode.
int gfx_draw_hchip_sprite(c8_context_t *ctx, int x, int y, int n)
{
    int j, b, collision = 0, i = ctx->i;

    // handle the special case: when N==0, draw 8x16 sprite
    if (!n) n = 16;

    for (j = 0; j < n; ++j) {
        uint8_t data = ctx->rom[i++];
        int y_pos = ((y + j) & 0x3F) << 7;
        for (b = 0; b < 8; ++b) {
            int offset = y_pos + (((x + b) & 0x3F) << 1);
            assert(offset < ctx->gfx_size);
            if (data & 0x80) {
                ctx->gfx[offset] ^= 1;
                ctx->gfx[offset + 1] ^= 1;
                if (!ctx->gfx[offset]) collision = 1;
            }
            data <<= 1;
        }
    }
    return collision;
}
#endif // HAVE_HCHIP_SUPPORT

#ifdef HAVE_SCHIP_SUPPORT
// -----------------------------------------------------------------------------
// Draw an 8xN or 16x16 sprite in SCHIP (SuperChip) mode.
int gfx_draw_schip_sprite(c8_context_t *ctx, int x, int y, int n)
{
    int j, b, collision = 0, i = ctx->i;

    if (n > 0) {
        for (j = 0; j < n; ++j) {
            int y_pos = ((y + j) & 0x3F) << 7;
            uint8_t data = ctx->rom[i++];
            for (b = 0; b < 8; ++b) {
                int offset = y_pos + ((x + b) & 0x7F);
                assert(offset < ctx->gfx_size);
                if (data & 0x80) {
                    ctx->gfx[offset] ^= 1;
                    if (!ctx->gfx[offset]) collision = 1;
                }
                data <<= 1;
            }
        }
    }
    else {
        for (j = 0; j < 16; ++j) {
            int y_pos = ((y + j) & 0x3F) << 7;
            uint16_t data = (ctx->rom[i] << 8) | ctx->rom[i + 1];
            i += 2;
            for (b = 0; b < 16; ++b) {
                int offset = y_pos + ((x + b) & 0x7F);
                assert(offset < ctx->gfx_size);
                if (data & 0x8000) {
                    ctx->gfx[offset] ^= 1;
                    if (!ctx->gfx[offset]) collision = 1;
                }
                data <<= 1;
            }
        }
    }
    return collision;
}

// -----------------------------------------------------------------------------
// scroll the framebuffer down N lines
void gfx_scroll_down(c8_context_t *ctx, int lines)
{
    const int w = SCHIP_XRES, h = SCHIP_YRES;
    int i;

    for (i = h - 1; i >= lines; --i) {
        memcpy(&ctx->gfx[i * w], &ctx->gfx[(i - lines) * w], w);
    }
    memset(&ctx->gfx[0], 0, lines * w);
    ctx->dirty = 1;
}

// -----------------------------------------------------------------------------
// scroll each line to the right by four pixels
void gfx_scroll_right(c8_context_t *ctx)
{
    const int w = SCHIP_XRES, h = SCHIP_YRES;
    int y;

    for (y = 0; y < h; ++y) {
        uint8_t *line = &ctx->gfx[y * w];
        memmove(&line[4], &line[0], w - 4);
        memset(&line[0], 0, 4);
    }
    ctx->dirty = 1;
}

// -----------------------------------------------------------------------------
// scroll each line to the left by four pixels
void gfx_scroll_left(c8_context_t *ctx)
{
    const int w = SCHIP_XRES, h = SCHIP_YRES;
    int y;

    for (y = 0; y < h; ++y) {
        uint8_t *line = &ctx->gfx[y * w];
        memmove(&line[0], &line[4], w - 4);
        memset(&line[w - 4], 0, 4);
    }
    ctx->dirty = 1;
}
#endif // HAVE_SCHIP_SUPPORT

#ifdef HAVE_MCHIP_SUPPORT
// -----------------------------------------------------------------------------
// Draw an SprWidth x SprHeight sprite in MCHIP (MegaChip) mode.
int gfx_draw_mchip_sprite(c8_context_t *ctx, int x, int y, int n)
{
    int j, b, collision = 0, i = ctx->i;
    uint32_t *gfx = (uint32_t *)ctx->gfx;

    for (j = 0; j < ctx->spr_height; ++j) {
        int y_pos = ((y + j) % MCHIP_YRES) * MCHIP_XRES;
        for (b = 0; b < ctx->spr_width; ++b) {
            int cindex = ctx->rom[i++];
            if (cindex) {
                int offset = y_pos + ((x + b) % MCHIP_XRES);
                if (gfx[offset] > 0) collision = 1;
                gfx[offset] = ctx->palette[cindex];
            }
        }
    }
    return collision;
}

// -----------------------------------------------------------------------------
// scroll the framebuffer up N lines
void gfx_scroll_up(c8_context_t *ctx, int lines)
{
    const int w = SCHIP_XRES, h = SCHIP_YRES;
    int i;

    for (i = 0; i < (h - lines); ++i) {
        memcpy(&ctx->gfx[i * w], &ctx->gfx[(i + lines) * w], w);
    }
    memset(&ctx->gfx[(h - lines) * w], 0, lines * w);
    ctx->dirty = 1;
}
#endif // HAVE_MCHIP_SUPPORT

// -----------------------------------------------------------------------------
int gfx_draw_sprite(c8_context_t *ctx, int rx, int ry, int n)
{
    int collision, x = ctx->v[rx], y = ctx->v[ry];

    switch (ctx->system) {
    default:
        assert(!"invalid system specified in gfx_draw_sprite");
        // fall through for release builds
    case SYSTEM_CHIP8:
        collision = gfx_draw_chip8_sprite(ctx, x, y, n);
        break;
#ifdef HAVE_HCHIP_SUPPORT
    case SYSTEM_HCHIP:
        collision = gfx_draw_hchip_sprite(ctx, x, y, n);
        break;
#endif
#ifdef HAVE_SCHIP_SUPPORT
    case SYSTEM_SCHIP:
        collision = gfx_draw_schip_sprite(ctx, x, y, n);
        break;
#endif
#ifdef HAVE_MCHIP_SUPPORT
    case SYSTEM_MCHIP:
        collision = gfx_draw_mchip_sprite(ctx, x, y, n);
        break;
#endif
    }
    ctx->dirty = 1;
    return collision;
}

