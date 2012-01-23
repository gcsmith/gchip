# ------------------------------------------------------------------------------
# Author:  Garrett Smith
# File:    cmake/Modules/DetectPlatform.cmake
# Created: 05/29/2011
# ------------------------------------------------------------------------------

if(WIN32)
    message(STATUS "Building for platform: WIN32")
    set(MKPROP WIN32)
    set(PLATFORM_WIN32 1)
    add_definitions(
        -DPLATFORM_WIN32
        -D_CRT_SECURE_NO_WARNINGS
        -D_CRT_SECURE_NO_DEPRECATE
        -W2
    )
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /TP")
elseif(UNIX)
    message(STATUS "Building for platform: UNIX")
    set(MKPROP "")
    set(PLATFORM_UNIX 1)
    add_definitions(
        -DPLATFORM_UNIX
        -D_GNU_SOURCE
        -Wall -Werror
    )
set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} rt m)
else()
    message(FATAL_ERROR "Unsupported platform detected")
endif()

