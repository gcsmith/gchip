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

#ifndef GCHIP_COMMON__H
#define GCHIP_COMMON__H

#include <stddef.h>
#include "config.h"

#define SAFE_FREE(x) if (NULL != x) { free(x); x = NULL; }

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#ifdef _MSC_VER

// Windows/MSVC specific definitions

#define INLINE      __inline
#define FORCEINLINE __forceinline
#define FASTCALL    __fastcall

typedef signed char      int8_t;
typedef signed short     int16_t;
typedef signed int       int32_t;
typedef signed long long int64_t;

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

#define snprintf _snprintf
#define strdup _strdup

#else

// UNIX specific definitions

#define INLINE      static __inline
#define FORCEINLINE __attribute__((always_inline))
#define FASTCALL    // __attribute__((fastcall))

#include <stdint.h>

#endif // _MSC_VER

#ifdef HAVE_RECOMPILER
void *low_malloc(size_t length);
void *low_calloc(size_t length);
void *low_realloc(void *p, size_t length);
void  low_free(void *p);
#else
#define low_malloc(length) malloc(length)
#define low_calloc(length) calloc(1, length)
#define low_realloc(p, length) realloc(p, length)
#define low_free(p) free(p)
#endif

#include <stdarg.h>

INLINE void log_info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
}

INLINE void log_dbg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

INLINE void log_err(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

INLINE void log_spew(const char *fmt, ...)
{
#if 0
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
#endif
}

#endif // GCHIP_COMMON__H

