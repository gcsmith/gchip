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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "chip8.h"

// -----------------------------------------------------------------------------
void check_for_hires(c8_context_t *ctx)
{
    int opcode = (ctx->rom[ctx->pc] << 8) | ctx->rom[ctx->pc + 1];
    if (ctx->pc == 0x200 && opcode == 0x1260) {
#ifdef HAVE_HCHIP_SUPPORT
        c8_set_system(ctx, SYSTEM_HCHIP);
        ctx->rom[ctx->pc + 1] = 0xC0;
#else
        log_info("HIRES game detected, but mode is not supported.\n");
#endif
    }
}

// -----------------------------------------------------------------------------
static void FASTCALL op_sys_cls(c8_context_t *ctx)
{
    assert(NULL != ctx->fn.vid_sync);
    ctx->fn.vid_sync(ctx->userdata);
    memset(ctx->gfx, 0, ctx->gfx_size);
}

// -----------------------------------------------------------------------------
static void FASTCALL op_sys_ret(c8_context_t *ctx)
{
    if (ctx->sp == 0)
        ctx->sp = STACK_SIZE;
    ctx->pc = ctx->stack[--ctx->sp];
}

// -----------------------------------------------------------------------------
static void FASTCALL op_jmp(c8_context_t *ctx)
{
    ctx->pc = OP_T;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_jsr(c8_context_t *ctx)
{
    ctx->stack[ctx->sp] = ctx->pc;
    if (++ctx->sp >= STACK_SIZE)
        ctx->sp = 0;
    ctx->pc = OP_T;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_sei(c8_context_t *ctx)
{
    if (ctx->v[OP_X] == OP_B)
        ctx->pc += 2;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_sni(c8_context_t *ctx)
{
    if (ctx->v[OP_X] != OP_B)
        ctx->pc += 2;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_ser(c8_context_t *ctx)
{
    if (ctx->v[OP_X] == ctx->v[OP_Y])
        ctx->pc += 2;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_mov(c8_context_t *ctx)
{
    ctx->v[OP_X] = OP_B;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_add(c8_context_t *ctx)
{
    ctx->v[OP_X] = (ctx->v[OP_X] + OP_B) & 0xFF;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_reg_mov(c8_context_t *ctx)
{
    ctx->v[OP_X] = ctx->v[OP_Y];
}

// -----------------------------------------------------------------------------
static void FASTCALL op_reg_orl(c8_context_t *ctx)
{
    ctx->v[OP_X] |= ctx->v[OP_Y];
}

// -----------------------------------------------------------------------------
static void FASTCALL op_reg_and(c8_context_t *ctx)
{
    ctx->v[OP_X] &= ctx->v[OP_Y];
}

// -----------------------------------------------------------------------------
static void FASTCALL op_reg_xor(c8_context_t *ctx)
{
    ctx->v[OP_X] ^= ctx->v[OP_Y];
}

// -----------------------------------------------------------------------------
static void FASTCALL op_reg_add(c8_context_t *ctx)
{
    int result = ctx->v[OP_X] + ctx->v[OP_Y];
    ctx->v[OP_X] = (result & 0xFF);
    ctx->v[0xF] = (result > 0xFF) ? 1 : 0;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_reg_sxy(c8_context_t *ctx)
{
    ctx->v[0xF] = ctx->v[OP_X] >= ctx->v[OP_Y];
    ctx->v[OP_X] = (ctx->v[OP_X] - ctx->v[OP_Y]) & 0xFF;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_reg_shr(c8_context_t *ctx)
{
    ctx->v[0xF] = ctx->v[OP_X] & 1;
    ctx->v[OP_X] >>= 1;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_reg_syx(c8_context_t *ctx)
{
    ctx->v[0xF] = ctx->v[OP_Y] >= ctx->v[OP_X];
    ctx->v[OP_X] = (ctx->v[OP_Y] - ctx->v[OP_X]) & 0xFF;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_reg_shl(c8_context_t *ctx)
{
    ctx->v[0xF] = (ctx->v[OP_X] >> 7) & 1;
    ctx->v[OP_X] = (ctx->v[OP_X] << 1) & 0xFF;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_snr(c8_context_t *ctx)
{
    if (ctx->v[OP_X] != ctx->v[OP_Y])
        ctx->pc += 2;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_ldi(c8_context_t *ctx)
{
    ctx->i = OP_T;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_vjp(c8_context_t *ctx)
{
    ctx->pc = (ctx->v[0] + OP_T) & 0xFFF;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_rnd(c8_context_t *ctx)
{
    ctx->v[OP_X] = rand() & OP_B;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_drw(c8_context_t *ctx)
{
    ctx->v[0xF] = gfx_draw_sprite(ctx, OP_X, OP_Y, OP_N);
}

// -----------------------------------------------------------------------------
static void FASTCALL op_key_seq(c8_context_t *ctx)
{
    if (ctx->keypad[ctx->v[OP_X]]) ctx->pc += 2;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_key_sne(c8_context_t *ctx)
{
    if (!ctx->keypad[ctx->v[OP_X]]) ctx->pc += 2;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_mem_rdd(c8_context_t *ctx)
{
    ctx->v[OP_X] = ctx->dt;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_mem_rdk(c8_context_t *ctx)
{
    assert(NULL != ctx->fn.key_wait);
    ctx->v[OP_X] = ctx->fn.key_wait(ctx->userdata);
}

// -----------------------------------------------------------------------------
static void FASTCALL op_mem_wrd(c8_context_t *ctx)
{
    ctx->dt = ctx->v[OP_X];
}

// -----------------------------------------------------------------------------
static void FASTCALL op_mem_wrs(c8_context_t *ctx)
{
    ctx->st = ctx->v[OP_X];
}

// -----------------------------------------------------------------------------
static void FASTCALL op_mem_addi(c8_context_t *ctx)
{
    int result = ctx->i + ctx->v[OP_X];
    ctx->i = (result & 0xFFFF);
    ctx->v[0xF] = (result > 0xFFF) ? 1 : 0;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_mem_font(c8_context_t *ctx)
{
    ctx->i = (ctx->v[OP_X] & 0xF) * 5;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_mem_bcd(c8_context_t *ctx)
{
    int value = ctx->v[OP_X];
    ctx->rom[ctx->i + 0] = value / 100;
    ctx->rom[ctx->i + 1] = (value % 100) / 10;
    ctx->rom[ctx->i + 2] = (value % 10);
}

// -----------------------------------------------------------------------------
static void FASTCALL op_mem_wr(c8_context_t *ctx)
{
    int offset, end = OP_X;
    for (offset = 0; offset <= end; ++offset)
        ctx->rom[ctx->i + offset] = ctx->v[offset];
}

// -----------------------------------------------------------------------------
static void FASTCALL op_mem_rd(c8_context_t *ctx)
{
    int offset, end = OP_X;
    for (offset = 0; offset <= end; ++offset)
        ctx->v[offset] = ctx->rom[ctx->i + offset];
}

// -----------------------------------------------------------------------------
static void FASTCALL op_bad(c8_context_t *ctx)
{
    log_err("invalid opcode %04X\n", ctx->opcode);
}

#ifdef HAVE_SCHIP_SUPPORT
// -----------------------------------------------------------------------------
static void FASTCALL op_sup_brk(c8_context_t *ctx)
{
    log_info("received exit signal. emulator terminating...\n");
    ctx->exec_flags |= EXEC_BREAK;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_sup_scd(c8_context_t *ctx)
{
    gfx_scroll_down(ctx, OP_N);
}

// -----------------------------------------------------------------------------
static void FASTCALL op_sup_scr(c8_context_t *ctx)
{
    gfx_scroll_right(ctx);
}

// -----------------------------------------------------------------------------
static void FASTCALL op_sup_scl(c8_context_t *ctx)
{
    gfx_scroll_left(ctx);
}

// -----------------------------------------------------------------------------
static void FASTCALL op_sup_ch8(c8_context_t *ctx)
{
    c8_set_system(ctx, SYSTEM_CHIP8);
}

// -----------------------------------------------------------------------------
static void FASTCALL op_sup_sch(c8_context_t *ctx)
{
    c8_set_system(ctx, SYSTEM_SCHIP);
}

// -----------------------------------------------------------------------------
static void FASTCALL op_sup_xfont(c8_context_t *ctx)
{
    ctx->i = LFONT_SIZE + (ctx->v[OP_X] & 0xF) * 10;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_sup_wr48(c8_context_t *ctx)
{
    int i, i_max = MIN(7, OP_X);
    for (i = 0; i <= i_max; ++i)
        ctx->hp[i] = ctx->v[i];
}

// -----------------------------------------------------------------------------
static void FASTCALL op_sup_rd48(c8_context_t *ctx)
{
    int i, i_max = MIN(7, OP_X);
    for (i = 0; i <= i_max; ++i)
        ctx->v[i] = ctx->hp[i];
}
#endif // HAVE_SCHIP_SUPPORT

#ifdef HAVE_MCHIP_SUPPORT
// -----------------------------------------------------------------------------
static void FASTCALL op_meg_off(c8_context_t *ctx)
{
    c8_set_system(ctx, SYSTEM_CHIP8);
}

// -----------------------------------------------------------------------------
static void FASTCALL op_meg_on(c8_context_t *ctx)
{
    c8_set_system(ctx, SYSTEM_MCHIP);
}

// -----------------------------------------------------------------------------
static void FASTCALL op_meg_scru(c8_context_t *ctx)
{
    gfx_scroll_up(ctx, OP_N);
}

// -----------------------------------------------------------------------------
static void FASTCALL op_meg_ldhi(c8_context_t *ctx)
{
    ctx->i = OP_24;
    ctx->pc += 2;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_meg_ldpal(c8_context_t *ctx)
{
#ifdef HAVE_HCHIP_SUPPORT
    // in chip-8 hires mode, this is a clear instruction
    if (ctx->system == SYSTEM_HCHIP) {
        memset(ctx->gfx, 0, ctx->gfx_size);
        return;
    }
#endif

#ifdef HAVE_MCHIP_SUPPORT
    // in megachip-8 mode, this is a load palette instruction
    int i, palette_size = OP_B, addr = ctx->i;
    for (i = 1; i <= palette_size; ++i) {
        uint32_t c = *(uint32_t *)&ctx->rom[addr];
        int b = (c >> 24) & 0xFF;
        int g = (c >> 16) & 0xFF;
        int r = (c >> 8) & 0xFF;
        int a = c & 0xFF;
        ctx->palette[i] = (a << 24) | (b << 16) | (g << 8) | r;
        addr += 4;
    }
#endif
}

// -----------------------------------------------------------------------------
static void FASTCALL op_meg_sprw(c8_context_t *ctx)
{
    ctx->spr_width = OP_B;
    if (!ctx->spr_width) ctx->spr_width = 256;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_meg_sprh(c8_context_t *ctx)
{
    ctx->spr_height = OP_B;
    if (!ctx->spr_height) ctx->spr_height = 256;
}

// -----------------------------------------------------------------------------
static void FASTCALL op_meg_alpha(c8_context_t *ctx)
{
    log_err("ALPHA not implemented\n");
    //exit(0);
}

// -----------------------------------------------------------------------------
static void FASTCALL op_meg_sndon(c8_context_t *ctx)
{
    log_err("DIGISND not implemented\n");
    //exit(0);
}

// -----------------------------------------------------------------------------
static void FASTCALL op_meg_sndoff(c8_context_t *ctx)
{
    log_err("STOPSND not implemented\n");
    //exit(0);
}

// -----------------------------------------------------------------------------
static void FASTCALL op_meg_bmode(c8_context_t *ctx)
{
    log_err("BMODE not implemented\n");
    //exit(0);
}
#endif // HAVE_MCHIP_SUPPORT

#ifdef HAVE_PTR_INTERPRETER
typedef void (FASTCALL *opcode_fn)(c8_context_t *);
extern opcode_fn opc_tab[0x10];
extern opcode_fn reg_tab[0x10];
extern opcode_fn sys_tab[0x100];
extern opcode_fn key_tab[0x100];
extern opcode_fn mem_tab[0x100];

// -----------------------------------------------------------------------------
static void FASTCALL op_sys(c8_context_t *ctx)
{
    sys_tab[ctx->opcode & 0xFF](ctx);
}

// -----------------------------------------------------------------------------
static void FASTCALL op_reg(c8_context_t *ctx)
{
    reg_tab[ctx->opcode & 0xF](ctx);
}

// -----------------------------------------------------------------------------
static void FASTCALL op_key(c8_context_t *ctx)
{
    key_tab[ctx->opcode & 0xFF](ctx);
}

// -----------------------------------------------------------------------------
static void FASTCALL op_mem(c8_context_t *ctx)
{
    mem_tab[ctx->opcode & 0xFF](ctx);
}

// -----------------------------------------------------------------------------
void init_dispatch_tables(void)
{
    static int first_time_init = 1;
    int i;

    // sys_tab and key_tab are global, so only perform initialization once
    if (!first_time_init)
        return;

    // populate the dispatch tables with opcode handlers
    for (i = 0; i < 0x100; i++)
        sys_tab[i] = key_tab[i] = mem_tab[i] = op_bad;

    sys_tab[0xE0] = op_sys_cls;
    sys_tab[0xEE] = op_sys_ret;
    key_tab[0x9E] = op_key_seq;
    key_tab[0xA1] = op_key_sne;
    mem_tab[0x07] = op_mem_rdd;
    mem_tab[0x0A] = op_mem_rdk;
    mem_tab[0x15] = op_mem_wrd;
    mem_tab[0x18] = op_mem_wrs;
    mem_tab[0x1E] = op_mem_addi;
    mem_tab[0x29] = op_mem_font;
    mem_tab[0x33] = op_mem_bcd;
    mem_tab[0x55] = op_mem_wr;
    mem_tab[0x65] = op_mem_rd;

#ifdef HAVE_SCHIP_SUPPORT
    for (i = 0; i < 0x10; ++i)
        sys_tab[0xC0 | i] = op_sup_scd;
    sys_tab[0xFB] = op_sup_scr;
    sys_tab[0xFC] = op_sup_scl;
    sys_tab[0xFD] = op_sup_brk;
    sys_tab[0xFE] = op_sup_ch8;
    sys_tab[0xFF] = op_sup_sch;
    mem_tab[0x30] = op_sup_xfont;
    mem_tab[0x75] = op_sup_wr48;
    mem_tab[0x85] = op_sup_rd48;
#endif

#ifdef HAVE_MCHIP_SUPPORT
    for (i = 0; i < 0x10; ++i)
        sys_tab[0xB0 | i] = op_meg_scru;
    sys_tab[0x10] = op_meg_off;
    sys_tab[0x11] = op_meg_on;
#endif
}

opcode_fn opc_tab[0x10] = {
    op_sys, op_jmp, op_jsr, op_sei, op_sni, op_ser, op_mov, op_add,
    op_reg, op_snr, op_ldi, op_vjp, op_rnd, op_drw, op_key, op_mem
};

opcode_fn reg_tab[0x10] = {
    op_reg_mov, op_reg_orl, op_reg_and, op_reg_xor, op_reg_add, op_reg_sxy,
    op_reg_shr, op_reg_syx, op_bad, op_bad, op_bad, op_bad, op_bad, op_bad,
    op_reg_shl, op_bad
};

opcode_fn sys_tab[0x100];
opcode_fn key_tab[0x100];
opcode_fn mem_tab[0x100];

// -----------------------------------------------------------------------------
long c8_execute_cycles_ptr(c8_context_t *ctx, long cycles)
{
    uint16_t pc;
    check_for_hires(ctx);
    while (0 != cycles--) {
        pc = ctx->pc;
        ctx->opcode = (ctx->rom[pc] << 8) | ctx->rom[pc + 1];
        ctx->pc = (pc + 2) & (ROM_SIZE - 1);

        if (ctx->exec_flags && c8_debug_instruction(ctx, pc)) break;
        opc_tab[ctx->opcode >> 12](ctx);
        ++ctx->cycles;
    }

    return 0;
}
#endif // HAVE_PTR_INTERPRETER

#ifdef HAVE_CASE_INTERPRETER
// -----------------------------------------------------------------------------
long c8_execute_cycles_case(c8_context_t *ctx, long cycles)
{
    uint16_t pc;
    check_for_hires(ctx);
    while (0 != cycles--) {
        pc = ctx->pc;
        ctx->opcode = (ctx->rom[pc] << 8) | ctx->rom[pc + 1];
        ctx->pc = (pc + 2) & (ROM_SIZE - 1);

        if (ctx->exec_flags && c8_debug_instruction(ctx, pc)) break;
#       define OPCODE ctx->opcode
#       define OP(x) op_##x(ctx)
#       include "decode.inc"
        ++ctx->cycles;
    }

    return 0;
}
#endif // HAVE_CASE_INTERPRETER

#ifdef HAVE_CACHE_INTERPRETER
// -----------------------------------------------------------------------------
long c8_execute_cycles_cache(c8_context_t *ctx, long cycles)
{
    opcode_fn cache[ROM_SIZE];
    uint16_t opcodes[ROM_SIZE], pc;
    int i;

    check_for_hires(ctx);
    for (i = 0; i < ROM_SIZE; ++i) {
        opcodes[i] = (ctx->rom[i] << 8) | ctx->rom[i + 1];
        switch (opcodes[i] >> 12) {
        case 0x0: cache[i] = sys_tab[opcodes[i] & 0xFF]; break;
        case 0x8: cache[i] = reg_tab[opcodes[i] & 0x0F]; break;
        case 0xE: cache[i] = key_tab[opcodes[i] & 0xFF]; break;
        case 0xF: cache[i] = mem_tab[opcodes[i] & 0xFF]; break;
        default:  cache[i] = opc_tab[opcodes[i] >>  12]; break;
        }
    }

    while (0 != cycles--) {
        pc = ctx->pc;
        ctx->opcode = opcodes[pc];
        ctx->pc = (pc + 2) & (ROM_SIZE - 1);

        if (ctx->exec_flags && c8_debug_instruction(ctx, pc)) break;
        cache[pc](ctx);
        ++ctx->cycles;
    }

    return 0;
}
#endif // HAVE_CACHE_INTERPRETER

