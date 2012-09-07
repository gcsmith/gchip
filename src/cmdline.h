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

#ifndef GCHIP_CMDLINE__H
#define GCHIP_CMDLINE__H

extern const char *gchip_desc;
extern const char *gchip_usage;
extern const char *gchip_ver;

typedef struct cmdargs {
    int debugger;       // enable debugging interface
    int max_cycles;     // set maximum number of cycles to execute
    int fullscreen;     // start in fullscreen mode
    int scale;          // window size scaling factor
    int bgcolor;        // background color
    int fgcolor;        // foreground color
    int vsync;          // enable vertical sync
    int speed;          // control emulator speed (instructions / second)
    int mode;           // TODO: refactor me
    char *rompath;      // path to rom image
} cmdargs_t;

int cmdline_parse(int argc, char *argv[], cmdargs_t *args);
void cmdline_destroy(cmdargs_t *args);
void cmdline_display_usage(void);
void cmdline_display_version(void);

#endif // GCHIP_CMDLINE__H

