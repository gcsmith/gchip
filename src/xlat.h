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

#ifndef GCHIP_XLAT__H
#define GCHIP_XLAT__H

#include "chip8.h"

typedef struct xlat_block {
    uint8_t *block;     // start of translation buffer
    uint8_t *ptr;       // pointer to next instruction location
    long length;        // size of translation buffer
    long num_cycles;    // number of target instructions represented
    long visits;        // number of times this block has been executed
} xlat_block_t;

#define GUEST_REGS 21

typedef struct xlat_state {
    c8_context_t *ctx;
    xlat_block_t *xb;
    uint16_t opcode;
    uint16_t pc;
    int *free_regs;
    int num_free;
    int reg_map[GUEST_REGS];
    int reg_bits[GUEST_REGS];
    void *reg_sync[GUEST_REGS];
} xlat_state_t;

typedef void (*xlat_fn)(void);

int  xlat_alloc_block(xlat_block_t *xb, long length);
void xlat_free_block(xlat_block_t *xb);

int  xlat_alloc_state(xlat_state_t *xs);
void xlat_free_state(xlat_state_t *xs);

void xlat_emit_call_0(xlat_state_t *xs, void *f);
void xlat_emit_call_1(xlat_state_t *xs, void *f, size_t d1);
void xlat_emit_call_2(xlat_state_t *xs, void *f, size_t d1, size_t d2);
void xlat_emit_call_3(xlat_state_t *xs, void *f, size_t d1, size_t d2, size_t d3);
void xlat_emit_call_4(xlat_state_t *xs, void *f, size_t d1, size_t d2, size_t d3, size_t d4);
void xlat_emit_call_5(xlat_state_t *xs, void *f, size_t d1, size_t d2, size_t d3, size_t d4, size_t d5);

int  xlat_reserve_register(xlat_state_t *xs, int bits, int reg, void *sync);
int  xlat_reserve_register_wo(xlat_state_t *xs, int bits, int reg, void *sync);
int  xlat_reserve_register_temp(xlat_state_t *xs, int bits);
int  xlat_reserve_register_index(xlat_state_t *xs, int bits, int index);
void xlat_commit_register(xlat_state_t *xs, int bits, int reg);
void xlat_free_register(xlat_state_t *xs, int reg);
void xlat_free_register_temp(xlat_state_t *xs, int host_reg);

void xlat_emit_prologue(xlat_state_t *state);
void xlat_emit_epilogue(xlat_state_t *state);

void xlat_emit_add_sp(xlat_block_t *xb, int bytes);
void xlat_emit_sub_sp(xlat_block_t *xb, int bytes);

void xlat_emit_push_i16(xlat_block_t *xb, uint16_t is);
void xlat_emit_push_r16(xlat_block_t *xb, int rs);
void xlat_emit_push_i32(xlat_block_t *xb, uint32_t is);
void xlat_emit_push_r32(xlat_block_t *xb, int rs);

void xlat_emit_pop_r16(xlat_block_t *xb, int rd);
void xlat_emit_pop_r32(xlat_block_t *xb, int rd);

void xlat_emit_call_i32(xlat_block_t *xb, void *is);
void xlat_emit_call_r32(xlat_block_t *xb, int rs);
void xlat_emit_call_r64(xlat_block_t *xb, int rs);

void xlat_emit_or_r8r8(xlat_block_t *xb, int rs, int rd);
void xlat_emit_and_r8r8(xlat_block_t *xb, int rs, int rd);
void xlat_emit_xor_r8r8(xlat_block_t *xb, int rs, int rd);
void xlat_emit_add_r8r8(xlat_block_t *xb, int rs, int rd);

void xlat_emit_or_i8r8(xlat_block_t *xb, uint8_t imm, int rd);
void xlat_emit_and_i8r8(xlat_block_t *xb, uint8_t imm, int rd);
void xlat_emit_xor_i8r8(xlat_block_t *xb, uint8_t imm, int rd);
void xlat_emit_add_i8r8(xlat_block_t *xb, uint8_t is, int rd);

void xlat_emit_and_i16r16(xlat_block_t *xb, uint16_t imm, int rd);
void xlat_emit_add_i16r16(xlat_block_t *xb, uint16_t imm, int rd);

void xlat_emit_add_r16r16(xlat_block_t *xb, int rs, int rd);
void xlat_emit_add_i32r64(xlat_block_t *xb, uint32_t is, int rd);
void xlat_emit_add_r64r64(xlat_block_t *xb, int rs, int rd);

void xlat_emit_mov_r8r8(xlat_block_t *xb, int rs, int rd);
void xlat_emit_mov_r8m8(xlat_block_t *xb, int rs, uint8_t *md);
void xlat_emit_mov_m8r8(xlat_block_t *xb, uint8_t *ms, int rd);
void xlat_emit_mov_i8r8(xlat_block_t *xb, uint8_t is, int rd);
void xlat_emit_mov_i8m8(xlat_block_t *xb, uint8_t is, uint8_t *md);
void xlat_emit_mov_rmr8(xlat_block_t *xb, int rs, int rd);
void xlat_emit_mov_r8rm(xlat_block_t *xb, int rs, int rd);

void xlat_emit_mov_r16m16(xlat_block_t *xb, int rs, uint16_t *md);
void xlat_emit_mov_m16r16(xlat_block_t *xb, uint16_t *ms, int rd);
void xlat_emit_mov_i16r16(xlat_block_t *xb, uint16_t is, int rd);
void xlat_emit_mov_i16m16(xlat_block_t *xb, uint16_t is, uint16_t *md);

void xlat_emit_mov_i16rm_index(xlat_block_t *xb, uint16_t is, int rb, int ri);

void xlat_emit_mov_rmr16_offset(xlat_block_t *xb, int rs, int rd, int offset);
void xlat_emit_mov_i16rm_offset(xlat_block_t *xb, uint16_t is, int rd, int offset);
void xlat_emit_mov_r16rm_offset(xlat_block_t *xb, int rs, int rd, int offset);
void xlat_emit_mov_rmr64_offset(xlat_block_t *xb, int rs, int rd, int offset);
void xlat_emit_mov_r64rm_offset(xlat_block_t *xb, int rs, int rd, int offset);

void xlat_emit_mov_rmr16_scale(xlat_block_t *xb, int rs, int rb, int ri, int scale);
void xlat_emit_mov_r16rm_scale(xlat_block_t *xb, int rb, int ri, int scale, int rd);

void xlat_emit_mov_i64r64(xlat_block_t *xb, uint64_t is, int rd);

void xlat_emit_movzx_m8r32(xlat_block_t *xb, uint8_t *is, int rd);
void xlat_emit_movzx_m16r32(xlat_block_t *xb, uint16_t *is, int rd);

void xlat_emit_cmp_r8r8(xlat_block_t *xb, int rs, int rd);
void xlat_emit_cmp_i8r8(xlat_block_t *xb, uint8_t i8, int rd);

void xlat_emit_cmove_r16m16(xlat_block_t *xb, int rs, uint16_t *md);
void xlat_emit_cmovne_r16m16(xlat_block_t *xb, int rs, uint16_t *md);

void xlat_emit_cmove_r16r16(xlat_block_t *xb, int rs, int rd);
void xlat_emit_cmovne_r16r16(xlat_block_t *xb, int rs, int rd);

void xlat_emit_shl_i8r64(xlat_block_t *xb, uint8_t imm, int rd);
void xlat_emit_mul_r8(xlat_block_t *xb, int rs);

void xlat_emit_ret(xlat_block_t *xb);

#endif // GCHIP_XLAT__H

