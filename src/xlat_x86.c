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
#include <assert.h>
#include "xlat.h"

#ifdef ARCH_X86
#define HOST_REGS 8
#else
#define HOST_REGS 16
#endif

// -----------------------------------------------------------------------------
// Generate an offset from the current translated instruction to addr.
INLINE uint32_t memaddr(const xlat_block_t *xb, const void *addr, size_t length)
{
#ifdef ARCH_X86_64
    int64_t off = (int64_t)addr - ((int64_t)xb->ptr + length);
    if (off < -0x7FFFFFFF || off > 0x7FFFFFFF) {
        log_err("offset: %p - %p = %p\n", xb->ptr + length, addr, (void *)off);
        assert(!"cannot create RIP offset over 4GB!");
    }
    return (uint32_t)(off & 0xFFFFFFFF);
#else
    return (uint32_t)((int32_t)addr - ((int32_t)xb->ptr + length));
#endif
}

// -----------------------------------------------------------------------------
// Write an arbitrary 8-bit value to the translation buffer.
INLINE void emit_08(xlat_block_t *xb, uint8_t data)
{
    *(uint8_t *)xb->ptr++ = data;
}

// -----------------------------------------------------------------------------
// Write an arbitrary 16-bit value to the translation buffer.
INLINE void emit_16(xlat_block_t *xb, uint16_t data)
{
    *(uint16_t *)xb->ptr = data;
    xb->ptr += 2;
}

// -----------------------------------------------------------------------------
// Write an arbitrary 24-bit value to the translation buffer.
INLINE void emit_24(xlat_block_t *xb, uint32_t data)
{
    *(uint8_t *)xb->ptr++ = data & 0xFF;
    *(uint8_t *)xb->ptr++ = (data >> 8) & 0xFF;
    *(uint8_t *)xb->ptr++ = (data >> 16) & 0xFF;
}

// -----------------------------------------------------------------------------
// Write an arbitrary 32-bit value to the translation buffer.
INLINE void emit_32(xlat_block_t *xb, uint32_t data)
{
    *(uint32_t *)xb->ptr = data;
    xb->ptr += 4;
}

// -----------------------------------------------------------------------------
// Write an arbitrary 64-bit value to the translation buffer.
INLINE void emit_64(xlat_block_t *xb, uint64_t data)
{
    *(uint64_t *)xb->ptr = data;
    xb->ptr += 8;
}

// -----------------------------------------------------------------------------
// Write a ModR/M formatted prefix byte to the translation buffer.
INLINE void emit_modrm(xlat_block_t *xb, int mod, int reg, int rm)
{
    emit_08(xb, (mod << 6) | ((reg & 7) << 3) | (rm & 7));
}

// -----------------------------------------------------------------------------
// Write a SIB formatted prefix byte to the translation buffer.
INLINE void emit_sib(xlat_block_t *xb, int scale, int index, int base)
{
    emit_08(xb, (scale << 6) | ((index & 7) << 3) | (base & 7));
}

#ifdef ARCH_X86_64

// -----------------------------------------------------------------------------
// Write a REX formatted prefix byte to the translation buffer.
INLINE void emit_rex(xlat_block_t *xb, int w, int r, int x, int b)
{
    emit_08(xb, 0x40 | ((w&1) << 3) | ((r&1) << 2) | ((x&1) << 1) | (b&1));
}

// -----------------------------------------------------------------------------
INLINE void emit_rexr(xlat_block_t *xb, int w, int r)
{
    if (w || (r >= 8)) emit_rex(xb, w, r >= 8, 0, 0);
}

// -----------------------------------------------------------------------------
INLINE void emit_rexb(xlat_block_t *xb, int w, int b)
{
    if (w || (b >= 8)) emit_rex(xb, w, 0, 0, b >= 8);
}

// -----------------------------------------------------------------------------
INLINE void emit_rexrb(xlat_block_t *xb, int w, int r, int b)
{
    if (w || (r >= 8) || (b >= 8)) emit_rex(xb, w, r >= 8, 0, b >= 8);
}

// -----------------------------------------------------------------------------
INLINE void emit_rexrxb(xlat_block_t *xb, int w, int r, int x, int b)
{
    if (w || (r >= 8) || (x >= 8) || (b >= 8))
        emit_rex(xb, w, r >= 8, x >= 8, b >= 8);
}

#else

#define emit_rex(xb, w, r, x, b)
#define emit_rexr(xb, w, r)
#define emit_rexb(xb, w, b)
#define emit_rexrb(xb, w, r, b)
#define emit_rexrxb(xb, w, r, x, b)

#endif

