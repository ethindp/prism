# SPDX-License-Identifier: MPL-2.0

include_guard(GLOBAL)
include(PrismGuards)
prism_require_vars(PRISM_SOURCE_ROOT PRISM_DEPENDENCY_PROVIDER)
set(PRISM_COMPILED_DEP_TARGETS
    ""
    CACHE INTERNAL "")

# prism_declare_dependency(<name> PACKAGE <find_package name>        required
# unless BUNDLED_ONLY MIN_VERSION <ver>                  version floor for the
# SYSTEM provider SYSTEM_TARGETS <tgt>...            what to link when SYSTEM
# BUNDLED_ROOT <dir>                 relative to PRISM_SOURCE_ROOT
# BUNDLED_SOURCES <file>...          relative to BUNDLED_ROOT; omit if
# HEADER_ONLY BUNDLED_INCLUDES <dir>...          relative to BUNDLED_ROOT; the
# installed spelling LICENSE <path>                     relative to
# PRISM_SOURCE_ROOT [HEADER_ONLY] [BUNDLED_ONLY] [LANGUAGE C|CXX])
function(prism_declare_dependency NAME)
  cmake_parse_arguments(
    PD "HEADER_ONLY;BUNDLED_ONLY"
    "PACKAGE;MIN_VERSION;BUNDLED_ROOT;LICENSE;LANGUAGE"
    "SYSTEM_TARGETS;BUNDLED_SOURCES;BUNDLED_INCLUDES" ${ARGN})
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
    find_package(${PD_PACKAGE} ${PD_MIN_VERSION} CONFIG QUIET)
    if(NOT ${PD_PACKAGE}_FOUND)
      message(
        FATAL_ERROR
          "PRISM_${_UP}_PROVIDER=SYSTEM but find_package(${PD_PACKAGE} ${PD_MIN_VERSION} CONFIG) failed.\n"
          "Install ${NAME}, or set -DPRISM_${_UP}_PROVIDER=BUNDLED to use third_party/."
      )
    endif()
    add_library(${_impl} INTERFACE)
    target_link_libraries(${_impl} INTERFACE ${PD_SYSTEM_TARGETS})
    set(_where "system ${${PD_PACKAGE}_VERSION}")
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
          "Every bundled dependency ships its license. No exceptions.")
    endif()
    if(PD_HEADER_ONLY)
      add_library(${_impl} INTERFACE)
      set(_vis INTERFACE)
    else()
      set(_srcs "")
      foreach(_s IN LISTS PD_BUNDLED_SOURCES)
        list(APPEND _srcs "${_root}/${_s}")
      endforeach()
      add_library(${_impl} STATIC ${_srcs})
      set_target_properties(
        ${_impl}
        PROPERTIES POSITION_INDEPENDENT_CODE ON
                   CXX_VISIBILITY_PRESET hidden
                   C_VISIBILITY_PRESET hidden
                   VISIBILITY_INLINES_HIDDEN ON)
      if(PD_LANGUAGE STREQUAL "C")
        set_target_properties(${_impl} PROPERTIES C_STANDARD 17
                                                  C_STANDARD_REQUIRED ON)
      endif()
      if(MSVC)
        target_compile_options(${_impl} PRIVATE /analyze- /W0 /WX- /utf-8)
      else()
        target_compile_options(${_impl} PRIVATE -w)
      endif()
      set_property(TARGET ${_impl} PROPERTY SKIP_LINTING ON)
      set(PRISM_COMPILED_DEP_TARGETS
          "${PRISM_COMPILED_DEP_TARGETS};${_impl}"
          CACHE INTERNAL "")
      set(_vis PUBLIC)
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
    set(_where "bundled ${PD_BUNDLED_ROOT}")
  endif()
  add_library(prism::dep::${NAME} ALIAS ${_impl})
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
  unofficial-concurrentqueue
  HEADER_ONLY
  SYSTEM_TARGETS
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
    support
    support/cpp
    support/jni
    LICENSE
    LICENSES/djinni)
endif()
