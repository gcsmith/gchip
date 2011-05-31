# ------------------------------------------------------------------------------
# Author:  Garrett Smith
# File:    gchip_egl/CMakeLists.txt
# Created: 05/28/2011
# ------------------------------------------------------------------------------

project(gchip_egl)

find_package(OpenGLES REQUIRED)

include_directories(
    ${OPENGLES_INCLUDE_DIR}
    ../gchip_sdl
)

add_definitions(${OPENGLES_DEFINITIONS})

set(gchip_egl_hdr
)

set(gchip_egl_src
    main.c
)

set(gchip_egl_lib
    ${OPENGLES_LIBRARIES}
    gchip
)

if(WIN32)
    # use gnulib for windows portability
    list(APPEND gchip_egl_hdr
        ${GNULIB_DIR}/getopt.h
        ${GNULIB_DIR}/getopt_int.h
        ${GNULIB_DIR}/gettext.h
    )
    list(APPEND gchip_egl_src
        ${GNULIB_DIR}/getopt.c
        ${GNULIB_DIR}/getopt1.c
    )
    include_directories("${CMAKE_SOURCE_DIR}/gnulib")

    # tell MSVC to compile as C++ for compatibility with C99 features
    set_source_files_properties(${gchip_src} PROPERTIES LANGUAGE CXX)
endif()

add_executable(gchip_egl ${MKPROP} ${gchip_egl_src} ${gchip_egl_hdr})
target_link_libraries(gchip_egl ${gchip_egl_lib})