void emit_call_func_0(xlat_state_t *xs, void *func)
{
    int r0 = xlat_reserve_register_index(xs, 32, 0);
#ifdef PLATFORM_WIN32
    xlat_emit_sub_sp(xs->xb, 32);
#endif
    xlat_emit_mov_i64r64(xs->xb, (uint64_t)func, r0);
    xlat_emit_call_r64(xs->xb, r0);
#ifdef PLATFORM_WIN32
    xlat_emit_add_sp(xs->xb, 32);
#endif
}

void emit_call_func_1(xlat_state_t *xs, void *func, uint64_t data1)
{
#ifdef PLATFORM_WIN32
    xlat_emit_push_r32(xs->xb, 0);
    xlat_emit_push_r32(xs->xb, 1);
    xlat_emit_sub_sp(xs->xb, 32);
#endif
    xlat_emit_mov_i64r64(xs->xb, data1, 1);
    xlat_emit_mov_i64r64(xs->xb, (uint64_t)func, 0);
    xlat_emit_call_r64(xs->xb, 0);
#ifdef PLATFORM_WIN32
    xlat_emit_add_sp(xs->xb, 32);
    xlat_emit_pop_r32(xs->xb, 1);
    xlat_emit_pop_r32(xs->xb, 0);
#endif
}

void emit_call_func_2(xlat_state_t *xs, void *func, uint64_t data1, uint64_t data2)
{
#ifdef PLATFORM_WIN32
    xlat_emit_push_r32(xs->xb, 0);
    xlat_emit_push_r32(xs->xb, 1);
    xlat_emit_push_r32(xs->xb, 2);
    xlat_emit_sub_sp(xs->xb, 32);
#endif
    xlat_emit_mov_i64r64(xs->xb, data1, 1);
    xlat_emit_mov_i64r64(xs->xb, data2, 2);
    xlat_emit_mov_i64r64(xs->xb, (uint64_t)func, 0);
    xlat_emit_call_r64(xs->xb, 0);
#ifdef PLATFORM_WIN32
    xlat_emit_add_sp(xs->xb, 32);
    xlat_emit_pop_r32(xs->xb, 2);
    xlat_emit_pop_r32(xs->xb, 1);
    xlat_emit_pop_r32(xs->xb, 0);
#endif
}

void emit_call_func_3(xlat_state_t *xs, void *func, uint64_t data1, uint64_t data2, uint64_t data3)
{
#ifdef PLATFORM_WIN32
    xlat_emit_push_r32(xs->xb, 0);
    xlat_emit_push_r32(xs->xb, 1);
    xlat_emit_push_r32(xs->xb, 2);
    xlat_emit_push_r32(xs->xb, 8);
    xlat_emit_sub_sp(xs->xb, 32);
#endif
    xlat_emit_mov_i64r64(xs->xb, data1, 1);
    xlat_emit_mov_i64r64(xs->xb, data2, 2);
    xlat_emit_mov_i64r64(xs->xb, data3, 8);
    xlat_emit_mov_i64r64(xs->xb, (uint64_t)func, 0);
    xlat_emit_call_r64(xs->xb, 0);
#ifdef PLATFORM_WIN32
    xlat_emit_add_sp(xs->xb, 32);
    xlat_emit_pop_r32(xs->xb, 8);
    xlat_emit_pop_r32(xs->xb, 2);
    xlat_emit_pop_r32(xs->xb, 1);
    xlat_emit_pop_r32(xs->xb, 0);
#endif
}

void emit_call_func_4(xlat_state_t *xs, void *func, uint64_t data1, uint64_t data2, uint64_t data3, uint64_t data4)
{
#ifdef PLATFORM_WIN32
    xlat_emit_push_r32(xs->xb, 0);
    xlat_emit_push_r32(xs->xb, 1);
    xlat_emit_push_r32(xs->xb, 2);
    xlat_emit_push_r32(xs->xb, 8);
    xlat_emit_push_r32(xs->xb, 9);
    xlat_emit_sub_sp(xs->xb, 32);
#endif
    xlat_emit_mov_i64r64(xs->xb, data1, 1);
    xlat_emit_mov_i64r64(xs->xb, data2, 2);
    xlat_emit_mov_i64r64(xs->xb, data3, 8);
    xlat_emit_mov_i64r64(xs->xb, data4, 9);
    xlat_emit_mov_i64r64(xs->xb, (uint64_t)func, 0);
    xlat_emit_call_r64(xs->xb, 0);
#ifdef PLATFORM_WIN32
    xlat_emit_add_sp(xs->xb, 32);
    xlat_emit_pop_r32(xs->xb, 9);
    xlat_emit_pop_r32(xs->xb, 8);
    xlat_emit_pop_r32(xs->xb, 2);
    xlat_emit_pop_r32(xs->xb, 1);
    xlat_emit_pop_r32(xs->xb, 0);
#endif
}

