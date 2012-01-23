# ------------------------------------------------------------------------------
# Author:  Garrett Smith
# File:    cmake/Modules/DetectArchitecture.cmake
# Created: 05/29/2011
# ------------------------------------------------------------------------------

if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64*")
    message(STATUS "Building for architecture: x86_64")
    add_definitions(-DARCH_X86_64)
    set(ARCH_X86_64 1)
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86*" OR
       ${CMAKE_SYSTEM_PROCESSOR} MATCHES "[iI][346]86")
    # need to check pointer size, since x86 will be reported under win32/64
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(STATUS "Building for architecture: x86_64")
        add_definitions(-DARCH_X86_64)
        set(ARCH_X86_64 1)
    else()
        message(STATUS "Building for architecture: x86")
        add_definitions(-DARCH_X86)
        set(ARCH_X86 1)
    endif()
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm*")
    message(STATUS "Building for architecture: arm")
    add_definitions(-DARCH_ARM)
    set(ARCH_OPTIMIZATIONS "-mcpu=cortex-a8 -mtune=cortex-a8 -mfpu=neon")
    set(ARCH_ARM 1)
else()
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ARCH_OPTIMIZATIONS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ARCH_OPTIMIZATIONS}")

