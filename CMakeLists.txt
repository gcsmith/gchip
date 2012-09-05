# ------------------------------------------------------------------------------
# Author:  Garrett Smith
# File:    CMakeLists.cmake
# Created: 05/24/2011
# ------------------------------------------------------------------------------

cmake_minimum_required(VERSION 2.8.4 FATAL_ERROR)
project(gchip)

# set default project options

option(BUILD_EGL "Build the gchip-egl frontend" OFF)
option(BUILD_SDL "Build the gchip-sdl frontend" ON)

option(HAVE_HCHIP_SUPPORT "Build with Chip-8 HiRes support" ON)
option(HAVE_SCHIP_SUPPORT "Build with SuperChip-8 support"  ON)
option(HAVE_MCHIP_SUPPORT "Build with MegaChip-8 support"   ON)

option(HAVE_CASE_INTERPRETER  "Build with case-based interpreter"    ON)
option(HAVE_PTR_INTERPRETER   "Build with pointer-based interpreter" ON)
option(HAVE_CACHE_INTERPRETER "Build with caching interpreter"       ON)
option(HAVE_RECOMPILER        "Build with recompiler support"        ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_BUILD_TYPE)

set(GCHIP_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(CMAKE_MODULE_PATH ${GCHIP_MODULE_PATH} ${CMAKE_MODULE_PATH})
set(GNULIB_DIR "${CMAKE_SOURCE_DIR}/gnulib")

# detect the platform and architecture

include(${GCHIP_MODULE_PATH}/DetectPlatform.cmake)
include(${GCHIP_MODULE_PATH}/DetectArchitecture.cmake)

# encode version number as <major>.<minor>.<revision>:<changeset>

set(GCHIP_VER_MAJ "0")
set(GCHIP_VER_MIN "0")
set(GCHIP_VER_REV "1")

find_package(Git)
if(GIT_FOUND)
    GIT_WC_INFO(${PROJECT_SOURCE_DIR} GCHIP)
    set(GCHIP_VER_ID ${GCHIP_WC_REVISION_HASH})
endif(GIT_FOUND)

set(BUILD_VERSION "${GCHIP_VER_MAJ}.${GCHIP_VER_MIN}.${GCHIP_VER_REV}:${GCHIP_VER_ID}")

# perform configuration and generate config.h file

include(CheckIncludeFiles)
check_include_files(intrin.h HAVE_INTRIN_H)
check_include_files(stdint.h HAVE_STDINT_H)
check_include_files(windows.h HAVE_WINDOWS_H)

include(CheckFunctionExists)
check_function_exists(clock_gettime HAVE_CLOCK_GETTIME)

configure_file(
    ${CMAKE_SOURCE_DIR}/gchip/config.h.cmake
    ${CMAKE_BINARY_DIR}/gchip/config.h
)

include_directories(
    "${CMAKE_SOURCE_DIR}/gchip"
    "${CMAKE_BINARY_DIR}/gchip"
)

message(STATUS "--------------------------------------------------------------")
message(STATUS "CMAKE_BUILD_TYPE:       ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_INSTALL_PREFIX:   ${CMAKE_INSTALL_PREFIX}")
message(STATUS "CMAKE_VERBOSE_MAKEFILE: ${CMAKE_VERBOSE_MAKEFILE}")
message(STATUS "BUILD_EGL:              ${BUILD_EGL}")          
message(STATUS "BUILD_SDL:              ${BUILD_SDL}")
message(STATUS "BUILD_VERSION:          ${BUILD_VERSION}")
message(STATUS "--------------------------------------------------------------")

set(BUILD_EGL ${BUILD_EGL} CACHE BOOL
    "Build the gchip-egl frontend" FORCE)
set(BUILD_SDL ${BUILD_SDL} CACHE BOOL
    "Build the gchip-sdl frontend" FORCE)
set(HAVE_HCHIP_SUPPORT ${HAVE_HCHIP_SUPPORT} CACHE BOOL
    "Build with Chip-8 HiRes support" FORCE)
set(HAVE_SCHIP_SUPPORT ${HAVE_SCHIP_SUPPORT} CACHE BOOL
    "Build with SuperChip-8 support" FORCE)
set(HAVE_MCHIP_SUPPORT ${HAVE_MCHIP_SUPPORT} CACHE BOOL
    "Build with MegaChip-8 support" FORCE)
set(HAVE_CASE_INTERPRETER ${HAVE_CASE_INTERPRETER} CACHE BOOL 
    "Build with case-based interpreter" FORCE)
set(HAVE_PTR_INTERPRETER ${HAVE_PTR_INTERPRETER} CACHE BOOL
    "Build with pointer-based interpreter" FORCE)
set(HAVE_CACHE_INTERPRETER ${HAVE_CACHE_INTERPRETER} CACHE BOOL
    "Build with caching interpreter" FORCE)
set(HAVE_RECOMPILER ${HAVE_RECOMPILER} CACHE BOOL
    "Build with recompiler support" FORCE)
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING
    "Choose type of build: None Debug Release RelWithDebInfo MinSizeRel" FORCE)

# add each sub-project
add_subdirectory(gchip)

if(BUILD_EGL)
    add_subdirectory(gchip_egl)
endif(BUILD_EGL)

if(BUILD_SDL)
    add_subdirectory(gchip_sdl)
endif(BUILD_SDL)