// -----------------------------------------------------------------------------
void xlat_emit_call_0(xlat_state_t *xs, void *f)
{
    int rax = xlat_reserve_register_index(xs, 32, 0);
    xlat_emit_mov_i64r64(xs->xb, (uint64_t)f, rax);
    xlat_emit_call_r64(xs->xb, rax);
}

// -----------------------------------------------------------------------------
void xlat_emit_call_1(xlat_state_t *xs, void *f, size_t d1)
{
    int rax = xlat_reserve_register_index(xs, 32, 0);
    int rdi = xlat_reserve_register_index(xs, 32, 7);
    xlat_emit_mov_i64r64(xs->xb, d1, rdi);
    xlat_emit_mov_i64r64(xs->xb, (uint64_t)f, rax);
    xlat_emit_call_r64(xs->xb, rax);
}

// -----------------------------------------------------------------------------
void xlat_emit_call_2(xlat_state_t *xs, void *f, size_t d1, size_t d2)
{
    int rax = xlat_reserve_register_index(xs, 32, 0);
    int rdi = xlat_reserve_register_index(xs, 32, 7);
    int rsi = xlat_reserve_register_index(xs, 32, 6);
    xlat_emit_mov_i64r64(xs->xb, d1, rdi);
    xlat_emit_mov_i64r64(xs->xb, d2, rsi);
    xlat_emit_mov_i64r64(xs->xb, (uint64_t)f, rax);
    xlat_emit_call_r64(xs->xb, rax);
}

// -----------------------------------------------------------------------------
void xlat_emit_call_4(xlat_state_t *xs, void *f, size_t d1, size_t d2,
        size_t d3, size_t d4)
{
    int rax = xlat_reserve_register_index(xs, 32, 0);
    int rdi = xlat_reserve_register_index(xs, 32, 7);
    int rsi = xlat_reserve_register_index(xs, 32, 6);
    int rdx = xlat_reserve_register_index(xs, 32, 2);
    int rcx = xlat_reserve_register_index(xs, 32, 1);
    xlat_emit_mov_i64r64(xs->xb, d1, rdi);
    xlat_emit_mov_i64r64(xs->xb, d2, rsi);
    xlat_emit_mov_i64r64(xs->xb, d3, rdx);
    xlat_emit_mov_i64r64(xs->xb, d4, rcx);
    xlat_emit_mov_i64r64(xs->xb, (uint64_t)f, rax);
    xlat_emit_call_r64(xs->xb, rax);
}

// -----------------------------------------------------------------------------
void xlat_emit_call_5(xlat_state_t *xs, void *f, size_t d1, size_t d2,
        size_t d3, size_t d4, size_t d5)
{
    int rax = xlat_reserve_register_index(xs, 32, 0);
    int rdi = xlat_reserve_register_index(xs, 32, 7);
    int rsi = xlat_reserve_register_index(xs, 32, 6);
    int rdx = xlat_reserve_register_index(xs, 32, 2);
    int rcx = xlat_reserve_register_index(xs, 32, 1);
    int r08 = xlat_reserve_register_index(xs, 32, 8);
    xlat_emit_mov_i64r64(xs->xb, d1, rdi);
    xlat_emit_mov_i64r64(xs->xb, d2, rsi);
    xlat_emit_mov_i64r64(xs->xb, d3, rdx);
    xlat_emit_mov_i64r64(xs->xb, d4, rcx);
    xlat_emit_mov_i64r64(xs->xb, d5, r08);
    xlat_emit_mov_i64r64(xs->xb, (uint64_t)f, rax);
    xlat_emit_call_r64(xs->xb, rax);
}

// -----------------------------------------------------------------------------
void xlat_emit_add_sp(xlat_block_t *xb, int bytes)
{
    emit_rex(xb, 1, 0, 0, 0);
    emit_08(xb, 0x81);
    emit_modrm(xb, 3, 0, 4);
    emit_32(xb, bytes);
}

// -----------------------------------------------------------------------------
void xlat_emit_sub_sp(xlat_block_t *xb, int bytes)
{
    emit_rex(xb, 1, 0, 0, 0);
    emit_08(xb, 0x81); 
    emit_modrm(xb, 3, 5, 4);
    emit_32(xb, bytes);
}

// -----------------------------------------------------------------------------
void xlat_emit_push_i16(xlat_block_t *xb, uint16_t is)
{
    emit_08(xb, 0x68);
    emit_32(xb, is);
}

// -----------------------------------------------------------------------------
void xlat_emit_push_r16(xlat_block_t *xb, int rs)
{
    emit_08(xb, 0x66);
    emit_08(xb, 0x50 | rs);
}

// -----------------------------------------------------------------------------
void xlat_emit_push_i32(xlat_block_t *xb, uint32_t is)
{
    emit_32(xb, is);
}

// -----------------------------------------------------------------------------
void xlat_emit_push_r32(xlat_block_t *xb, int rs)
{
    emit_rex(xb, 0, 0, 0, rs >> 3);
    emit_08(xb, 0x50 | (rs & 7));
}

