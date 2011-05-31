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

switch (OPCODE >> 12) {
case 0x0:
    switch ((OPCODE >> 8) & 0xF) {
    case 0x0:
        switch (OPCODE & 0xFF) {
        case 0xE0: OP(sys_cls); break;
        case 0xEE: OP(sys_ret); break;
#ifdef HAVE_MCHIP_SUPPORT
        case 0x10: OP(meg_off); break;
        case 0x11: OP(meg_on); break;
        case 0xB0: case 0xB1: case 0xB2: case 0xB3:
        case 0xB4: case 0xB5: case 0xB6: case 0xB7:
        case 0xB8: case 0xB9: case 0xBA: case 0xBB:
        case 0xBC: case 0xBD: case 0xBE: case 0xBF:
            OP(meg_scru); break;
#endif
#ifdef HAVE_SCHIP_SUPPORT
        case 0xC0: case 0xC1: case 0xC2: case 0xC3:
        case 0xC4: case 0xC5: case 0xC6: case 0xC7:
        case 0xC8: case 0xC9: case 0xCA: case 0xCB:
        case 0xCC: case 0xCD: case 0xCE: case 0xCF:
            OP(sup_scd); break;
        case 0xFB: OP(sup_scr); break;
        case 0xFC: OP(sup_scl); break;
        case 0xFD: OP(sup_brk); break;
        case 0xFE: OP(sup_ch8); break;
        case 0xFF: OP(sup_sch); break;
#endif
        } break;
#ifdef HAVE_MCHIP_SUPPORT
    case 0x1: OP(meg_ldhi); break;
    case 0x2: OP(meg_ldpal); break;
    case 0x3: OP(meg_sprw); break;
    case 0x4: OP(meg_sprh); break;
    case 0x5: OP(meg_alpha); break;
    case 0x6: OP(meg_sndon); break;
    case 0x7: OP(meg_sndoff); break;
    case 0x8: OP(meg_bmode); break;
#elif defined(HAVE_HCHIP_SUPPORT)
    case 0x02: OP(sys_cls); break;
#endif
    default:  OP(bad); break;
    } break;
case 0x1: OP(jmp); break;
case 0x2: OP(jsr); break;
case 0x3: OP(sei); break;
case 0x4: OP(sni); break;
case 0x5: OP(ser); break;
case 0x6: OP(mov); break;
case 0x7: OP(add); break;
case 0x8:
    switch (OPCODE & 0xF) {
    case 0x0: OP(reg_mov); break;
    case 0x1: OP(reg_orl); break;
    case 0x2: OP(reg_and); break;
    case 0x3: OP(reg_xor); break;
    case 0x4: OP(reg_add); break;
    case 0x5: OP(reg_sxy); break;
    case 0x6: OP(reg_shr); break;
    case 0x7: OP(reg_syx); break;
    case 0xE: OP(reg_shl); break;
    default:  OP(bad); break;
    } break;
case 0x9: OP(snr); break;
case 0xA: OP(ldi); break;
case 0xB: OP(vjp); break;
case 0xC: OP(rnd); break;
case 0xD: OP(drw); break;
case 0xE:
    switch (OPCODE & 0xFF) {
    case 0x9E: OP(key_seq); break;
    case 0xA1: OP(key_sne); break;
    default:   OP(bad); break;
    } break;
case 0xF:
    switch (OPCODE & 0xFF) {
    case 0x07: OP(mem_rdd); break;
    case 0x0A: OP(mem_rdk); break;
    case 0x15: OP(mem_wrd); break;
    case 0x18: OP(mem_wrs); break;
    case 0x1E: OP(mem_addi); break;
    case 0x29: OP(mem_font); break;
    case 0x33: OP(mem_bcd); break;
    case 0x55: OP(mem_wr); break;
    case 0x65: OP(mem_rd); break;
#ifdef HAVE_SCHIP_SUPPORT
    case 0x30: OP(sup_xfont); break;
    case 0x75: OP(sup_wr48); break;
    case 0x85: OP(sup_rd48); break;
#endif
    default:   OP(bad); break;
    } break;
}

