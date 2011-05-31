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
#include <time.h>
#include <assert.h>
#include "chip8.h"

#define dasm_sys_cls    snprintf(o, s, "cls")
#define dasm_sys_ret    snprintf(o, s, "ret")
#define dasm_bad        snprintf(o, s, "???")
#define dasm_jmp        snprintf(o, s, "jp %03X", OP_T)
#define dasm_jsr        snprintf(o, s, "call %03X", OP_T)
#define dasm_sei        snprintf(o, s, "se V%1X, %02X", OP_X, OP_B)
#define dasm_sni        snprintf(o, s, "sne V%1X, %02X", OP_X, OP_B)
#define dasm_ser        snprintf(o, s, "se V%1X, V%1X", OP_X, OP_Y)
#define dasm_mov        snprintf(o, s, "ld V%1X, %02X", OP_X, OP_B)
#define dasm_add        snprintf(o, s, "add V%1X, %02X", OP_X, OP_B)
#define dasm_reg_mov    snprintf(o, s, "ld V%1X, V%1X", OP_X, OP_Y)
#define dasm_reg_orl    snprintf(o, s, "or V%1X, V%1X", OP_X, OP_Y)
#define dasm_reg_and    snprintf(o, s, "and V%1X, V%1X", OP_X, OP_Y)
#define dasm_reg_xor    snprintf(o, s, "xor V%1X, V%1X", OP_X, OP_Y)
#define dasm_reg_add    snprintf(o, s, "add V%1X, V%1X", OP_X, OP_Y)
#define dasm_reg_sxy    snprintf(o, s, "sub V%1X, V%1X", OP_X, OP_Y)
#define dasm_reg_shr    snprintf(o, s, "shr V%1X", OP_X)
#define dasm_reg_syx    snprintf(o, s, "subn V%1X, V%1X", OP_Y, OP_X)
#define dasm_reg_shl    snprintf(o, s, "shl V%1X", OP_X)
#define dasm_snr        snprintf(o, s, "sne V%1X, V%1X", OP_X, OP_Y)
#define dasm_ldi        snprintf(o, s, "ld I, %03X", OP_T)
#define dasm_vjp        snprintf(o, s, "jp V0, %03X", OP_T)
#define dasm_rnd        snprintf(o, s, "rnd V%1X, %02X", OP_X, OP_B)
#define dasm_drw        snprintf(o, s, "drw V%1X, V%1X, %1X", OP_X, OP_Y, OP_N)
#define dasm_key_seq    snprintf(o, s, "skp V%1X", OP_X)
#define dasm_key_sne    snprintf(o, s, "sknp V%1X", OP_X)
#define dasm_mem_rdd    snprintf(o, s, "ld V%1X, DT", OP_X)
#define dasm_mem_rdk    snprintf(o, s, "ld V%1X, K", OP_X)
#define dasm_mem_wrd    snprintf(o, s, "ld DT, V%1X", OP_X)
#define dasm_mem_wrs    snprintf(o, s, "ld ST, V%1X", OP_X)
#define dasm_mem_addi   snprintf(o, s, "add I, V%1X", OP_X)
#define dasm_mem_font   snprintf(o, s, "ld I, LF[V%1X]", OP_X)
#define dasm_mem_bcd    snprintf(o, s, "ld [I], B[V%1X]", OP_X)
#define dasm_mem_wr     snprintf(o, s, "ld [I], V%1X", OP_X)
#define dasm_mem_rd     snprintf(o, s, "ld V%1X, [I]", OP_X)

#define dasm_sup_brk    snprintf(o, s, "exit")
#define dasm_sup_scd    snprintf(o, s, "scd %1X", OP_N)
#define dasm_sup_scr    snprintf(o, s, "scr")
#define dasm_sup_scl    snprintf(o, s, "scl")
#define dasm_sup_ch8    snprintf(o, s, "low")
#define dasm_sup_sch    snprintf(o, s, "high")
#define dasm_sup_xfont  snprintf(o, s, "ld I, HF[V%1X]", OP_X)
#define dasm_sup_wr48   snprintf(o, s, "ld R, V%1X", OP_X)
#define dasm_sup_rd48   snprintf(o, s, "ld V%1X, R", OP_X)

#define dasm_meg_off    snprintf(o, s, "megaoff")
#define dasm_meg_on     snprintf(o, s, "megaon")
#define dasm_meg_scru   snprintf(o, s, "scru %1X", OP_N)
#define dasm_meg_ldhi   snprintf(o, s, "ldhi I, %06X", OP_24)
#define dasm_meg_ldpal  snprintf(o, s, "ldpal %02X", OP_B)
#define dasm_meg_sprw   snprintf(o, s, "sprw %02X", OP_B)
#define dasm_meg_sprh   snprintf(o, s, "sprh %02X", OP_B)
#define dasm_meg_alpha  snprintf(o, s, "alpha %02X", OP_B)
#define dasm_meg_sndon  snprintf(o, s, "digisnd")
#define dasm_meg_sndoff snprintf(o, s, "stopsnd")
#define dasm_meg_bmode  snprintf(o, s, "bmode %1X", OP_N)

