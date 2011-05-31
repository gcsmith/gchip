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
#include <math.h>
#include <string.h>
#include "chip8.h"
#include "graphics.h"

static const int emu_width = 256, emu_height = 192;

// -----------------------------------------------------------------------------
void graphics_init(graphics_t *gfx, int width, int height, int bg, int fg)
{
    gfx->pbo_sz = emu_width * emu_height * 4;
    gfx->texture_dim = tex_pow2(emu_width, emu_height);
    gfx->fg = fg;
    gfx->bg = bg;

    glShadeModel(GL_FLAT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);

    glViewport(0, 0, width, height);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // perform opengl initialization
    glDeleteTextures(1, &gfx->texture);
    glGenTextures(1, &gfx->texture);
    glBindTexture(GL_TEXTURE_2D, gfx->texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 gfx->texture_dim, gfx->texture_dim,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glGenBuffers(1, &gfx->pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gfx->pbo);
}

// -----------------------------------------------------------------------------
void graphics_update(graphics_t *gfx, uint8_t *src, int system)
{
    // lock the pbo for write only access
    glBufferData(GL_PIXEL_UNPACK_BUFFER, gfx->pbo_sz, NULL, GL_DYNAMIC_DRAW);
    void *pbuffer = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    // memset(pbuffer, 0, pbo_sz);

    uint8_t *surf = (uint8_t *)pbuffer;
    if (system == SYSTEM_MCHIP) {
        uint32_t *src32 = (uint32_t *)src;
        for (int y = 0; y < MCHIP_YRES; ++y) {
            uint32_t *line = (uint32_t *)&surf[y * gfx->texture_dim * 4];
            for (int x = 0; x < MCHIP_XRES; ++x) {
                line[x] = src32[y * MCHIP_XRES + x];
            }
        }
    }
    else {
        for (int y = 0; y < SCHIP_YRES; ++y) {
            uint32_t *line = (uint32_t *)&surf[y * gfx->texture_dim * 4];
            for (int x = 0; x < SCHIP_XRES; ++x) {
                line[x] = src[y * SCHIP_XRES + x] ? gfx->fg : gfx->bg;
            }
        }
    }

    // unlock the pbo
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
            emu_width, emu_height, GL_RGBA, GL_UNSIGNED_BYTE, 0);
}

// -----------------------------------------------------------------------------
void graphics_render(graphics_t *gfx, int system)
{
    int width = SCHIP_XRES, height = SCHIP_YRES;
    if (system == SYSTEM_MCHIP) {
        width = MCHIP_XRES;
        height = MCHIP_YRES;
    }

    float tex_width  = width / (float)gfx->texture_dim;
    float tex_height = height / (float)gfx->texture_dim;

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, tex_height);
    glVertex3f(-1.0f, -1.0f, 0.0f); 
    glTexCoord2f(tex_width, tex_height);
    glVertex3f( 1.0f, -1.0f, 0.0f); 
    glTexCoord2f(tex_width, 0.0f);
    glVertex3f( 1.0f,  1.0f, 0.0f); 
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.0f,  1.0f, 0.0f);
    glEnd();
}