// -----------------------------------------------------------------------------
void xlat_emit_pop_r16(xlat_block_t *xb, int rd)
{
    emit_08(xb, 0x66);
    emit_08(xb, 0x58 | rd);
}

// -----------------------------------------------------------------------------
void xlat_emit_pop_r32(xlat_block_t *xb, int rd)
{
    emit_rex(xb, 0, 0, 0, rd >> 3);
    emit_08(xb, 0x58 | (rd & 7));
}

// -----------------------------------------------------------------------------
void xlat_emit_call_i32(xlat_block_t *xb, void *is)
{
    uint64_t off = (uint64_t)is - ((uint64_t)xb->ptr + 5);
    assert((int64_t)off <= 0x7fffffff && (int64_t)off >= -0x7fffffff);
    emit_08(xb, 0xE8);
    emit_32(xb, (uint32_t)off);
}

// -----------------------------------------------------------------------------
void xlat_emit_call_r32(xlat_block_t *xb, int rs)
{
    emit_08(xb, 0xFF);
    emit_modrm(xb, 3, 2, rs);
}

// -----------------------------------------------------------------------------
void xlat_emit_call_r64(xlat_block_t *xb, int rs)
{
    emit_rexb(xb, 0, rs);
    emit_08(xb, 0xFF);
    emit_modrm(xb, 3, 2, rs);
}

// -----------------------------------------------------------------------------
void xlat_emit_or_r8r8(xlat_block_t *xb, int rs, int rd)
{
    emit_rexrb(xb, 0, rs, rd);
    emit_08(xb, 0x08);
    emit_modrm(xb, 3, rs, rd);
}

// -----------------------------------------------------------------------------
void xlat_emit_and_r8r8(xlat_block_t *xb, int rs, int rd)
{
    emit_rexrb(xb, 0, rd, rs);
    emit_08(xb, 0x22);
    emit_modrm(xb, 3, rd, rs);
}

// -----------------------------------------------------------------------------
void xlat_emit_xor_r8r8(xlat_block_t *xb, int rs, int rd)
{
    emit_rexrb(xb, 0, rd, rs);
    emit_08(xb, 0x32);
    emit_modrm(xb, 3, rd, rs);
}

// -----------------------------------------------------------------------------
void xlat_emit_add_r8r8(xlat_block_t *xb, int rs, int rd)
{
    emit_rexrb(xb, 0, rd, rs);
    emit_08(xb, 0x00);
    emit_modrm(xb, 3, rd, rs);
}

// -----------------------------------------------------------------------------
void xlat_emit_or_i8r8(xlat_block_t *xb, uint8_t imm, int rd)
{
    emit_rexb(xb, 0, rd);
    emit_08(xb, 0x80);
    emit_modrm(xb, 3, 1, rd);
    emit_08(xb, imm);
}

// -----------------------------------------------------------------------------
void xlat_emit_and_i8r8(xlat_block_t *xb, uint8_t imm, int rd)
{
    emit_rexb(xb, 0, rd);
    emit_08(xb, 0x80);
    emit_modrm(xb, 3, 4, rd);
    emit_08(xb, imm);
}

// -----------------------------------------------------------------------------
void xlat_emit_xor_i8r8(xlat_block_t *xb, uint8_t imm, int rd)
{
    emit_rexb(xb, 0, rd);
    emit_08(xb, 0x80);
    emit_modrm(xb, 3, 6, rd);
    emit_08(xb, imm);
}

// -----------------------------------------------------------------------------
void xlat_emit_add_i8r8(xlat_block_t *xb, uint8_t imm, int rd)
{
    emit_rexb(xb, 0, rd);
    emit_08(xb, 0x80);
    emit_modrm(xb, 3, 0, rd);
    emit_08(xb, imm);
}

// -----------------------------------------------------------------------------
void xlat_emit_and_i16r16(xlat_block_t *xb, uint16_t imm, int rd)
{

}

// -----------------------------------------------------------------------------
void xlat_emit_add_i16r16(xlat_block_t *xb, uint16_t imm, int rd)
{
    emit_08(xb, 0x66);
    emit_rexb(xb, 0, rd);
    emit_08(xb, 0x83);
    emit_modrm(xb, 3, 0, rd);
    emit_08(xb, imm);
}

// -----------------------------------------------------------------------------
void xlat_emit_add_r16r16(xlat_block_t *xb, int rs, int rd)
{
    emit_08(xb, 0x66);
    emit_rexrb(xb, 0, rd, rs);
    emit_08(xb,  0x03 ); 
    emit_modrm(xb, 3, rd, rs);
}

