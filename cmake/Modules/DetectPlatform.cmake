# ------------------------------------------------------------------------------
# Author:  Garrett Smith
# File:    cmake/Modules/DetectPlatform.cmake
# Created: 05/29/2011
# ------------------------------------------------------------------------------

if(WIN32)
    message(STATUS "Building for platform: win32")
    set(MKPROP WIN32)
    set(PLATFORM_WIN32 1)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_DEPRECATE -W2)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /TP")
elseif(APPLE)
    message(STATUS "Building for platform: apple")
    set(MKPROP MACOSX_BUNDLE)
    set(PLATFORM_APPLE 1)
    add_definitions(-Wall -Werror)
    set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} rt m)
elseif(UNIX)
    message(STATUS "Building for platform: unix")
    set(MKPROP "")
    set(PLATFORM_UNIX 1)
    add_definitions(-D_GNU_SOURCE -Wall -Werror)
    set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} rt m)
else()
    message(FATAL_ERROR "Unsupported platform detected")
endif()

