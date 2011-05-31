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
#include "chip8.h"
#include "xlat.h"

#ifdef PLATFORM_WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif // PLATFORM_WIN32

#define O_X ((xs->opcode >> 8) & 0xF)
#define O_Y ((xs->opcode >> 4) & 0xF)
#define O_N (xs->opcode & 0xF)
#define O_B (xs->opcode & 0xFF)
#define O_T (xs->opcode & 0xFFF)

#define R_SP 16
#define R_PC 17
#define R_I  18
#define R_DT 19
#define R_ST 20

void temp_clear_screen(c8_context_t *ctx)
{
    log_spew("temp_clear_screen(%p)\n", ctx);
    memset(ctx->gfx, 0, ctx->gfx_size);
    ctx->dirty = 1;
}

// -----------------------------------------------------------------------------
void *low_malloc(size_t length)
{
#if ARCH_X86_64
    return mmap(NULL, length, PROT_READ | PROT_WRITE,
                MAP_ANONYMOUS | MAP_PRIVATE | MAP_32BIT, 0, 0);
#else
    return malloc(length);
#endif // ARCH_X86_64
}

// -----------------------------------------------------------------------------
void *low_calloc(size_t length)
{
#if ARCH_X86_64
    void *p = mmap(NULL, length, PROT_READ | PROT_WRITE,
                   MAP_ANONYMOUS | MAP_PRIVATE | MAP_32BIT, 0, 0);
    memset(p, 0, length);
    return p;
#else
    return calloc(1, length);
#endif // ARCH_X86_64
}

// -----------------------------------------------------------------------------
void *low_realloc(void *p, size_t length)
{
#if ARCH_X86_64
    low_free(p);
    return low_calloc(length);
#else
    return realloc(p, length);
#endif
}

// -----------------------------------------------------------------------------
void low_free(void *p)
{
#ifdef ARCH_X86_64
#else
    free(p);
#endif // ARCH_X86_64
}