// -----------------------------------------------------------------------------
void xlat_emit_add_i32r64(xlat_block_t *xb, uint32_t is, int rd)
{
    emit_rex(xb, 1, 0, 0, rd >> 3);
    if (rd == 0) {
        emit_08(xb, 0x05);
    }
    else {
        emit_08(xb, 0x81);
        emit_modrm(xb, 3, 0, rd);
    }
    emit_32(xb, is);
}

// -----------------------------------------------------------------------------
void xlat_emit_add_r64r64(xlat_block_t *xb, int rs, int rd)
{
    emit_rexrb(xb, 1, rs, rd);
    emit_08(xb, 0x01); 
    emit_modrm(xb, 3, rs, rd);
}

// -----------------------------------------------------------------------------
void xlat_emit_mov_r8r8(xlat_block_t *xb, int rs, int rd)
{
    emit_rexrb(xb, 0, rs, rd);
    emit_08(xb, 0x88);
    emit_modrm(xb, 3, rs, rd);
}

// -----------------------------------------------------------------------------
void xlat_emit_mov_r8m8(xlat_block_t *xb, int rs, uint8_t *md)
{
    emit_rexr(xb, 0, rs);
    emit_08(xb, 0x88);
    emit_modrm(xb, 0, rs, 5);
    emit_32(xb, memaddr(xb, md, 4));
}

// -----------------------------------------------------------------------------
void xlat_emit_mov_m8r8(xlat_block_t *xb, uint8_t *ms, int rd)
{
    emit_rexr(xb, 0, rd);
    emit_08(xb, 0x8A);
    emit_modrm(xb, 0, rd, 5);
    emit_32(xb, memaddr(xb, ms, 4));
}

// -----------------------------------------------------------------------------
void xlat_emit_mov_i8r8(xlat_block_t *xb, uint8_t is, int rd)
{
    emit_rexb(xb, 0, rd);
    emit_08(xb, 0xB0 | (rd & 7));
    emit_08(xb, is);
}

// -----------------------------------------------------------------------------
void xlat_emit_mov_i8m8(xlat_block_t *xb, uint8_t is, uint8_t *md)
{
    emit_08(xb, 0xC6);
    emit_modrm(xb, 0, 0, 5);
    emit_32(xb, memaddr(xb, md, 5));
    emit_08(xb, is);
}

// XXX: borrowed from PCSX2 -- rewrite me
INLINE void WriteRmOffset(xlat_block_t *xb, int to, int32_t offset)
{
    if ((to & 7) == 7) {
        if (offset == 0) {
            emit_modrm(xb, 0, 0, 4);
            emit_sib(xb, 0, 7, 4);
        }
        else if (offset < 128 && offset >= -128) {
            emit_modrm(xb, 1, 0, 4);
            emit_sib(xb, 0, 7, 4);
            emit_08(xb,offset);
        }
        else {
            emit_modrm(xb, 2, 0, 4);
            emit_sib(xb, 0, 7, 4);
            emit_32(xb,offset);
        }
    }
    else {
        if (offset == 0) {
            emit_modrm(xb, 0, 0, to);
        }
        else if (offset < 128 && offset >= -128) {
            emit_modrm(xb, 1, 0, to);
            emit_08(xb,offset);
        }
        else {
            emit_modrm(xb, 2, 0, to);
            emit_32(xb,offset);
        }
    }
}

// XXX: borrowed from PCSX2 -- rewrite me
INLINE void WriteRmOffsetFrom(xlat_block_t *xb, int to, int from, int offset)
{
    if ((from&7) == 4) {
        if( offset == 0 ) {
            emit_modrm(xb, 0, to, 0x4);
            emit_sib(xb, 0, 0x4, 0x4);
        }
        else if( offset < 128 && offset >= -128 ) {
            emit_modrm(xb, 1, to, 0x4);
            emit_sib(xb, 0, 0x4, 0x4);
            emit_08(xb, offset);
        }
        else {
            emit_modrm(xb, 2, to, 0x4);
            emit_sib(xb, 0, 0x4, 0x4);
            emit_32(xb, offset);
        }
    }
    else {
        if( offset == 0 ) {
            emit_modrm(xb, 0, to, from );
        }
        else if( offset < 128 && offset >= -128 ) {
            emit_modrm(xb, 1, to, from );
            emit_08(xb, offset);
        }
        else {
            emit_modrm(xb, 2, to, from );
            emit_32(xb, offset);
        }
    }
}

// -----------------------------------------------------------------------------
void xlat_emit_mov_rmr8(xlat_block_t *xb, int rs, int rd)
{
    emit_rexrb(xb, 0, rd, rs);
    emit_08(xb, 0x8A);
    WriteRmOffsetFrom(xb, rd, rs, 0);
}

// -----------------------------------------------------------------------------
void xlat_emit_mov_r8rm(xlat_block_t *xb, int rs, int rd)
{
    emit_rexrb(xb, 0, rs, rd);
    emit_08(xb, 0x88);
    WriteRmOffsetFrom(xb, rs, rd, 0);
}

