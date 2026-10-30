# SPDX-License-Identifier: MPL-2.0
include_guard(GLOBAL)
include(PrismGuards)
prism_require_targets(prism prism_common)
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_library(AVFOUNDATION_FRAMEWORK AVFoundation REQUIRED)
target_link_libraries(prism PRIVATE ${FOUNDATION_FRAMEWORK}
                                    ${AVFOUNDATION_FRAMEWORK} c++)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
  find_library(CORESERVICES_FRAMEWORK CoreServices REQUIRED)
  find_library(SECURITY_FRAMEWORK Security REQUIRED)
  target_link_libraries(
    prism PRIVATE ${APPKIT_FRAMEWORK} ${CORESERVICES_FRAMEWORK}
                  ${SECURITY_FRAMEWORK})
elseif(CMAKE_SYSTEM_NAME STREQUAL "watchOS")
  find_library(WATCHKIT_FRAMEWORK WatchKit REQUIRED)
  target_link_libraries(prism PRIVATE ${WATCHKIT_FRAMEWORK})
  string(TOLOWER "${CMAKE_OSX_SYSROOT}" _prism_sysroot_lc)
  if(_prism_sysroot_lc MATCHES "simulator")
    set(_prism_watch_env "-simulator")
  else()
    set(_prism_watch_env "")
  endif()
  set(_prism_swift_target
      "${CMAKE_OSX_ARCHITECTURES}-apple-watchos${CMAKE_OSX_DEPLOYMENT_TARGET}${_prism_watch_env}"
  )
  include(ExternalProject)
  ExternalProject_Add(
    prism_speech_bridge
    SOURCE_DIR "${PRISM_SOURCE_ROOT}/swiftbridge"
    CMAKE_ARGS -DCMAKE_SYSTEM_NAME=watchOS
               -DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}
               -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
               -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
               -DCMAKE_Swift_COMPILER_TARGET=${_prism_swift_target}
    BUILD_BYPRODUCTS <BINARY_DIR>/libPrismSpeechBridge.a
                     <BINARY_DIR>/include/PrismSpeechBridge-Swift.h
    INSTALL_COMMAND "")
  ExternalProject_Get_Property(prism_speech_bridge BINARY_DIR)
  if(TARGET prism_backend_voiceover)
    add_dependencies(prism_backend_voiceover prism_speech_bridge)
    target_include_directories(prism_backend_voiceover
                               PRIVATE "${BINARY_DIR}/include")
  endif()
  add_dependencies(prism prism_speech_bridge)
  target_link_libraries(prism PRIVATE "${BINARY_DIR}/libPrismSpeechBridge.a")
else()
  find_library(UIKIT_FRAMEWORK UIKit REQUIRED)
  target_link_libraries(prism PRIVATE ${UIKIT_FRAMEWORK})
endif()
if(PRISM_ENABLE_POWER_MANAGEMENT AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
  find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
  target_compile_definitions(prism_common
                             INTERFACE PRISM_ENABLE_POWER_MANAGEMENT)
  target_link_libraries(prism PRIVATE ${IOKIT_FRAMEWORK}
                                      ${COREFOUNDATION_FRAMEWORK})
endif()
set_source_files_properties(
  "${PRISM_SOURCE_ROOT}/source/power_notifier.cpp" TARGET_DIRECTORY prism
  PROPERTIES COMPILE_OPTIONS "-x;objective-c++;-fobjc-arc")
target_link_options(prism PRIVATE "LINKER:-headerpad_max_install_names")
