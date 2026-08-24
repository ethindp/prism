# SPDX-License-Identifier: MPL-2.0
include_guard(GLOBAL)
include(PrismGuards)
prism_require_vars(PRISM_SOURCE_ROOT PRISM_DEPENDENCY_PROVIDER PRISM_USE_IPO)

set(PRISM_COMPILED_DEP_TARGETS
    ""
    CACHE INTERNAL "")
set(PRISM_SYSTEM_DEP_TARGETS
    ""
    CACHE INTERNAL "")
set(PRISM_SYSTEM_DEP_FIND_DEPENDS
    ""
    CACHE INTERNAL "")
set(PRISM_PKGCONFIG_FIND_DEPENDS
    ""
    CACHE INTERNAL "")

function(prism_declare_dependency NAME)
  cmake_parse_arguments(
    PD
    "HEADER_ONLY;BUNDLED_ONLY"
    "PACKAGE;ALT_PACKAGE;MIN_VERSION;BUNDLED_ROOT;LICENSE;LANGUAGE"
    "SYSTEM_TARGETS;ALT_SYSTEM_TARGETS;BUNDLED_SOURCES;BUNDLED_INCLUDES"
    ${ARGN})
  if(PD_UNPARSED_ARGUMENTS)
    message(
      FATAL_ERROR
        "prism_declare_dependency(${NAME}): unknown args: ${PD_UNPARSED_ARGUMENTS}"
    )
  endif()
  foreach(_req BUNDLED_ROOT LICENSE BUNDLED_INCLUDES)
    if(NOT PD_${_req})
      message(
        FATAL_ERROR "prism_declare_dependency(${NAME}): ${_req} is required")
    endif()
  endforeach()
  if(PD_HEADER_ONLY AND PD_BUNDLED_SOURCES)
    message(
      FATAL_ERROR
        "prism_declare_dependency(${NAME}): HEADER_ONLY excludes BUNDLED_SOURCES"
    )
  endif()
  if(NOT PD_HEADER_ONLY AND NOT PD_BUNDLED_SOURCES)
    message(
      FATAL_ERROR
        "prism_declare_dependency(${NAME}): need BUNDLED_SOURCES or HEADER_ONLY"
    )
  endif()
  string(TOUPPER "${NAME}" _UP)
  set(_impl prism_dep_${NAME})
  set(_alias_target ${_impl})
  if(PD_BUNDLED_ONLY)
    if(PD_PACKAGE OR PD_SYSTEM_TARGETS)
      message(
        FATAL_ERROR
          "prism_declare_dependency(${NAME}): BUNDLED_ONLY excludes PACKAGE/SYSTEM_TARGETS"
      )
    endif()
    set(_provider "BUNDLED")
  else()
    if(NOT PD_PACKAGE OR NOT PD_SYSTEM_TARGETS)
      message(
        FATAL_ERROR
          "prism_declare_dependency(${NAME}): PACKAGE and SYSTEM_TARGETS are required unless BUNDLED_ONLY"
      )
    endif()
    set(PRISM_${_UP}_PROVIDER
        "${PRISM_DEPENDENCY_PROVIDER}"
        CACHE STRING "Provider for ${NAME} (BUNDLED or SYSTEM)")
    set_property(CACHE PRISM_${_UP}_PROVIDER PROPERTY STRINGS BUNDLED SYSTEM)
    prism_require_enum(PRISM_${_UP}_PROVIDER BUNDLED SYSTEM)
    set(_provider "${PRISM_${_UP}_PROVIDER}")
  endif()
  if(_provider STREQUAL "SYSTEM")
    set(_pkg "${PD_PACKAGE}")
    set(_pkg_targets "${PD_SYSTEM_TARGETS}")
    find_package(${_pkg} ${PD_MIN_VERSION} CONFIG QUIET)
    if(NOT ${_pkg}_FOUND AND PD_ALT_PACKAGE)
      set(_pkg "${PD_ALT_PACKAGE}")
      set(_pkg_targets "${PD_ALT_SYSTEM_TARGETS}")
      find_package(${_pkg} ${PD_MIN_VERSION} CONFIG QUIET)
    endif()
    if(NOT ${_pkg}_FOUND)
      set(_tried "${PD_PACKAGE}")
      if(PD_ALT_PACKAGE)
        string(APPEND _tried " or ${PD_ALT_PACKAGE}")
      endif()
      message(
        FATAL_ERROR
          "PRISM_${_UP}_PROVIDER=SYSTEM but find_package(${_tried} ${PD_MIN_VERSION} CONFIG) failed.\n"
          "Install ${NAME}, or set -DPRISM_${_UP}_PROVIDER=BUNDLED to use third_party/."
      )
    endif()
    add_library(${_impl} INTERFACE)
    target_link_libraries(${_impl} INTERFACE ${_pkg_targets})
    set(_where "system ${${_pkg}_VERSION} (${_pkg})")
    set(_sdt "${PRISM_SYSTEM_DEP_TARGETS}")
    list(APPEND _sdt ${_pkg_targets})
    set(PRISM_SYSTEM_DEP_TARGETS
        "${_sdt}"
        CACHE INTERNAL "")
    set(_found_version "${${_pkg}_VERSION}")
    if(NOT _found_version)
      set(_found_version "${PD_MIN_VERSION}")
    endif()
    set(_sfd "${PRISM_SYSTEM_DEP_FIND_DEPENDS}")
    list(APPEND _sfd "find_dependency(${_pkg} ${_found_version} CONFIG)")
    set(PRISM_SYSTEM_DEP_FIND_DEPENDS
        "${_sfd}"
        CACHE INTERNAL "")
  else()
    set(_root "${PRISM_SOURCE_ROOT}/${PD_BUNDLED_ROOT}")
    if(NOT IS_DIRECTORY "${_root}")
      message(
        FATAL_ERROR
          "prism_declare_dependency(${NAME}): BUNDLED_ROOT '${_root}' does not exist"
      )
    endif()
    if(NOT EXISTS "${PRISM_SOURCE_ROOT}/${PD_LICENSE}")
      message(
        FATAL_ERROR
          "prism_declare_dependency(${NAME}): bundled, but LICENSE '${PD_LICENSE}' is missing.\n"
      )
    endif()
    if(PD_HEADER_ONLY)
      add_library(${_impl} INTERFACE)
      set(_vis INTERFACE)
    else()
      set(_srcs "")
      foreach(_s IN LISTS PD_BUNDLED_SOURCES)
        list(APPEND _srcs "${_root}/${_s}")
      endforeach()
      add_library(${_impl} OBJECT ${_srcs})
      set_target_properties(
        ${_impl}
        PROPERTIES POSITION_INDEPENDENT_CODE ON
                   CXX_VISIBILITY_PRESET hidden
                   C_VISIBILITY_PRESET hidden
                   VISIBILITY_INLINES_HIDDEN ON
                   INTERPROCEDURAL_OPTIMIZATION ${PRISM_USE_IPO})
      if(PRISM_USE_IPO AND PRISM_IPO_FAT_OBJECTS)
        target_compile_options(
          ${_impl} PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffat-lto-objects>)
      endif()
      if(PD_LANGUAGE STREQUAL "C")
        set_target_properties(${_impl} PROPERTIES C_STANDARD 17
                                                  C_STANDARD_REQUIRED ON)
      endif()
      if(MSVC)
        target_compile_options(${_impl} PRIVATE /analyze- /W0 /WX- /utf-8)
      else()
        target_compile_options(${_impl} PRIVATE -w)
      endif()
      set_source_files_properties(${_srcs} PROPERTIES SKIP_LINTING ON)
      set(_cdt "${PRISM_COMPILED_DEP_TARGETS}")
      list(APPEND _cdt "${_impl}")
      set(PRISM_COMPILED_DEP_TARGETS
          "${_cdt}"
          CACHE INTERNAL "")
      set(_vis PUBLIC)
      add_library(${_impl}_iface INTERFACE)
      set(_alias_target ${_impl}_iface)
    endif()
    foreach(_inc IN LISTS PD_BUNDLED_INCLUDES)
      get_filename_component(_abs "${_root}/${_inc}" ABSOLUTE)
      if(NOT IS_DIRECTORY "${_abs}")
        message(
          FATAL_ERROR
            "prism_declare_dependency(${NAME}): BUNDLED_INCLUDES '${_inc}' does not exist under ${_root}"
        )
      endif()
      target_include_directories(${_impl} SYSTEM ${_vis}
                                 "$<BUILD_INTERFACE:${_abs}>")
    endforeach()
    if(NOT _alias_target STREQUAL ${_impl})
      target_include_directories(
        ${_alias_target} SYSTEM
        INTERFACE "$<TARGET_PROPERTY:${_impl},INTERFACE_INCLUDE_DIRECTORIES>")
    endif()
    set(_where "bundled ${PD_BUNDLED_ROOT}")
  endif()
  add_library(prism::dep_iface::${NAME} ALIAS ${_alias_target})
  message(STATUS "Prism dep ${NAME}: ${_where}")