// -----------------------------------------------------------------------------
void xlat_emit_mov_r16m16(xlat_block_t *xb, int rs, uint16_t *md)
{
    emit_08(xb, 0x66);
    emit_rexr(xb, 0, rs);
    emit_08(xb, 0x89);
    emit_modrm(xb, 0, rs, 5);
    emit_32(xb, memaddr(xb, md, 4));
}

// -----------------------------------------------------------------------------
void xlat_emit_mov_m16r16(xlat_block_t *xb, uint16_t *ms, int rd)
{
    emit_08(xb, 0x66);
    emit_rexr(xb, 0, rd);
    emit_08(xb, 0x8B);
    emit_modrm(xb, 0, rd, 5);
    emit_32(xb, memaddr(xb, ms, 4)); 
}

// -----------------------------------------------------------------------------
void xlat_emit_mov_i16r16(xlat_block_t *xb, uint16_t is, int rd)
{
    emit_08(xb, 0x66);
    emit_rexb(xb, 0, rd);
    emit_08(xb, 0xB8 | (rd & 0x7)); 
    emit_16(xb, is);
}

// -----------------------------------------------------------------------------
void xlat_emit_mov_i16m16(xlat_block_t *xb, uint16_t is, uint16_t *md)
{
    emit_08(xb, 0x66);
    emit_08(xb, 0xC7);
    emit_modrm(xb, 0, 0, 5);
    emit_32(xb, memaddr(xb, md, 6));
    emit_16(xb, is); 
}

// -----------------------------------------------------------------------------
void xlat_emit_mov_i16rm_index(xlat_block_t *xb, uint16_t is, int rb, int ri)
{
    emit_08(xb, 0x66);
    emit_rexb(xb, 0, rb);
    emit_08(xb, 0xC7);
    emit_modrm(xb, 0, 0, 4);
    emit_sib(xb, 0, ri, rb);
    emit_16(xb, is);
}

// -----------------------------------------------------------------------------
void xlat_emit_mov_rmr16_offset(xlat_block_t *xb, int rs, int rd, int off)
{
    emit_08(xb, 0x66);
    emit_rexrb(xb, 0, rd, rs);
    emit_08(xb, 0x8B);
    WriteRmOffsetFrom(xb, rd, rs, off);
}

// -----------------------------------------------------------------------------
void xlat_emit_mov_i16rm_offset(xlat_block_t *xb, uint16_t is, int rd, int off)
{
    emit_08(xb, 0x66);
    emit_rexb(xb, 0, rd);
    emit_08(xb, 0xC7);
    WriteRmOffset(xb, rd, off);
    emit_16(xb, is);
}

// -----------------------------------------------------------------------------
void xlat_emit_mov_r16rm_offset(xlat_block_t *xb, int rs, int rd, int off)
{
    emit_08(xb, 0x66);
    emit_rexrb(xb, 0, rs, rd);
    emit_08(xb, 0x89);
    emit_modrm(xb, 0, rs, rd );
}

// -----------------------------------------------------------------------------
void xlat_emit_mov_rmr16_scale(xlat_block_t *xb, int rs, int rb, int ri, int scale)
{
    emit_08(xb, 0x66);
    emit_rexrxb(xb, 0, rs, ri, rb);
    emit_08(xb, 0x8B);
    emit_modrm(xb, 0, rs, 0x4 );
    emit_sib(xb, scale, ri, rb);
}

// -----------------------------------------------------------------------------
void xlat_emit_mov_r16rm_scale(xlat_block_t *xb, int rb, int ri, int scale, int rd)
{
    emit_08(xb, 0x66);
    emit_rexrxb(xb, 0, rd, ri, rb);
    emit_08(xb, 0x89);
    emit_modrm(xb, 0, rd, 0x4 );
    emit_sib(xb, scale, ri, rb);
}

// -----------------------------------------------------------------------------
void xlat_emit_mov_rmr64_offset(xlat_block_t *xb, int rs, int rd, int off)
{
    emit_rexrb(xb, 1, rd, rs);
    emit_08(xb, 0x8B);
    WriteRmOffsetFrom(xb, rd, rs, off);
}

// -----------------------------------------------------------------------------
void xlat_emit_mov_r64rm_offset(xlat_block_t *xb, int rs, int rd, int off)
{
    emit_rexrb(xb, 1, rs, rd);
    emit_08(xb, 0x89);
    WriteRmOffsetFrom(xb, rs, rd, off);
}

// -----------------------------------------------------------------------------
void xlat_emit_mov_i64r64(xlat_block_t *xb, uint64_t is, int rd)
{
    emit_rexb(xb, 1, rd);
    emit_08(xb, 0xB8 | (rd & 7));
    emit_64(xb, is);
}

