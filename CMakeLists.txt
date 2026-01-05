# SPDX-License-Identifier: MPL-2.0

cmake_minimum_required(VERSION 3.14)

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(CheckLinkerFlag)
include(CheckIPOSupported)
include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

project(
  prism
  VERSION 0.1.0
  DESCRIPTION
    "The Platform-agnostic Reader Interface library for Speech and Messages"
  LANGUAGES C CXX)

if(APPLE)
  enable_language(Swift)
endif()

function(add_import_library target def_file dll_name)
  set(lib_file "${CMAKE_CURRENT_BINARY_DIR}/${target}.lib")
  if(MSVC)
    add_custom_command(
      OUTPUT "${lib_file}"
      COMMAND lib /nologo /def:"${def_file}" /out:"${lib_file}"
              /machine:${MSVC_CXX_ARCHITECTURE_ID}
      DEPENDS "${def_file}"
      COMMENT "Generating import library ${target}.lib")
  elseif(MINGW)
    add_custom_command(
      OUTPUT "${lib_file}"
      COMMAND dlltool -d "${def_file}" -l "${lib_file}"
      DEPENDS "${def_file}"
      COMMENT "Generating import library ${target}.lib")
  endif()
  add_custom_target(${target}_gen DEPENDS "${lib_file}")
  add_library(${target} SHARED IMPORTED GLOBAL)
  set_target_properties(${target} PROPERTIES IMPORTED_IMPLIB "${lib_file}"
                                             IMPORTED_LOCATION "${dll_name}")
  add_dependencies(${target} ${target}_gen)
endfunction()

set(CPM_DOWNLOAD_VERSION 0.42.0)
if(CPM_SOURCE_CACHE)
  set(CPM_DOWNLOAD_LOCATION
      "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif(DEFINED ENV{CPM_SOURCE_CACHE})
  set(CPM_DOWNLOAD_LOCATION
      "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else()
  set(CPM_DOWNLOAD_LOCATION
      "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif()
if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
  message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
  file(
    DOWNLOAD
    https://github.com/TheLartians/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
    ${CPM_DOWNLOAD_LOCATION})
endif()
include(${CPM_DOWNLOAD_LOCATION})

file(GLOB_RECURSE headers CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
     "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h")
if(APPLE)
  file(
    GLOB_RECURSE
    sources
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.swift")
else()
  file(GLOB_RECURSE sources CONFIGURE_DEPENDS
       "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
       "${CMAKE_CURRENT_SOURCE_DIR}/source/*.c")
endif()
# Remove platform-specific files
if(NOT WIN32)
  list(
    REMOVE_ITEM
    sources
    "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/fsapi.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/sapibridge.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/wineyes.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/zt.c")
  list(REMOVE_ITEM headers
       "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/sapibridge.h")
endif()
if(APPLE
   OR WIN32
   OR WINDOWS_STORE)
  list(REMOVE_ITEM sources
       "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/orca-module.c"
       "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/orca-service.c")
  list(REMOVE_ITEM headers
       "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/orca-module.h"
       "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/orca-service.h")
endif()
add_library(${PROJECT_NAME} SHARED ${headers} ${sources})
set_property(TARGET ${PROJECT_NAME} PROPERTY OUTPUT_NAME ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME}
                           PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_compile_definitions(
  ${PROJECT_NAME}
  PRIVATE PRISM_BUILDING
  PUBLIC NOMINMAX)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
if(WIN32)
  find_package(NVDAController REQUIRED)
  target_link_libraries(${PROJECT_NAME} PRIVATE NVDA::Controller)
  target_link_libraries(${PROJECT_NAME} PRIVATE delayimp.lib onecore.lib)
  add_import_library(SystemAccess ${CMAKE_CURRENT_SOURCE_DIR}/defs/sa.def
                     SAAPI64.dll)
  target_link_libraries(${PROJECT_NAME} PRIVATE SystemAccess)
endif()
if(APPLE)
  find_library(AVFOUNDATION_FRAMEWORK AVFoundation REQUIRED)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${AVFOUNDATION_FRAMEWORK})
endif()
if(UNIX AND NOT APPLE)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GIO gio-2.0)
  if(GIO_FOUND)
    target_include_directories(
      ${PROJECT_NAME} PRIVATE ${GIO_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${GIO_LIBRARIES})
  else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE NO_ORCA)
  endif()
  pkg_check_modules(LIBSPEECHD speech-dispatcher)
  if(LIBSPEECHD_FOUND)
    target_include_directories(
      ${PROJECT_NAME} PRIVATE ${LIBSPEECHD_INCLUDE_DIRS}
                              ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBSPEECHD_LIBRARIES})
  else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE NO_LIBSPEECHD)
  endif()
