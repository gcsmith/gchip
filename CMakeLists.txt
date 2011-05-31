# ------------------------------------------------------------------------------
# Author:  Garrett Smith
# File:    CMakeLists.cmake
# Created: 05/24/2011
# ------------------------------------------------------------------------------

cmake_minimum_required(VERSION 2.6 FATAL_ERROR)
project(gchip)

# set(CMAKE_VERBOSE_MAKEFILE ON)
# set(CMAKE_BUILD_TYPE Release)

option(BUILD_EGL "Build the gchip-egl frontend" OFF)
option(BUILD_SDL "Build the gchip-sdl frontend" ON)

option(HAVE_HCHIP_SUPPORT "Build with Chip-8 HiRes support" ON)
option(HAVE_SCHIP_SUPPORT "Build with SuperChip-8 support" ON)
option(HAVE_MCHIP_SUPPORT "Build with MegaChip-8 support" ON)

option(HAVE_CASE_INTERPRETER "Build with case-based interpreter" ON)
option(HAVE_PTR_INTERPRETER "Build with pointer-based interpreter" ON)
option(HAVE_CACHE_INTERPRETER "Build with caching interpreter" ON)
option(HAVE_RECOMPILER "Build with recompiler support" ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Choose type of build: None Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
endif(NOT CMAKE_BUILD_TYPE)

set(GCHIP_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(CMAKE_MODULE_PATH ${GCHIP_MODULE_PATH} ${CMAKE_MODULE_PATH})
set(GNULIB_DIR "${CMAKE_SOURCE_DIR}/gnulib")

include(${GCHIP_MODULE_PATH}/DetectPlatform.cmake)
include(${GCHIP_MODULE_PATH}/DetectArchitecture.cmake)

configure_file(
    ${CMAKE_SOURCE_DIR}/gchip/config.h.cmake
    ${CMAKE_BINARY_DIR}/gchip/config.h
)

include_directories(
    "${CMAKE_SOURCE_DIR}/gchip"
    "${CMAKE_BINARY_DIR}/gchip"
)

# add each sub-project
add_subdirectory(gchip)

if(BUILD_EGL)
    add_subdirectory(gchip_egl)
endif(BUILD_EGL)

if(BUILD_SDL)
    add_subdirectory(gchip_sdl)
endif(BUILD_SDL)