// -----------------------------------------------------------------------------
void xlat_emit_movzx_m8r32(xlat_block_t *xb, uint8_t *is, int rd)
{
    emit_rexr(xb, 0, rd);
    emit_16(xb, 0xB60F); 
    emit_modrm(xb, 0, rd, 5);
    emit_32(xb, memaddr(xb, is, 4));
}

// -----------------------------------------------------------------------------
void xlat_emit_movzx_m16r32(xlat_block_t *xb, uint16_t *is, int rd)
{
    emit_rexr(xb, 0, rd);
    emit_16(xb, 0xB70F); 
    emit_modrm(xb, 0, rd, 5);
    emit_32(xb, memaddr(xb, is, 4));
}

// -----------------------------------------------------------------------------
void xlat_emit_movzx_m8r64(xlat_block_t *xb, uint8_t *is, int rd)
{
}

// -----------------------------------------------------------------------------
void xlat_emit_cmp_r8r8(xlat_block_t *xb, int rs, int rd)
{
    emit_rexrb(xb, 0, rs, rd);
    emit_08(xb, 0x3A);
    emit_modrm(xb, 3, rs, rd);
}

// -----------------------------------------------------------------------------
void xlat_emit_cmp_i8r8(xlat_block_t *xb, uint8_t i8, int rd)
{
    emit_rexb(xb, 0, rd);
    if (rd == 0) {
        emit_08(xb, 0x3C);
    } 
    else {
        emit_08(xb, 0x80 );
        emit_modrm(xb, 3, 7, rd);
    }
    emit_08(xb, i8);
}

// -----------------------------------------------------------------------------
void xlat_emit_cmove_r16m16(xlat_block_t *xb, int is, uint16_t *md)
{
}

// -----------------------------------------------------------------------------
void xlat_emit_cmovne_r16m16(xlat_block_t *xb, int rs, uint16_t *md)
{
}

// -----------------------------------------------------------------------------
void xlat_emit_cmove_r16r16(xlat_block_t *xb, int rs, int rd)
{
    emit_08(xb, 0x66);
    emit_rexrb(xb, 0, rd, rs);
    emit_08(xb, 0x0F);
    emit_08(xb, 0x44);
    emit_modrm(xb, 3, rd, rs);
}

// -----------------------------------------------------------------------------
void xlat_emit_cmovne_r16r16(xlat_block_t *xb, int rs, int rd)
{
    emit_08(xb, 0x66);
    emit_rexrb(xb, 0, rd, rs);
    emit_08(xb, 0x0F);
    emit_08(xb, 0x45);
    emit_modrm(xb, 3, rd, rs);
}

// -----------------------------------------------------------------------------
void xlat_emit_ret(xlat_block_t *xb)
{
    emit_08(xb, 0xC3);
}

// -----------------------------------------------------------------------------
void xlat_emit_shl_i8r64(xlat_block_t *xb, uint8_t imm, int rd)
{
    emit_rexb(xb, 1, rd);
    if (imm == 1) {
        emit_08(xb, 0xD1);
        emit_modrm(xb, 3, 4, rd);
    }
    else {
        emit_08(xb, 0xC1); 
        emit_modrm(xb, 3, 4, rd);
        emit_08(xb, imm); 
    }
}

// -----------------------------------------------------------------------------
void xlat_emit_mul_r8(xlat_block_t *xb, int rs)
{
    emit_rexb(xb, 0, rs);
    emit_08(xb, 0xF6);
    emit_modrm(xb, 3, 4, rs);
}

// -----------------------------------------------------------------------------
int xlat_alloc_state(xlat_state_t *xs)
{
    int i;

    // allocate and populate host register list
    xs->num_free = HOST_REGS;
    xs->free_regs = (int *)malloc(sizeof(int) * HOST_REGS);

    for (i = 0; i < HOST_REGS; ++i)
        xs->free_regs[i] = i;

    // set all guest register mappings to unreserved state
    for (i = 0; i < GUEST_REGS; ++i)
        xs->reg_map[i] = -1;

    return 0;
}

// -----------------------------------------------------------------------------
void xlat_free_state(xlat_state_t *xs)
{
    assert(NULL != xs->free_regs);
    SAFE_FREE(xs->free_regs);
}