endif()
if(WIN32 OR WINDOWS_STORE)
  target_compile_definitions(
    ${PROJECT_NAME}
    PUBLIC _CRT_SECURE_NO_WARNINGS _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1
           _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT=1)
endif()
if(MSVC)
  set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES
      LINK_FLAGS
      "/STACK:8388608 /delayload:nvdaControllerClient.dll /delayload:SAAPI64.dll"
  )
  check_cxx_compiler_flag(/utf-8 MSVC_HAS_UTF8_OPTION)
  check_cxx_compiler_flag(/bigobj MSVC_HAS_BIGOBJ)
  if(MSVC_HAS_UTF8_OPTION)
    target_compile_options(${PROJECT_NAME} PUBLIC /utf-8)
  endif()
  if(MSVC_HAS_BIGOBJ)
    target_compile_options(${PROJECT_NAME} PUBLIC /bigobj)
  endif()
endif()
if(APPLE)
  set_target_properties(
    ${PROJECT_NAME} PROPERTIES LINK_FLAGS "-Wl,-headerpad_max_install_names")
endif()
if(NOT MSVC)
  check_cxx_compiler_flag(-march=x86-64-v3 COMPILER_SUPPORTS_X86_64_V3)
  check_cxx_compiler_flag(-march=armv8.9-a COMPILER_SUPPORTS_ARMV89_A)
  check_cxx_compiler_flag(-march=armv9.5-a COMPILER_SUPPORTS_ARMV95_A)
  if(COMPILER_SUPPORTS_X86_64_V3)
    target_compile_options(${PROJECT_NAME} PUBLIC -march=x86-64-v3)
  elseif(COMPILER_SUPPORTS_ARMV95_A)
    target_compile_options(${PROJECT_NAME} PUBLIC -march=armv9.5-a)
  elseif(COMPILER_SUPPORTS_ARMV89_A)
    target_compile_options(${PROJECT_NAME} PUBLIC -march=armv8.9-a)
  else()
    message(
      STATUS
        "Architecture ISA extensions are disabled: no supported instruction sets"
    )
  endif()
endif()
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT CMAKE_CXX_COMPILER_ID
                                               MATCHES "MSVC")
  find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
  if(CLANG_TIDY_EXE)
    set(CLANG_TIDY_COMMAND
        "${CLANG_TIDY_EXE}"
        "-checks=-*,bugprone-*,clang-*,concurrency-*,modernize-*,performance-*")
    set_target_properties(${PROJECT_NAME} PROPERTIES CXX_CLANG_TIDY
                                                     "${CLANG_TIDY_COMMAND}")
    set_source_files_properties(source/simdutf.cpp source/simdutf.hpp
                                PROPERTIES SKIP_LINTING ON)
  endif()
endif()
check_ipo_supported()
set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
set(CMAKE_INSTALL_DEBUG_LIBRARIES ON)
set(CMAKE_INSTALL_UCRT_LIBRARIES ON)
set(CMAKE_INSTALL_MFC_LIBRARIES ON)
set(CMAKE_INSTALL_OPENMP_LIBRARIES ON)
include(InstallRequiredSystemLibraries)
