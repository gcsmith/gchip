# ------------------------------------------------------------------------------
# Author:  Garrett Smith
# File:    src/CMakeLists.txt
# Created: 05/24/2011
# ------------------------------------------------------------------------------

project(libgchip_project)

set(gchip_hdr
    chip8.h
    cmdline.h
    common.h
    xlat.h
)

set(gchip_src
    chip8.c
    cmdline.c
    debugger.c
    interp.c
)

if(NOT HAVE_GETOPT_H)
    list(APPEND gchip_hdr
        gnulib/getopt.h
        gnulib/getopt_int.h
        gnulib/gettext.h
    )
    list(APPEND gchip_src
        gnulib/getopt.c
        gnulib/getopt1.c
    )
    include_directories(gnulib)
endif(NOT HAVE_GETOPT_H)

if(HAVE_RECOMPILER)
    list(APPEND gchip_src xlat.c)
    if(ARCH_X86 OR ARCH_X86_64)
        list(APPEND gchip_src xlat_x86.c)
    endif(ARCH_X86 OR ARCH_X86_64)
endif(HAVE_RECOMPILER)

add_library(gchip ${gchip_src})

# add each sub-directory

if(BUILD_EGL)
    add_subdirectory(egl)
endif(BUILD_EGL)

if(BUILD_SDL)
    add_subdirectory(sdl)
endif(BUILD_SDL)

