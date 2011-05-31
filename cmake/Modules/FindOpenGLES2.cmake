# Locate OpenGL ES2.0
# This module defines
#  OPENGLES2_INCLUDE_DIR, the include directory for OpenGL ES2.0
#  OPENGLES2_LIBRARIES, the OpenGL ES2.0 library
#  OPENGLES2_FOUND, system has OpenGL ES2.0 package installed
#

FIND_PATH(OPENGLES2_INCLUDE_DIR GLES2/gl2.h
  HINTS
  $ENV{OPENGLES2_DIR}
  PATH_SUFFIXES include/GLES2 include
  PATHS
  /usr/openwin/share
  /opt/graphics/OpenGL
  /usr/X11R6
  /usr/local
  /usr
  /opt/local
  /opt
)

FIND_LIBRARY(OPENGLES2_LIBRARIES
  NAMES GLESv2
  HINTS
  $ENV{OPENGLES2_DIR}
  PATH_SUFFIXES lib64 lib
  PATHS
  /opt/graphics/OpenGL
  /usr/openwin
  /usr/shlib
  /usr/X11R6
  /usr/local
  /usr
  /opt/local
  /opt
)

IF(OPENGLES2_INCLUDE_DIR AND OPENGLES2_LIBRARIES)
    SET(OPENGLES2_FOUND TRUE)
ENDIF(OPENGLES2_INCLUDE_DIR AND OPENGLES2_LIBRARIES)

IF(OPENGLES2_FOUND)
    MESSAGE(STATUS "Found OpenGLES2: ${OPENGLES2_LIBRARIES}")
ELSE(OPENGLES2_FOUND)
    MESSAGE(FATAL_ERROR "Could not find OpenGLES2")
ENDIF(OPENGLES2_FOUND)


