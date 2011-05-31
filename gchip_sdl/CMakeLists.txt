# ------------------------------------------------------------------------------
# Author:  Garrett Smith
# File:    gchip_sdl/CMakeLists.txt
# Created: 05/24/2011
# ------------------------------------------------------------------------------

project(gchip_sdl)

find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)
find_package(SDL REQUIRED)

include_directories(
    ${GLEW_INCLUDE_DIR}
    ${OPENGL_INCLUDE_DIR}
    ${SDL_INCLUDE_DIR}
)

add_definitions(
    ${GLEW_DEFINITIONS}
)

set(gchip_sdl_hdr
    audio.h
    graphics.h
    util.h
)

set(gchip_sdl_src
    audio.c
    graphics.c
    main.c
)

set(gchip_sdl_lib
    ${GLEW_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${SDL_LIBRARY}
    gchip
)

if(WIN32)
    # use gnulib for windows portability
    list(APPEND gchip_sdl_hdr
        ${GNULIB_DIR}/getopt.h
        ${GNULIB_DIR}/getopt_int.h
        ${GNULIB_DIR}/gettext.h
    )
    list(APPEND gchip_sdl_src
        ${GNULIB_DIR}/getopt.c
        ${GNULIB_DIR}/getopt1.c
    )
    include_directories(${GNULIB_DIR})
endif()

add_executable(gchip_sdl ${MKPROP} ${gchip_sdl_src} ${gchip_sdl_hdr})
target_link_libraries(gchip_sdl ${gchip_sdl_lib})

