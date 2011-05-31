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

#ifndef CONFIG__H
#define CONFIG__H

#cmakedefine HAVE_HCHIP_SUPPORT
#cmakedefine HAVE_SCHIP_SUPPORT
#cmakedefine HAVE_MCHIP_SUPPORT

#cmakedefine HAVE_CASE_INTERPRETER
#cmakedefine HAVE_PTR_INTERPRETER
#cmakedefine HAVE_CACHE_INTERPRETER
#cmakedefine HAVE_RECOMPILER

#endif // CONFIG__H