endfunction()

prism_declare_dependency(
  fmt
  PACKAGE
  fmt
  MIN_VERSION
  10.0.0
  SYSTEM_TARGETS
  fmt::fmt
  BUNDLED_ROOT
  third_party/fmt
  BUNDLED_SOURCES
  src/format.cc
  src/os.cc
  BUNDLED_INCLUDES
  include
  LICENSE
  LICENSES/fmt)
prism_declare_dependency(
  highway
  PACKAGE
  HWY
  MIN_VERSION
  1.2.0
  SYSTEM_TARGETS
  hwy::hwy
  BUNDLED_ROOT
  third_party/highway
  BUNDLED_SOURCES
  hwy/abort.cc
  hwy/per_target.cc
  hwy/print.cc
  hwy/targets.cc
  hwy/timer.cc
  BUNDLED_INCLUDES
  .
  LICENSE
  LICENSES/highway)
prism_declare_dependency(
  simdutf
  PACKAGE
  simdutf
  MIN_VERSION
  8.2.0
  SYSTEM_TARGETS
  simdutf::simdutf
  BUNDLED_ROOT
  third_party/simdutf
  BUNDLED_SOURCES
  simdutf.cpp
  BUNDLED_INCLUDES
  include
  LICENSE
  LICENSES/simdutf)