// -----------------------------------------------------------------------------
void c8_debug_disassemble(const c8_context_t *ctx, char *o, int s)
{
#   define OPCODE ctx->opcode
#   define OP(x) dasm_##x
#   include "decode.inc"
}

// -----------------------------------------------------------------------------
int c8_debug_instruction(const c8_context_t *ctx, uint16_t pc)
{
    char buffer[64];

    if (ctx->exec_flags & EXEC_DEBUG) {
        // print out the program counter, opcode, and disassembled instruction
        c8_debug_disassemble(ctx, buffer, 64);
        log_dbg("%03X  %04X  %-14s  ", pc, ctx->opcode, buffer);

        // print out a dump of the system registers
        for (int i = 0; i < 16; ++i)
            log_dbg("%02X ", ctx->v[i]);
        log_dbg("%04X %04X %02X %02X\n", ctx->sp, ctx->i, ctx->dt, ctx->st);
    }

    if (ctx->exec_flags & EXEC_SUBSET) {
        if (ctx->cycles >= ctx->max_cycles)
            return 1;
    }

    if (ctx->exec_flags & EXEC_BREAK)
        return 1;

    return 0;
}

// -----------------------------------------------------------------------------
int c8_debug_lockstep_test(const char *path)
{
    c8_context_t *intp, *xlat;
    c8_create_context(&intp, MODE_CASE);
    c8_create_context(&xlat, MODE_DBT);

    if ((0 > c8_load_file(intp, path)) || (0 > c8_load_file(xlat, path))) {
        log_err("error: failed to load rom\n");
        return 1;
    }

    c8_set_debugger_enabled(intp, 1);

    for (;;) {
        // need to guarantee identical execution of RND instruction
        unsigned int random_seed = (unsigned int)time(NULL);

        // execute binary translator, check number of effective cycles executed
        xlat->cycles = 0;
        srand(random_seed);
        long num_cycles = c8_execute_cycles(xlat, 1);

        // execute same number of cycles in interpreter
        intp->cycles = 0;
        srand(random_seed);
        c8_execute_cycles(intp, num_cycles);

        // compare state of registers and memory
        if (c8_debug_cmp_context(intp, xlat)) {
            log_dbg("error at PC A=%02X B=%02X\n", intp->pc, xlat->pc);

            log_dbg("===== Dumping Interpreter Registers =====\n");
            c8_debug_dump_context(intp);

            log_dbg("===== Dumping Translator Registers =====\n");
            c8_debug_dump_context(xlat);
            return 1;
        }
    }

    c8_destroy_context(intp);
    c8_destroy_context(xlat);
    return 0;
}

// -----------------------------------------------------------------------------
int c8_debug_cmp_context(const c8_context_t *a, const c8_context_t *b)
{
    int mismatch = 0;

    // compare special purpose registers
    if (a->pc != b->pc) {
        log_dbg("PC mismatch (A=%04X B=%04X)\n", a->pc, b->pc);
        mismatch = 1;
    }
    if (a->sp != b->sp) {
        log_dbg("SP mismatch (A=%04X B=%04X)\n", a->sp, b->sp);
        mismatch = 1;
    }
    if (a->i != b->i) {
        log_dbg("I mismatch (A=%04X B=%04X)\n", a->i, b->i);
        mismatch = 1;
    }
    if (a->dt != b->dt) {
        log_dbg("DT mismatch (A=%02X B=%02X)\n", a->dt, b->dt);
        mismatch = 1;
    }
    if (a->st != b->st) {
        log_dbg("ST mismatch (A=%02X B=%02X)\n", a->st, b->st);
        mismatch = 1;
    }

    // compare general purpose registers
    for (int i = 0; i < 16; ++i) {
        if (a->v[i] == b->v[i])
            continue;
        log_dbg("V%X mismatch (A=%02X B=%02X)\n", i, a->v[i], b->v[i]);
        mismatch = 1;
    }

    // compare stack
    for (int i = 0; i < STACK_SIZE; ++i) {
        if (a->stack[i] == b->stack[i])
            continue;
        log_dbg("STACK[%d] mismatch (A=%04X B=%04X)\n",
                i, a->stack[i], b->stack[i]);
        mismatch = 1;
    }

    return mismatch;
}

// -----------------------------------------------------------------------------
void c8_debug_dump_context(const c8_context_t *ctx)
{
    log_dbg("PC=%04X  SP=%04X  I=%04X  DT=%02X  ST=%02X\n",
            ctx->pc, ctx->sp, ctx->i, ctx->dt, ctx->st);

    for (int i = 0; i < 8; ++i)
        log_dbg("V%X=%02X  ", i, ctx->v[i]);
    log_dbg("\n");

    for (int i = 8; i < 16; i++)
        log_dbg("V%X=%02X  ", i, ctx->v[i]);
    log_dbg("\n");

#ifdef HAVE_SCHIP_SUPPORT
    for (int i = 0; i < 8; i++)
        log_dbg("H%X=%02X  ", i, ctx->hp[i]);
    log_dbg("\n");
#endif // HAVE_SCHIP_SUPPORT
}

