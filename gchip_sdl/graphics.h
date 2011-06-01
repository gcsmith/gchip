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

#ifndef GCHIP_GRAPHICS__H
#define GCHIP_GRAPHICS__H

#include <GL/glew.h>
#include "common.h"

typedef struct graphics {
    GLuint pbo, texture;
    int pbo_sz;
    int texture_dim;
    int bg, fg;
} graphics_t;

void graphics_init(graphics_t *gfx, int width, int height, int bg, int fg);
void graphics_update(graphics_t *gfx, uint8_t *src, int system);
void graphics_render(graphics_t *gfx, int system);

// -----------------------------------------------------------------------------
// Return the nearest square, pow2 texture dimension >= MAX(width, height).
INLINE unsigned int tex_pow2(unsigned int width, unsigned int height)
{
    unsigned int input = MAX(width, height);
    unsigned int value = 2;

    if (0 == (input & (input - 1)))
        return input;

    while (0 != (input >>= 1))
        value <<= 1;
    return value;
}

#endif // GCHIP_GRAPHICS__H

