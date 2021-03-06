######################################################################################
# CMake directives
######################################################################################

#Require 2.6 or higher. 
CMAKE_MINIMUM_REQUIRED(VERSION 2.6.0 FATAL_ERROR)

INCLUDE( ${CMAKE_SOURCE_DIR}/BuildSysCMakeLib.cmake )

#setting link directory policy
IF(COMMAND cmake_policy)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND cmake_policy)

######################################################################################
# Project declaration and options
######################################################################################

#Project declaration

PROJECT(LIBOMRON)

# Project initialization
INITIALIZE_BUILD()

#Common Options

OPTION_LIBRARY_BUILD_STATIC(ON)
OPTION_LIBRARY_BUILD_SHARED(ON)
IF(WIN32)
  OPTION_USE_STATIC_SUFFIX(ON)
ENDIF()
IF(APPLE)
  OPTION_LIBRARY_BUILD_FRAMEWORK(OFF)
ENDIF()
OPTION_BUILD_RPATH(ON)

######################################################################################
# Project specific globals
######################################################################################

#library definitions

SET(LIBOMRON_MAJOR_VERSION 0)
SET(LIBOMRON_MINOR_VERSION 9)
SET(LIBOMRON_BUILD_VERSION 0)

SET(LIBOMRON_VERSION ${LIBOMRON_MAJOR_VERSION}.${LIBOMRON_MINOR_VERSION}.${LIBOMRON_BUILD_VERSION})

CREATE_LIBRARY_LINK_NAME("omron")

# Just set our include and linking directories globally
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)
LINK_DIRECTORIES(${CMAKE_BINARY_DIR}/lib)

#library name definitions
SET(LIBOMRON_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/include")
SET(LIBOMRON_REQUIRED_LIBS)

IF(WIN32)
  SET(WDK_PATH CACHE PATH "Path to WDK Installation")
  INCLUDE_DIRECTORIES(${WDK_PATH}/inc/api ${WDK_PATH}/inc/crt ${WDK_PATH}/inc)
  LINK_DIRECTORIES(${WDK_PATH}/lib/wxp/i386)
  LIST(APPEND LIBOMRON_REQUIRED_LIBS hid setupapi)
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include/win)
ELSEIF(UNIX)
  FIND_PACKAGE(libusb-1.0 REQUIRED)
  IF(LIBUSB_1_FOUND)
    INCLUDE_DIRECTORIES(${LIBUSB_1_INCLUDE_DIRS})
    LIST(APPEND LIBOMRON_REQUIRED_LIBS ${LIBUSB_1_LIBRARIES})
  ENDIF(LIBUSB_1_FOUND)
ENDIF(WIN32)

######################################################################################
# Installation of headers
######################################################################################

SET(LIBOMRON_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/include/libomron
)

INSTALL(DIRECTORY 
  ${LIBOMRON_INCLUDE_DIRS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/libomron
  )

FOREACH(DIR ${LIBOMRON_INCLUDE_DIRS})
  FILE(GLOB_RECURSE HEADER_FILES ${DIR}/*.h)
  LIST(APPEND LIBOMRON_INCLUDE_FILES ${HEADER_FILES})
ENDFOREACH(DIR ${LIBOMRON_INCLUDE_DIRS})

IF(BUILD_SHARED AND WIN32)
  ADD_DEFINITIONS(-DOMRON_DYNAMIC)
ENDIF()

######################################################################################
# Subdirectories
######################################################################################

ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(examples)
ADD_SUBDIRECTORY(swig)
ADD_SUBDIRECTORY(python)