// -----------------------------------------------------------------------------
int xlat_reserve_register(xlat_state_t *xs, int bits, int reg, void *sync)
{
    if ((reg >= 0) && (xs->reg_map[reg] >= 0)) {
        // the register is already reserved, just return its assigned index
        return xs->reg_map[reg];
    }

    if (xs->num_free <= 0) {
        // we're out of registers, need to do a replacement... (TODO)
        assert(!"register replacement not yet implemented!");
        return -1;
    }
    else {
        // pull the next unreserved host register from the free list
        int host_reg = xs->free_regs[--xs->num_free];
        if ((reg < 0) || (NULL == sync))
            return host_reg;
        
        assert((reg >= 0) && (NULL != sync));
        xs->reg_map[reg] = host_reg;
        xs->reg_bits[reg] = bits;
        xs->reg_sync[reg] = sync;

        // initialize the register using the emulator context
        switch (bits) {
        default:
            assert(!"xlat_reserve_register: invalid bit width specified");
            // fall through to 8-bit for release builds
        case 8:
            xlat_emit_movzx_m8r32(xs->xb, (uint8_t *)sync, host_reg);
            break;
        case 16:
            xlat_emit_movzx_m16r32(xs->xb, (uint16_t *)sync, host_reg);
            break;
        }

        return host_reg;
    }
}

// -----------------------------------------------------------------------------
int xlat_reserve_register_wo(xlat_state_t *xs, int bits, int reg, void *sync)
{
    if ((reg >= 0) && (xs->reg_map[reg] >= 0)) {
        // the register is already reserved, just return its assigned index
        return xs->reg_map[reg];
    }

    if (xs->num_free <= 0) {
        // we're out of registers, need to do a replacement... (TODO)
        assert(!"register replacement not yet implemented!");
        return -1;
    }
    else {
        // pull the next unreserved host register from the free list
        int host_reg = xs->free_regs[--xs->num_free];
        if ((reg < 0) || (NULL == sync))
            return host_reg;
        
        assert((reg >= 0) && (NULL != sync));
        xs->reg_map[reg] = host_reg;
        xs->reg_bits[reg] = bits;
        xs->reg_sync[reg] = sync;
        return host_reg;
    }
}

// -----------------------------------------------------------------------------
int xlat_reserve_register_temp(xlat_state_t *xs, int bits)
{
    return xlat_reserve_register(xs, bits, -1, NULL);
}

// -----------------------------------------------------------------------------
int xlat_reserve_register_index(xlat_state_t *xs, int bits, int index)
{
    int i;
    for (i = 0; i < GUEST_REGS; i++) {
        if (xs->reg_map[i] == index) {
            log_spew("ejecting register index %d (%d)\n", index, i);
            xlat_free_register(xs, i);
            break;
        }
    }
    return index;
}

// -----------------------------------------------------------------------------
void xlat_commit_register(xlat_state_t *xs, int bits, int reg)
{
    void *reg_sync = xs->reg_sync[reg];
    int host_reg = xs->reg_map[reg];
    if (host_reg < 0)
        return;

    switch (bits) {
    default:
        assert(!"xlat_free_register: invalid bit width specified");
        // fall through to 8-bit for release builds
    case 8:
        xlat_emit_mov_r8m8(xs->xb, host_reg, (uint8_t *)reg_sync);
        break;
    case 16:
        xlat_emit_mov_r16m16(xs->xb, host_reg, (uint16_t *)reg_sync);
        break;
    }
}

// -----------------------------------------------------------------------------
void xlat_free_register(xlat_state_t *xs, int reg)
{
    if (xs->reg_map[reg] < 0) {
        assert(!"attempting to free unreserved register");
        return;
    }

    // write register back to the emulator context and add to unreserved list
    xs->free_regs[xs->num_free++] = xs->reg_map[reg];
    xlat_commit_register(xs, xs->reg_bits[reg], reg);
}

// -----------------------------------------------------------------------------
void xlat_free_register_temp(xlat_state_t *xs, int host_reg)
{
    xs->free_regs[xs->num_free++] = host_reg;
}

// -----------------------------------------------------------------------------
void xlat_emit_prologue(xlat_state_t *state)
{
    // save the reserved registers to the stack before we continue
#ifdef PLATFORM_WIN32
    xlat_emit_push_r32(state->xb, 3);
    xlat_emit_push_r32(state->xb, 6);
    xlat_emit_push_r32(state->xb, 7);
#else
    xlat_emit_push_r32(state->xb, 0xC);
    xlat_emit_push_r32(state->xb, 0xE);
#endif // PLATFORM_WIN32
}

// -----------------------------------------------------------------------------
void xlat_emit_epilogue(xlat_state_t *state)
{
    int i;

    // commit updated registers to the emulator context
    for (i = 0; i < GUEST_REGS; ++i)
        if (state->reg_map[i] >= 0)
            xlat_free_register(state, i);

    // pop the reserved registers from the stack before returning to interpreter
#ifdef PLATFORM_WIN32
    xlat_emit_pop_r32(state->xb, 7);
    xlat_emit_pop_r32(state->xb, 6);
    xlat_emit_pop_r32(state->xb, 3);
#else
    xlat_emit_pop_r32(state->xb, 0xE);
    xlat_emit_pop_r32(state->xb, 0xC);
#endif // PLATFORM_WIN32

    // generate a return to escape back to the interpreter
    xlat_emit_ret(state->xb);
}

