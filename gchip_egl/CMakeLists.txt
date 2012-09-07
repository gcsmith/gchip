# ------------------------------------------------------------------------------
# Author:  Garrett Smith
# File:    gchip_egl/CMakeLists.txt
# Created: 05/28/2011
# ------------------------------------------------------------------------------

project(gchip_egl)

find_package(OpenGLES REQUIRED)

include_directories(${OPENGLES_INCLUDE_DIR} ../gchip_sdl)
add_definitions(${OPENGLES_DEFINITIONS})

set(gchip_egl_hdr )
set(gchip_egl_src main.c)
set(gchip_egl_lib ${OPENGLES_LIBRARIES} gchip)

add_executable(gchip_egl ${MKPROP} ${gchip_egl_src} ${gchip_egl_hdr})
target_link_libraries(gchip_egl ${gchip_egl_lib})

