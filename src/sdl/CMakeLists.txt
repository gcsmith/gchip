# ------------------------------------------------------------------------------
# Author:  Garrett Smith
# File:    src/sdl/CMakeLists.txt
# Created: 05/24/2011
# ------------------------------------------------------------------------------

project(gchip_sdl)

find_package(GLEW   REQUIRED)
find_package(OpenGL REQUIRED)
find_package(SDL2   REQUIRED)

include_directories(
    ${GLEW_INCLUDE_DIR}
    ${OPENGL_INCLUDE_DIR}
    ${SDL2_INCLUDE_DIR}
)

add_definitions(${GLEW_DEFINITIONS})

set(gchip_sdl_hdr audio.h graphics.h)
set(gchip_sdl_src audio.c graphics.c main.c)
set(gchip_sdl_lib ${GLEW_LIBRARIES} ${OPENGL_LIBRARIES} ${SDL2_LIBRARY} gchip)

add_executable(gchip_sdl ${MKPROP} ${gchip_sdl_src} ${gchip_sdl_hdr})
target_link_libraries(gchip_sdl ${gchip_sdl_lib})

