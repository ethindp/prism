# SPDX-License-Identifier: MPL-2.0

include_guard(GLOBAL)
include(PrismGuards)
prism_require_vars(PRISM_SOURCE_ROOT PRISM_ARCH_CLASS PRISM_USE_IPO)
prism_require_targets(prism::dep::fmt prism::dep::simdutf
                      prism::dep::concurrentqueue)
add_library(prism_common INTERFACE)
target_compile_features(prism_common INTERFACE cxx_std_23 c_std_17)
target_include_directories(
  prism_common
  INTERFACE "$<BUILD_INTERFACE:${PRISM_SOURCE_ROOT}/include>"
            "$<BUILD_INTERFACE:${PRISM_SOURCE_ROOT}/source>"
            "$<BUILD_INTERFACE:${PRISM_SOURCE_ROOT}/source/backends>")
target_link_libraries(
  prism_common
  INTERFACE prism::dep::fmt prism::dep::simdutf prism::dep::concurrentqueue
            prism::dep::dr_wav prism::dep::moderncom)
set(PRISM_PUBLIC_DEFINES
    NOMINMAX
    $<$<STREQUAL:$<TARGET_PROPERTY:prism,TYPE>,STATIC_LIBRARY>:PRISM_STATIC>)
target_compile_definitions(prism_common INTERFACE PRISM_BUILDING
                                                  ${PRISM_PUBLIC_DEFINES})
if(NOT MSVC AND NOT PRISM_HAS_JTHREAD_NATIVE)
  target_compile_options(
    prism_common INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:-fexperimental-library>)
  target_link_options(prism_common INTERFACE -fexperimental-library)
endif()
set(_prism_sources
    source/backend_catalog.cpp
    source/backend_check.cpp
    source/backend_enumerator.cpp
    source/delayimp.cpp
    source/frozen_registry.cpp
    source/logging.cpp
    source/poll_waiter.cpp
    source/power_notifier.cpp
    source/prism.cpp
    source/utils.cpp
    source/backends/custom_backend.cpp)
if(WIN32)
  list(APPEND _prism_sources source/backends/raw/fsapi.c
       source/backends/raw/wineyes.c)
endif()
if(ANDROID)
  file(GLOB_RECURSE _android_srcs CONFIGURE_DEPENDS
       "${PRISM_SOURCE_ROOT}/source/backends/android/*.cpp")
  list(APPEND _prism_sources ${_android_srcs})
endif()
file(GLOB_RECURSE _prism_headers CONFIGURE_DEPENDS
     "${PRISM_SOURCE_ROOT}/include/*.h" "${PRISM_SOURCE_ROOT}/source/*.h"
     "${PRISM_SOURCE_ROOT}/source/*.hpp")
if(NOT ANDROID)
  list(FILTER _prism_headers EXCLUDE REGEX "/source/backends/android/")
endif()
if(EMSCRIPTEN)
  add_library(prism STATIC ${_prism_sources} ${_prism_headers})
else()
  add_library(prism ${_prism_sources} ${_prism_headers})
endif()
target_link_libraries(prism PRIVATE "$<BUILD_INTERFACE:prism_common>")
target_include_directories(
  prism PUBLIC "$<BUILD_INTERFACE:${PRISM_SOURCE_ROOT}/include>"
               "$<INSTALL_INTERFACE:include>")
target_compile_definitions(
  prism
  PRIVATE PRISM_BUILDING
  PUBLIC ${PRISM_PUBLIC_DEFINES})
set_target_properties(
  prism
  PROPERTIES OUTPUT_NAME prism
             POSITION_INDEPENDENT_CODE ON
             CXX_VISIBILITY_PRESET hidden
             C_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN ON
             INTERPROCEDURAL_OPTIMIZATION ${PRISM_USE_IPO})
get_target_property(PRISM_LIB_TYPE prism TYPE)
if(PRISM_LIB_TYPE STREQUAL "STATIC_LIBRARY")
  set(PRISM_LINK_VIS INTERFACE) # delay-load directives must reach consumers
else()
  set(PRISM_LINK_VIS PRIVATE)
endif()
add_library(prism_iface INTERFACE)
if(PRISM_LIB_TYPE STREQUAL "STATIC_LIBRARY")
  target_link_libraries(
    prism_iface
    INTERFACE
      "$<BUILD_INTERFACE:$<LINK_LIBRARY:WHOLE_ARCHIVE,prism>>"
      "$<INSTALL_INTERFACE:$<LINK_LIBRARY:WHOLE_ARCHIVE,prism::_prism_lib>>")
else()
  target_link_libraries(
    prism_iface INTERFACE "$<BUILD_INTERFACE:prism>"
                          "$<INSTALL_INTERFACE:prism::_prism_lib>")
endif()
set_target_properties(prism PROPERTIES EXPORT_NAME _prism_lib)
set_target_properties(prism_iface PROPERTIES EXPORT_NAME prism)
add_library(prism::_prism_lib ALIAS prism)
add_library(prism::prism ALIAS prism_iface)
