# Locate OpenGL ES
# This module defines
#  OPENGLES_INCLUDE_DIR, the include directory for OpenGL ES
#  OPENGLES_LIBRARIES, the OpenGL ES library
#  OPENGLES_FOUND, system has OpenGL ES package installed
#

FIND_PATH(OPENGLES_INCLUDE_DIR GLES/gl.h
  HINTS
  $ENV{OPENGLES_DIR}
  PATH_SUFFIXES include/GLES include
  PATHS
  /usr/openwin/share
  /opt/graphics/OpenGL
  /usr/X11R6
  /usr/local
  /usr
  /opt/local
  /opt
)

FIND_LIBRARY(OPENGLES_GLES_LIBRARY
  NAMES GLES_CM
  HINTS
  $ENV{OPENGLES_DIR}
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

FIND_LIBRARY(OPENGLES_EGL_LIBRARY
  NAMES EGL
  HINTS
  $ENV{OPENGLES_DIR}
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

IF(OPENGLES_INCLUDE_DIR AND OPENGLES_GLES_LIBRARY AND OPENGLES_EGL_LIBRARY)
    SET(OPENGLES_LIBRARIES ${OPENGLES_GLES_LIBRARY} ${OPENGLES_EGL_LIBRARY})
    SET(OPENGLES_FOUND TRUE)
ENDIF(OPENGLES_INCLUDE_DIR AND OPENGLES_GLES_LIBRARY AND OPENGLES_EGL_LIBRARY)

IF(OPENGLES_FOUND)
    MESSAGE(STATUS "Found OpenGLES: ${OPENGLES_LIBRARIES}")
ELSE(OPENGLES_FOUND)
    MESSAGE(FATAL_ERROR "Could not find OpenGLES")
ENDIF(OPENGLES_FOUND)

