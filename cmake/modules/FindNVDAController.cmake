# FindNVDAController.cmake - Find and set up the NVDA Controller Client library
#
# SPDX-License-Identifier: MPL-2.0
#
# This module defines the following variables:
#
# NVDA_CONTROLLER_FOUND - True if NVDA controller was found
#
# NVDA_CONTROLLER_INCLUDE_DIRS - NVDA controller include directories
#
# NVDA_CONTROLLER_LIBRARIES - NVDA controller libraries
#
# This module defines the following imported targets:
#
# NVDA::Controller - The NVDA controller library

include(CMakeDependentOption)
include(FetchContent)
include(${CPM_DOWNLOAD_LOCATION})
set(NVDA_CONTROLLER_VERSION
    "2025.3.2"
    CACHE STRING "Version of the NVDA controller client to download")
set(NVDA_CONTROLLER_URL
    "https://download.nvaccess.org/releases/stable/nvda_${NVDA_CONTROLLER_VERSION}_controllerClient.zip"
    CACHE STRING "URL to download NVDA controller client from")
set(NVDA_CONTROLLER_HASH
    "177fe3f2fa92911806939e41f961411ce6f851069b574d15529854f3ed6d1bf8"
    CACHE STRING "Expected hash of the NVDA controller client zip file")
message(STATUS "Downloading NVDA Controller Client from ${NVDA_CONTROLLER_URL}")
cpmaddpackage(
  NAME
  NVDAController
  VERSION
  ${NVDA_CONTROLLER_VERSION}
  URL
  ${NVDA_CONTROLLER_URL}
  DOWNLOAD_ONLY
  YES
  URL_HASH
  "SHA256=${NVDA_CONTROLLER_HASH}")
if(NVDAController_ADDED)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
      set(NVDA_ARCH_DIR "arm64")
    else()
      set(NVDA_ARCH_DIR "x64")
    endif()
  else()
    set(NVDA_ARCH_DIR "x86")
  endif()
  message(
    STATUS "Using NVDA Controller Client for architecture: ${NVDA_ARCH_DIR}")
  set(NVDA_CONTROLLER_DIR "${NVDAController_SOURCE_DIR}/${NVDA_ARCH_DIR}")
  set(NVDA_CONTROLLER_INCLUDE_DIRS "${NVDA_CONTROLLER_DIR}")
  find_library(
    NVDA_CONTROLLER_LIBRARY
    NAMES nvdaControllerClient nvdaControllerClient64 nvdaControllerClient32
    PATHS "${NVDA_CONTROLLER_DIR}"
    NO_DEFAULT_PATH)
  set(NVDA_CONTROLLER_LIBRARIES ${NVDA_CONTROLLER_LIBRARY})
  if(NOT NVDA_CONTROLLER_LIBRARY)
    message(
      FATAL_ERROR
        "Could not find NVDA controller library in ${NVDA_CONTROLLER_DIR}")
    set(NVDA_CONTROLLER_FOUND FALSE)
  else()
    set(NVDA_CONTROLLER_FOUND TRUE)
    add_library(NVDA::Controller SHARED IMPORTED)
    if(WIN32)
      set_target_properties(
        NVDA::Controller
        PROPERTIES IMPORTED_IMPLIB "${NVDA_CONTROLLER_LIBRARY}"
                   INTERFACE_INCLUDE_DIRECTORIES
                   "${NVDA_CONTROLLER_INCLUDE_DIRS}")
      file(GLOB NVDA_CONTROLLER_DLL "${NVDA_CONTROLLER_DIR}/*.dll")
      if(NVDA_CONTROLLER_DLL)
        set_target_properties(
          NVDA::Controller PROPERTIES IMPORTED_LOCATION
                                      "${NVDA_CONTROLLER_DLL}")
      endif()
    else()
      set_target_properties(
        NVDA::Controller
        PROPERTIES IMPORTED_LOCATION "${NVDA_CONTROLLER_LIBRARY}"
                   INTERFACE_INCLUDE_DIRECTORIES
                   "${NVDA_CONTROLLER_INCLUDE_DIRS}")
    endif()
    message(STATUS "Found NVDA Controller: ${NVDA_CONTROLLER_LIBRARY}")
  endif()
else()
  message(FATAL_ERROR "Failed to download NVDA Controller Client")
endif()
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  NVDAController REQUIRED_VARS NVDA_CONTROLLER_FOUND NVDA_CONTROLLER_LIBRARIES
                               NVDA_CONTROLLER_INCLUDE_DIRS)
mark_as_advanced(NVDA_CONTROLLER_FOUND NVDA_CONTROLLER_INCLUDE_DIRS
                 NVDA_CONTROLLER_LIBRARIES)