prism_declare_dependency(
  concurrentqueue
  PACKAGE
  concurrentqueue
  ALT_PACKAGE
  unofficial-concurrentqueue
  HEADER_ONLY
  SYSTEM_TARGETS
  concurrentqueue::concurrentqueue
  ALT_SYSTEM_TARGETS
  unofficial::concurrentqueue::concurrentqueue
  BUNDLED_ROOT
  third_party/concurrentqueue
  BUNDLED_INCLUDES
  include
  LICENSE
  LICENSES/concurrentqueue)
prism_declare_dependency(
  dr_wav
  BUNDLED_ONLY
  LANGUAGE
  C
  BUNDLED_ROOT
  third_party/dr_wav
  BUNDLED_SOURCES
  wav.c
  BUNDLED_INCLUDES
  include
  LICENSE
  LICENSES/dr_wav)
prism_declare_dependency(
  moderncom
  BUNDLED_ONLY
  HEADER_ONLY
  BUNDLED_ROOT
  third_party/moderncom
  BUNDLED_INCLUDES
  include
  LICENSE
  LICENSES/moderncom)
if(ANDROID)
  prism_declare_dependency(
    djinni
    BUNDLED_ONLY
    BUNDLED_ROOT
    third_party/djinni
    BUNDLED_SOURCES
    support/cpp/DataRef.cpp
    support/jni/djinni_support.cpp
    support/jni/djinni_main.cpp
    support/jni/DataRef_jni.cpp
    support/jni/Future_jni.cpp
    BUNDLED_INCLUDES
    support/include
    LICENSE
    LICENSES/djinni)
  target_link_libraries(prism_dep_djinni PRIVATE prism_dep_simdutf)
endif()
