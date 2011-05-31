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
#include <getopt.h>
#include <assert.h>
#include "cmdline.h"
#include "chip8.h"

#define CMDLINE_VERSION 0x1000
#define CMDLINE_BGCOLOR 0x1001
#define CMDLINE_FGCOLOR 0x1002

const char *gchip_desc  = "gchip - a portabl chip8 emulator";
const char *gchip_usage = "usage: gchip [options] [file]";
const char *gchip_ver   = "0.1-dev";

// -----------------------------------------------------------------------------
void cmdline_display_usage(void)
{
    log_info("%s\n%s\n\n", gchip_desc, gchip_usage);
    log_info("Options:\n"
        "  -d, --debugger      enable debugging interface\n"
        "  -c, --cycles        max number of cycles to execute\n"
        "  -m, --mode=MODE     specify CPU execution mode: "
#ifdef HAVE_CASE_INTERPRETER
            "case "
#endif
#ifdef HAVE_PTR_INTERPRETER
            "ptr "
#endif
#ifdef HAVE_CACHE_INTERPRETER
            "cache "
#endif
#ifdef HAVE_RECOMPILER
            "dbt "
#endif
            "test\n"
        "  -r, --rom=PATH      path to rom file\n"
        "  -s, --scale=INT     scale screen resolution\n"
        "      --bg=COLOR      specify the background color\n"
        "      --fg=COLOR      specify the foreground color\n"
        "  -f, --fullscreen    run in fullscreen mode\n"
        "  -v, --vsync         enable vertical sync\n"
        "  -S, --speed=INT     set emulator speed (instructions/second)\n"
        "  -h, --help          display this usage message\n"
        "      --autonomous    fake keypress for benchmarking\n"
        "      --headless      disable graphical display\n"
        "      --version       display program version\n");
    exit(EXIT_FAILURE);
}

// -----------------------------------------------------------------------------
void cmdline_display_version(void)
{
    log_info("gchip %s\n", gchip_ver);
    log_info("compiled %s %s\n", __DATE__, __TIME__);
    log_info("features: %s\n",
#ifdef HAVE_CASE_INTERPRETER
             "case "
#endif
#ifdef HAVE_PTR_INTERPRETER
             "ptr "
#endif
#ifdef HAVE_CACHE_INTERPRETER
             "cache "
#endif
#ifdef HAVE_RECOMPILER
             "dbt "
#endif
#ifdef HAVE_HCHIP_SUPPORT
             "hchip "
#endif
#ifdef HAVE_SCHIP_SUPPORT
             "schip "
#endif
#ifdef HAVE_MCHIP_SUPPORT
             "mchip "
#endif
             "chip8 "
            );

    exit(EXIT_FAILURE);
}

// -----------------------------------------------------------------------------
int cmdline_parse(int argc, char *argv[], cmdargs_t *args)
{
    static const char *s_opts = "dc:m:r:s:fvS:h?";
    static const struct option l_opts[] = {
        { "debugger",   no_argument,        NULL, 'd' },
        { "cycles",     required_argument,  NULL, 'c' },
        { "mode",       required_argument,  NULL, 'm' },
        { "rom",        required_argument,  NULL, 'r' },
        { "scale",      required_argument,  NULL, 's' },
        { "bg",         required_argument,  NULL, CMDLINE_BGCOLOR },
        { "fg",         required_argument,  NULL, CMDLINE_FGCOLOR },
        { "fullscreen", no_argument,        NULL, 'f' },
        { "vsync",      no_argument,        NULL, 'v' },
        { "speed",      required_argument,  NULL, 'S' },
        { "help",       no_argument,        NULL, 'h' },
        { "version",    no_argument,        NULL, CMDLINE_VERSION },
        { NULL,         no_argument,        NULL, 0 }
    };

    // set some reasonable defaults
    args->debugger = 0;
    args->max_cycles = 0;
    args->fullscreen = 0;
    args->bgcolor = 0x00000000;
    args->fgcolor = 0xFFFFFFFF;
    args->vsync = 0;
    args->speed = 1200;
    args->rompath = NULL;
    args->mode = MODE_CASE;

    int opt, index = 0, scale = 1;
    while (-1 != (opt = getopt_long(argc, argv, s_opts, l_opts, &index))) {
        switch (opt) {
        case 'd':
            args->debugger = 1;
            break;
        case 'c':
            args->max_cycles = strtol(optarg, NULL, 10);
            break;
        case 'm':
            if (!strcmp(optarg, "ptr"))
                args->mode = MODE_PTR;
            else if (!strcmp(optarg, "case"))
                args->mode = MODE_CASE;
            else if (!strcmp(optarg, "cache"))
                args->mode = MODE_CACHE;
            else if (!strcmp(optarg, "dbt"))
                args->mode = MODE_DBT;
            else if (!strcmp(optarg, "test"))
                args->mode = MODE_TEST;
            else {
                log_err("invalid mode specified (%s)\n", optarg);
                cmdline_display_usage();
            }
            break;
        case 'r':
            args->rompath = strdup(optarg);
            break;
        case 's':
            scale = strtol(optarg, NULL, 0);
            break;
        case CMDLINE_BGCOLOR:
            args->bgcolor = strtol(optarg, NULL, 0);
            break;
        case CMDLINE_FGCOLOR:
            args->fgcolor = strtol(optarg, NULL, 0);
            break;
        case 'f':
            args->fullscreen = 1;
            break;
        case 'v':
            args->vsync = 1;
            break;
        case 'S':
            args->speed = strtol(optarg, NULL, 0);
            break;
        case CMDLINE_VERSION:
            cmdline_display_version();
            break;
        case 'h':
        case '?':
            cmdline_display_usage();
            break;
        default:
            break;
        }
    }

    // check for non-option arguments. if -r isn't specified, treat as romfile
    if (optind < argc) {
        if (args->rompath || (argc - optind) > 1) {
            log_err("trailing arguments (%d)\n", argc - optind);
            return -1;
        }
        args->rompath = strdup(argv[optind]);
    }

    if (!args->rompath) {
        log_err("no rom file specified\n");
        return -1;
    }

    if (args->max_cycles < 0) {
        log_err("expected positive cycle count value\n");
        return -1;
    }

    if (args->speed < 0) {
        log_err("expected positive speed value\n");
        return -1;
    }
    
    if (scale <= 0 || scale > 20) {
        log_err("invalid scale factor specified (must be 0-20)\n");
        return -1;
    }
    args->width = 64 * scale;
    args->height = 32 * scale;

    return 0;
}

// -----------------------------------------------------------------------------
void cmdline_destroy(cmdargs_t *args)
{
    assert(NULL != args);
    SAFE_FREE(args->rompath);
}

