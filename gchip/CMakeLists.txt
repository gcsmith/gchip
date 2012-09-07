# ------------------------------------------------------------------------------
# Author:  Garrett Smith
# File:    gchip/CMakeLists.txt
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
        ${GNULIB_DIR}/getopt.h
        ${GNULIB_DIR}/getopt_int.h
        ${GNULIB_DIR}/gettext.h
    )
    list(APPEND gchip_src
        ${GNULIB_DIR}/getopt.c
        ${GNULIB_DIR}/getopt1.c
    )
    include_directories(${GNULIB_DIR})
endif(NOT HAVE_GETOPT_H)

if(HAVE_RECOMPILER)
    list(APPEND gchip_src xlat.c)
    if(ARCH_X86 OR ARCH_X86_64)
        list(APPEND gchip_src xlat_x86.c)
    endif(ARCH_X86 OR ARCH_X86_64)
endif(HAVE_RECOMPILER)

add_library(gchip ${gchip_src})

