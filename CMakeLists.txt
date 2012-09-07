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
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Build type: None Debug Release RelWithDebInfo MinSizeRel" FORCE)
endif(NOT CMAKE_BUILD_TYPE)

set(GCHIP_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(CMAKE_MODULE_PATH ${GCHIP_MODULE_PATH} ${CMAKE_MODULE_PATH})

# detect the platform and architecture

include(${GCHIP_MODULE_PATH}/DetectPlatform.cmake)
include(${GCHIP_MODULE_PATH}/DetectArchitecture.cmake)

# encode version number as <major>.<minor>.<patch>:<changeset>

set(GCHIP_VERSION_MAJOR "0")
set(GCHIP_VERSION_MINOR "0")
set(GCHIP_VERSION_PATCH "1")

find_package(Git)
if(GIT_FOUND)
    GIT_WC_INFO(${PROJECT_SOURCE_DIR} GCHIP)
    set(GCHIP_VERSION_EXTRA ${GCHIP_WC_REVISION_HASH})
endif(GIT_FOUND)

set(BUILD_VERSION "${GCHIP_VERSION_MAJOR}.${GCHIP_VERSION_MINOR}.${GCHIP_VERSION_PATCH}:${GCHIP_VERSION_EXTRA}")

# perform configuration and generate config.h file

include(CheckIncludeFiles)
check_include_files(getopt.h HAVE_GETOPT_H)
check_include_files(intrin.h HAVE_INTRIN_H)
check_include_files(stdint.h HAVE_STDINT_H)
check_include_files(windows.h HAVE_WINDOWS_H)

include(CheckFunctionExists)
check_function_exists(clock_gettime HAVE_CLOCK_GETTIME)

configure_file(
    ${CMAKE_SOURCE_DIR}/src/config.h.in
    ${CMAKE_BINARY_DIR}/src/config.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src
)

message(STATUS "--------------------------------------------------------------")
message(STATUS "CMAKE_BUILD_TYPE:       ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_INSTALL_PREFIX:   ${CMAKE_INSTALL_PREFIX}")
message(STATUS "CMAKE_VERBOSE_MAKEFILE: ${CMAKE_VERBOSE_MAKEFILE}")
message(STATUS "BUILD_VERSION:          ${BUILD_VERSION}")
message(STATUS "BUILD_EGL:              ${BUILD_EGL}")          
message(STATUS "BUILD_SDL:              ${BUILD_SDL}")
message(STATUS "HAVE_HCHIP_SUPPORT:     ${HAVE_HCHIP_SUPPORT}")
message(STATUS "HAVE_SCHIP_SUPPORT:     ${HAVE_SCHIP_SUPPORT}")
message(STATUS "HAVE_MCHIP_SUPPORT:     ${HAVE_MCHIP_SUPPORT}")
message(STATUS "--------------------------------------------------------------")

# add each sub-directory

add_subdirectory(src)

