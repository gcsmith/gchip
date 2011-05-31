# Locate OpenGL Extension Wrangler
# This module defines
#  GLEW_LIBRARIES, the libraries to link against
#  GLEW_INCLUDE_DIR, where to find the GLEW headers
#  GLEW_DEFINITIONS, compiler definitions required to use GLEW
#  GLEW_FOUND, if false, do not try to link to GLEW
#

FIND_PATH(GLEW_INCLUDE_DIR GL/glew.h
  HINTS
  $ENV{GLEW_DIR}
  PATH_SUFFIXES include/GL include
  PATHS
  ~/Library/Frameworks
  /Library/Frameworks
  /usr/local
  /usr
  /opt/local
  /opt
)

FIND_LIBRARY(GLEW_LIBRARIES
  NAMES GLEW glew glew32
  HINTS
  $ENV{GLEW_DIR}
  PATH_SUFFIXES lib64 lib
  PATHS
  ~/Library/Frameworks
  /Library/Frameworks
  /usr/local
  /usr
  /opt/local
  /opt
)

IF(GLEW_INCLUDE_DIR AND GLEW_LIBRARIES)
   SET(GLEW_FOUND TRUE)
ENDIF(GLEW_INCLUDE_DIR AND GLEW_LIBRARIES)

IF(GLEW_FOUND)
  IF(NOT GLEW_FIND_QUIETLY)
    MESSAGE(STATUS "Found GLEW: ${GLEW_LIBRARIES}")
  ENDIF(NOT GLEW_FIND_QUIETLY)
ELSE(GLEW_FOUND)
  IF(GLEW_FIND_REQUIRED)
    MESSAGE(FATAL_ERROR "Could not find GLEW")
  ENDIF(GLEW_FIND_REQUIRED)
ENDIF(GLEW_FOUND)

# show the GLEW_INCLUDE_DIR and GLEW_LIBRARIES variables only in the advanced view
MARK_AS_ADVANCED(GLEW_INCLUDE_DIR GLEW_LIBRARIES)