// -----------------------------------------------------------------------------
// Allocate an executable translation block of the specified length.
int xlat_alloc_block(xlat_block_t *xb, long length)
{
#ifdef PLATFORM_WIN32
    void *p = VirtualAlloc(NULL, length, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
#else
    void *p = mmap(NULL, length, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_ANONYMOUS | MAP_PRIVATE | MAP_32BIT, 0, 0);
#endif // PLATFORM_WIN32

    if (!p) {
        log_err("failed to allocate executable xlat block\n");
        return -1;
    }

    xb->block = (uint8_t *)p;
    xb->ptr = (uint8_t *)p;
    xb->length = length;
    xb->num_cycles = 0;
    xb->visits = 0;
    return 0;
}

// -----------------------------------------------------------------------------
// Free and clear the allocated translation block.
void xlat_free_block(xlat_block_t *xb)
{
#ifdef PLATFORM_WIN32
    VirtualFree(xb->block, 0, MEM_RELEASE);
#else
    munmap(xb->block, xb->length);
#endif // PLATFORM_WIN32

    memset(xb, 0, sizeof(xlat_block_t));
}

// -----------------------------------------------------------------------------
static int xlat_sys_cls(xlat_state_t *xs)
{
    xlat_emit_call_1(xs, (void *)temp_clear_screen, (size_t)xs->ctx);
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_sys_ret(xlat_state_t *xs)
{
    int rsp = xlat_reserve_register(xs, 16, R_SP, &xs->ctx->sp);
    int rpc = xlat_reserve_register(xs, 16, R_PC, &xs->ctx->pc);
    int tmp = xlat_reserve_register_index(xs, 32, 0);
    xlat_emit_add_i32r64(xs->xb, -1, rsp);
    xlat_emit_mov_i64r64(xs->xb, (uint64_t)xs->ctx->stack, tmp);
    xlat_emit_mov_rmr16_scale(xs->xb, rpc, tmp, rsp, 2);
    return 1;
}

// -----------------------------------------------------------------------------
static int xlat_bad(xlat_state_t *xs)
{
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_jmp(xlat_state_t *xs)
{
    int rpc = xlat_reserve_register_wo(xs, 16, R_PC, &xs->ctx->pc);
    xlat_emit_mov_i16r16(xs->xb, O_T, rpc);
    return 1;
}

// -----------------------------------------------------------------------------
static int xlat_jsr(xlat_state_t *xs)
{
    int rsp = xlat_reserve_register(xs, 16, R_SP, &xs->ctx->sp);
    int rpc = xlat_reserve_register(xs, 16, R_PC, &xs->ctx->pc);
    int tmp = xlat_reserve_register_index(xs, 32, 0);
    xlat_emit_mov_i64r64(xs->xb, (uint64_t)xs->ctx->stack, tmp);
    xlat_emit_mov_r16rm_scale(xs->xb, tmp, rsp, 2, rpc);
    xlat_emit_add_i32r64(xs->xb, 1, rsp);
    xlat_emit_mov_i16r16(xs->xb, O_T, rpc);
    return 1;
}

// -----------------------------------------------------------------------------
static int xlat_sei(xlat_state_t *xs)
{
    int rx = xlat_reserve_register(xs, 8, O_X, &xs->ctx->v[O_X]);
    int rpc = xlat_reserve_register(xs, 16, R_PC, &xs->ctx->pc);
    int tmp = xlat_reserve_register_index(xs, 16, 0);
    xlat_emit_mov_i16r16(xs->xb, xs->ctx->pc + 2, tmp);
    xlat_emit_cmp_i8r8(xs->xb, O_B, rx);
    xlat_emit_cmove_r16r16(xs->xb, tmp, rpc);
    return 1; // TODO: do we have to terminate the basic block?
}

// -----------------------------------------------------------------------------
static int xlat_sni(xlat_state_t *xs)
{
    int rx = xlat_reserve_register(xs, 8, O_X, &xs->ctx->v[O_X]);
    int rpc = xlat_reserve_register(xs, 16, R_PC, &xs->ctx->pc);
    int tmp = xlat_reserve_register_index(xs, 16, 0);
    xlat_emit_mov_i16r16(xs->xb, xs->ctx->pc + 2, tmp);
    xlat_emit_cmp_i8r8(xs->xb, O_B, rx);
    xlat_emit_cmovne_r16r16(xs->xb, tmp, rpc);
    return 1; // TODO: do we have to terminate the basic block?
}

// -----------------------------------------------------------------------------
static int xlat_ser(xlat_state_t *xs)
{
    int rx = xlat_reserve_register(xs, 8, O_X, &xs->ctx->v[O_X]);
    int ry = xlat_reserve_register(xs, 8, O_Y, &xs->ctx->v[O_Y]);
    int rpc = xlat_reserve_register(xs, 16, R_PC, &xs->ctx->pc);
    int tmp = xlat_reserve_register_index(xs, 16, 0);
    xlat_emit_mov_i16r16(xs->xb, xs->ctx->pc + 2, tmp);
    xlat_emit_cmp_r8r8(xs->xb, ry, rx);
    xlat_emit_cmove_r16r16(xs->xb, tmp, rpc);
    return 1;
}

// -----------------------------------------------------------------------------
static int xlat_mov(xlat_state_t *xs)
{
    int rx = xlat_reserve_register_wo(xs, 8, O_X, &xs->ctx->v[O_X]);
    xlat_emit_mov_i8r8(xs->xb, O_B, rx);
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_add(xlat_state_t *xs)
{
    int rx = xlat_reserve_register(xs, 8, O_X, &xs->ctx->v[O_X]);
    xlat_emit_add_i8r8(xs->xb, O_B, rx);
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_reg_mov(xlat_state_t *xs)
{
    int rx = xlat_reserve_register(xs, 8, O_X, &xs->ctx->v[O_X]);
    int ry = xlat_reserve_register(xs, 8, O_Y, &xs->ctx->v[O_Y]);
    xlat_emit_mov_r8r8(xs->xb, ry, rx);
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_reg_orl(xlat_state_t *xs)
{
    int rx = xlat_reserve_register(xs, 8, O_X, &xs->ctx->v[O_X]);
    int ry = xlat_reserve_register(xs, 8, O_Y, &xs->ctx->v[O_Y]);
    xlat_emit_or_r8r8(xs->xb, ry, rx);
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_reg_and(xlat_state_t *xs)
{
    int rx = xlat_reserve_register(xs, 8, O_X, &xs->ctx->v[O_X]);
    int ry = xlat_reserve_register(xs, 8, O_Y, &xs->ctx->v[O_Y]);
    xlat_emit_and_r8r8(xs->xb, ry, rx);
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_reg_xor(xlat_state_t *xs)
{
    int rx = xlat_reserve_register(xs, 8, O_X, &xs->ctx->v[O_X]);
    int ry = xlat_reserve_register(xs, 8, O_Y, &xs->ctx->v[O_Y]);
    xlat_emit_xor_r8r8(xs->xb, ry, rx);
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_reg_add(xlat_state_t *xs)
{
    int rx = xlat_reserve_register(xs, 8, O_X, &xs->ctx->v[O_X]);
    int ry = xlat_reserve_register(xs, 8, O_Y, &xs->ctx->v[O_Y]);
    xlat_emit_mov_r8r8(xs->xb, ry, rx);
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_reg_sxy(xlat_state_t *xs)
{
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_reg_shr(xlat_state_t *xs)
{
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_reg_syx(xlat_state_t *xs)
{
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_reg_shl(xlat_state_t *xs)
{
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_snr(xlat_state_t *xs)
{
    int rx = xlat_reserve_register(xs, 8, O_X, &xs->ctx->v[O_X]);
    int ry = xlat_reserve_register(xs, 8, O_Y, &xs->ctx->v[O_Y]);
    int rpc = xlat_reserve_register(xs, 16, R_PC, &xs->ctx->pc);
    int tmp = xlat_reserve_register_index(xs, 16, 0);
    xlat_emit_mov_i16r16(xs->xb, xs->ctx->pc + 2, tmp);
    xlat_emit_cmp_r8r8(xs->xb, ry, rx);
    xlat_emit_cmovne_r16r16(xs->xb, tmp, rpc);
    return 1;
}

// -----------------------------------------------------------------------------
static int xlat_ldi(xlat_state_t *xs)
{
    int ri = xlat_reserve_register(xs, 16, R_I, &xs->ctx->i);
    xlat_emit_mov_i16r16(xs->xb, O_T, ri);
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_vjp(xlat_state_t *xs)
{
    return 1;
}

// -----------------------------------------------------------------------------
static int xlat_rnd(xlat_state_t *xs)
{
    int r0 = xlat_reserve_register_index(xs, 32, 0);
    int rx = xlat_reserve_register(xs, 8, O_X, &xs->ctx->v[O_X]);
    xlat_emit_call_0(xs, (void *)rand);
    xlat_emit_and_i8r8(xs->xb, O_B, r0);
    xlat_emit_mov_r8r8(xs->xb, r0, rx);
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_drw(xlat_state_t *xs)
{
    int rvf = xlat_reserve_register(xs, 8, 0xF, &xs->ctx->v[0xF]);
    xlat_commit_register(xs, 8, O_X);
    xlat_commit_register(xs, 8, O_Y);
    xlat_commit_register(xs, 16, R_I);
    xlat_emit_call_4(xs, (void *)gfx_draw_sprite, (size_t)xs->ctx, O_X, O_Y, O_N);
    xlat_emit_mov_r8r8(xs->xb, 0, rvf);
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_key_seq(xlat_state_t *xs)
{
    // XXX: implement this for real
    int rpc = xlat_reserve_register_wo(xs, 16, R_PC, &xs->ctx->pc);
    xlat_emit_mov_i16r16(xs->xb, xs->ctx->pc, rpc);
    return 1;
}

// -----------------------------------------------------------------------------
static int xlat_key_sne(xlat_state_t *xs)
{
    // XXX: implement this for real
    int rpc = xlat_reserve_register_wo(xs, 16, R_PC, &xs->ctx->pc);
    xlat_emit_mov_i16r16(xs->xb, xs->ctx->pc + 2, rpc);
    return 1;
}

// -----------------------------------------------------------------------------
static int xlat_mem_rdd(xlat_state_t *xs)
{
    int rx = xlat_reserve_register(xs, 8, O_X, &xs->ctx->v[O_X]);
    int rdt = xlat_reserve_register(xs, 8, R_DT, &xs->ctx->dt);
    xlat_emit_mov_r8r8(xs->xb, rdt, rx);
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_mem_rdk(xlat_state_t *xs)
{
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_mem_wrd(xlat_state_t *xs)
{
    int rx = xlat_reserve_register(xs, 8, O_X, &xs->ctx->v[O_X]);
    int rdt = xlat_reserve_register(xs, 8, R_DT, &xs->ctx->dt);
    xlat_emit_mov_r8r8(xs->xb, rx, rdt);
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_mem_wrs(xlat_state_t *xs)
{
    int rx = xlat_reserve_register(xs, 8, O_X, &xs->ctx->v[O_X]);
    int rst = xlat_reserve_register(xs, 8, R_ST, &xs->ctx->st);
    xlat_emit_mov_r8r8(xs->xb, rx, rst);
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_mem_addi(xlat_state_t *xs)
{
    int rx = xlat_reserve_register(xs, 8, O_X, &xs->ctx->v[O_X]);
    int ri = xlat_reserve_register(xs, 16, R_I, &xs->ctx->i);
    xlat_emit_add_r16r16(xs->xb, rx, ri);
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_mem_font(xlat_state_t *xs)
{
    int rx = xlat_reserve_register(xs, 8, O_X, &xs->ctx->v[O_X]);
    int ri = xlat_reserve_register(xs, 8, R_I, &xs->ctx->i);
    int r0 = xlat_reserve_register_index(xs, 32, 0);
    xlat_emit_mov_i8r8(xs->xb, 5, r0);
    xlat_emit_mul_r8(xs->xb, rx);
    xlat_emit_mov_r8r8(xs->xb, r0, ri);
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_mem_bcd(xlat_state_t *xs)
{
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_mem_wr(xlat_state_t *xs)
{
    int r0 = xlat_reserve_register_index(xs, 32, 0);
    int ri = xlat_reserve_register(xs, 16, R_I, &xs->ctx->i);
    xlat_emit_mov_i64r64(xs->xb, (uint64_t)xs->ctx->rom, r0);
    xlat_emit_add_r64r64(xs->xb, ri, r0);

    int x, end = O_X;
    for (x = 0; x <= end; ++x) {
        int rx = xlat_reserve_register(xs, 8, x, &xs->ctx->v[x]); // RO?
        xlat_emit_mov_r8rm(xs->xb, rx, r0);
        xlat_emit_add_i32r64(xs->xb, 1, ri);
    }
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_mem_rd(xlat_state_t *xs)
{
    int r0 = xlat_reserve_register_index(xs, 32, 0);
    int ri = xlat_reserve_register(xs, 16, R_I, &xs->ctx->i);
    xlat_emit_mov_i64r64(xs->xb, (uint64_t)xs->ctx->rom, r0);
    xlat_emit_add_r64r64(xs->xb, ri, r0);

    int x, end = O_X;
    for (x = 0; x <= end; ++x) {
        int rx = xlat_reserve_register(xs, 8, x, &xs->ctx->v[x]); // RO?
        xlat_emit_mov_rmr8(xs->xb, r0, rx);
        xlat_emit_add_i32r64(xs->xb, 1, ri);
    }
    return 0;
}

#ifdef HAVE_SCHIP_SUPPORT
// -----------------------------------------------------------------------------
static int xlat_sup_brk(xlat_state_t *xs)
{
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_sup_scd(xlat_state_t *xs)
{
    xlat_emit_call_2(xs, (void *)gfx_scroll_down, (size_t)xs->ctx, O_N);
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_sup_scr(xlat_state_t *xs)
{
    xlat_emit_call_1(xs, (void *)gfx_scroll_right, (size_t)xs->ctx);
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_sup_scl(xlat_state_t *xs)
{
    xlat_emit_call_1(xs, (void *)gfx_scroll_left, (size_t)xs->ctx);
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_sup_ch8(xlat_state_t *xs)
{
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_sup_sch(xlat_state_t *xs)
{
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_sup_xfont(xlat_state_t *xs)
{
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_sup_wr48(xlat_state_t *xs)
{
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_sup_rd48(xlat_state_t *xs)
{
    return 0;
}
#endif // HAVE_SCHIP_SUPPORT

#ifdef HAVE_MCHIP_SUPPORT
// -----------------------------------------------------------------------------
static int xlat_meg_off(xlat_state_t *xa)
{
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_meg_on(xlat_state_t *xa)
{
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_meg_scru(xlat_state_t *xa)
{
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_meg_ldhi(xlat_state_t *xa)
{
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_meg_ldpal(xlat_state_t *xa)
{
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_meg_sprw(xlat_state_t *xa)
{
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_meg_sprh(xlat_state_t *xa)
{
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_meg_alpha(xlat_state_t *xa)
{
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_meg_sndon(xlat_state_t *xa)
{
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_meg_sndoff(xlat_state_t *xa)
{
    return 0;
}

// -----------------------------------------------------------------------------
static int xlat_meg_bmode(xlat_state_t *xa)
{
    return 0;
}
#endif // HAVE_MCHIP_SUPPORT

// -----------------------------------------------------------------------------
static void translate_block(c8_context_t *ctx, xlat_block_t *xb)
{
    int block_finished = 0;

    xlat_state_t xs;
    xs.ctx = ctx;
    xs.xb = xb;
    xlat_alloc_state(&xs);

    if (0 > xlat_alloc_block(xb, 4096)) {
        log_err("failed to allocate xlat block @PC=%04X", xs.ctx->pc);
        return;
    }

    // block initialization code synchronizes target and host registers
    xlat_emit_prologue(&xs);

    while (!block_finished) {
        // fetch the next instruction opcode, trap for debugger if necessary
        int pc = xs.ctx->pc;
        ctx->opcode = xs.opcode = (ctx->rom[pc] << 8) | ctx->rom[pc + 1];
        xs.ctx->pc = (pc + 2) & (ROM_SIZE - 1);
        xb->num_cycles++;

        // translate the current instruction, terminating if branch encountered
#       define OPCODE xs.opcode
#       define OP(x) block_finished = xlat_##x(&xs)
#       include "decode.inc"
    }

    // block cleanup code commits target registers to emulator context
    xlat_emit_epilogue(&xs);
    xlat_free_state(&xs);
}

// -----------------------------------------------------------------------------
long c8_execute_cycles_dbt(c8_context_t *ctx, long cycles)
{
    xlat_block_t blocks[ROM_SIZE], *pblock;
    memset(blocks, 0, sizeof(xlat_block_t) * ROM_SIZE);

    long start_cycles = ctx->cycles;
    while (cycles > 0) {
        // fetch the block for this instruction, translating when necessary
        pblock = &blocks[ctx->pc];
        if (NULL == pblock->ptr) {
            // new code segment. translate and cache the next block
            translate_block(ctx, pblock);
        }

        // execute the translated instruction sequence
        // long bytes = (long)((int64_t)pblock->ptr - (int64_t)pblock->block);
        // fprintf(stderr,
        //         ">>> exec %ld instructions @ %p (%ld bytes, pc = %02X)\n",
        //         pblock->num_cycles, pblock->ptr, bytes, ctx->pc);

        ((xlat_fn)pblock->block)();
        ++pblock->visits;

        if (ctx->exec_flags && c8_debug_instruction(ctx, ctx->pc))
            break;

        cycles -= pblock->num_cycles;
        ctx->cycles += pblock->num_cycles;
    }

    return ctx->cycles - start_cycles;
}

