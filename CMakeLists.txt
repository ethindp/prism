# SPDX-License-Identifier: MPL-2.0

cmake_minimum_required(VERSION 3.20)

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
  VERSION 0.13.0
  DESCRIPTION "The Platform-agnostic Reader Interface for Speech and Messages"
  LANGUAGES C CXX)
option(PRISM_ENABLE_TESTS "Enable tests" OFF)
option(PRISM_ENABLE_DEMOS "Enable demo applications" OFF)
option(
  PRISM_ENABLE_LINTING
  "Enable execution of linting this build with whatever linting tools are available (implicitly enables warnings as errors)"
  OFF)
option(PRISM_ENABLE_VCPKG_SPECIFIC_OPTIONS
       "Enable vcpkg-specific build options (DO NOT USE NORMALLY)" OFF)
if(PRISM_ENABLE_VCPKG_SPECIFIC_OPTIONS)
  option(
    PRISM_VCPKG_WANTS_SPEECH_DISPATCHER_BACKEND
    "Indicates that the Prism vcpkg port is being built with the speech-dispatcher feature enabled"
    OFF)
  option(
    PRISM_VCPKG_WANTS_ORCA_BACKEND
    "Indicates that the Prism vcpkg port is being built with the orca feature enabled"
    OFF)
endif()
option(PRISM_ENABLE_GDEXTENSION "Enable building of the Godot GDExtension" ON)
option(PRISM_ENABLE_LEGACY_BACKENDS
       "Enable building of legacy backends which are dead or rarely used" OFF)
if(PRISM_ENABLE_LEGACY_BACKENDS)
  option(PRISM_ENABLE_LEGACY_SYSTEM_ACCESS_BACKEND
         "Enable legacy SystemAccess backend" ON)
  option(PRISM_ENABLE_LEGACY_WINDOW_EYES_BACKEND
         "Enable legacy WindowEyes backend" ON)
endif()
option(
  PRISM_ENABLE_SHIMS
  "Enable shim shared libraries for compatibility with older screen reader libraries"
  OFF)
if(PRISM_ENABLE_SHIMS)
  option(PRISM_ENABLE_TOLK_SHIM "Enable tolk compatibility shim"
         ${PRISM_ENABLE_SHIMS})
endif()
option(PRISM_BUILD_WINELIBS
       "Build Winelib bridges so Wine-hosted Prism can reach Linux TTS hosts"
       OFF)
if(WIN32)
  if(PRISM_ENABLE_GDEXTENSION)
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreadedDLL"
        CACHE STRING "Match godot-cpp non-debug CRT")
  else()
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
      set(CMAKE_MSVC_RUNTIME_LIBRARY
          "MultiThreaded"
          CACHE STRING "Set MSVC CRT library type")
    else()
      set(CMAKE_MSVC_RUNTIME_LIBRARY
          "MultiThreadedDebug"
          CACHE STRING "Set MSVC CRT library type")
    endif()
  endif()
endif()
function(add_import_library target def_file dll_name)
  set(lib_file "${CMAKE_CURRENT_BINARY_DIR}/${target}.lib")
  if(DEFINED MSVC_CXX_ARCHITECTURE_ID)
    set(_lib_machine "${MSVC_CXX_ARCHITECTURE_ID}")
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_lib_machine "X64")
  else()
    set(_lib_machine "X86")
  endif()
  if(MSVC OR (WIN32 AND NOT MINGW))
    find_program(LIB_TOOL NAMES lib llvm-lib)
    if(NOT LIB_TOOL)
      message(
        FATAL_ERROR
          "Could not find 'lib' or 'llvm-lib' to generate import libraries.")
    endif()
    add_custom_command(
      OUTPUT "${lib_file}"
      COMMAND "${LIB_TOOL}" /nologo /def:"${def_file}" /out:"${lib_file}"
              /machine:${_lib_machine}
      DEPENDS "${def_file}"
      COMMENT "Generating import library ${target}.lib")
  elseif(MINGW)
    add_custom_command(
      OUTPUT "${lib_file}"
      COMMAND dlltool -d "${def_file}" -l "${lib_file}"
      DEPENDS "${def_file}"
      COMMENT "Generating import library ${target}.lib")
  else()
    message(FATAL_ERROR "Unknown platform for generating import library")
  endif()
  add_custom_target(${target}_gen DEPENDS "${lib_file}")
  add_library(${target} SHARED IMPORTED GLOBAL)
  set_target_properties(${target} PROPERTIES IMPORTED_IMPLIB "${lib_file}"
                                             IMPORTED_LOCATION "${dll_name}")
  add_dependencies(${target} ${target}_gen)
endfunction()

file(
  GLOB_RECURSE
  headers
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/source/*.hpp")
file(GLOB_RECURSE android_headers CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/java/*.hpp")
file(GLOB_RECURSE android_sources CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/java/*.cpp")
file(GLOB_RECURSE sources CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/source/*.c")
# Remove platform-specific files
if(NOT WIN32)
  list(REMOVE_ITEM sources
       "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/fsapi.c"
       "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/wineyes.c")
endif()
if(NOT ANDROID)
  list(REMOVE_ITEM sources ${android_sources})
  list(REMOVE_ITEM headers ${android_headers})
endif()
if(NOT EMSCRIPTEN)
  set(BUILD_SHARED_LIBS
      ON
      CACHE BOOL "Build shared libraries")
  add_library(${PROJECT_NAME} ${headers} ${sources})
else()
  add_library(${PROJECT_NAME} STATIC ${headers} ${sources})
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/source/simdutf.cpp
                            PROPERTIES COMPILE_OPTIONS "-w")
if(APPLE)
  set(PRISM_OBJCPP_SOURCES
      "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/avspeech.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/voiceover.cpp")
  set_source_files_properties(
    ${PRISM_OBJCPP_SOURCES} PROPERTIES COMPILE_OPTIONS
                                       "-x;objective-c++;-fobjc-arc")
endif()
set(_comp_arch_id "")
if(DEFINED MSVC_CXX_ARCHITECTURE_ID)
  set(_comp_arch_id "${MSVC_CXX_ARCHITECTURE_ID}")
elseif(DEFINED CMAKE_C_COMPILER_ARCHITECTURE_ID)
  set(_comp_arch_id "${CMAKE_C_COMPILER_ARCHITECTURE_ID}")
else()
  set(_comp_arch_id "${CMAKE_SYSTEM_PROCESSOR}")
endif()
string(TOLOWER "${_comp_arch_id}" _comp_arch_id_lc)
set_property(TARGET ${PROJECT_NAME} PROPERTY OUTPUT_NAME ${PROJECT_NAME})
target_include_directories(
  ${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                         $<INSTALL_INTERFACE:include>)
if(ANDROID)
  target_include_directories(${PROJECT_NAME}
                             PRIVATE "${PROJECT_SOURCE_DIR}/source/backends")
endif()
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_compile_definitions(
  ${PROJECT_NAME}
  PRIVATE PRISM_BUILDING
  PUBLIC
    NOMINMAX
    $<$<STREQUAL:$<TARGET_PROPERTY:${PROJECT_NAME},TYPE>,STATIC_LIBRARY>:PRISM_STATIC>
)
if(PRISM_ENABLE_LEGACY_BACKENDS)
  target_compile_definitions(${PROJECT_NAME}
                             PRIVATE PRISM_ENABLE_LEGACY_BACKENDS)
  if(PRISM_ENABLE_LEGACY_SYSTEM_ACCESS_BACKEND)
    target_compile_definitions(
      ${PROJECT_NAME} PRIVATE PRISM_ENABLE_SYSTEM_ACCESS_LEGACY_BACKEND)
  endif()
  if(PRISM_ENABLE_LEGACY_WINDOW_EYES_BACKEND)
    target_compile_definitions(${PROJECT_NAME}
                               PRIVATE PRISM_ENABLE_WINDOW_EYES_LEGACY_BACKEND)
  endif()
endif()
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
get_target_property(_PRISM_LIB_TYPE ${PROJECT_NAME} TYPE)
if(_PRISM_LIB_TYPE STREQUAL "STATIC_LIBRARY")
  set(_PRISM_LINK_VIS INTERFACE)
else()
  set(_PRISM_LINK_VIS PRIVATE)
endif()
if(_PRISM_LIB_TYPE STREQUAL "STATIC_LIBRARY")
  if(MSVC)
    target_link_options(${PROJECT_NAME} INTERFACE
                        "/WHOLEARCHIVE:$<TARGET_LINKER_FILE:${PROJECT_NAME}>")
  elseif(APPLE)
    target_link_options(
      ${PROJECT_NAME} INTERFACE
      "LINKER:-force_load,$<TARGET_LINKER_FILE:${PROJECT_NAME}>")
  else()
    target_link_options(
      ${PROJECT_NAME} INTERFACE "LINKER:--whole-archive"
      "$<TARGET_LINKER_FILE:${PROJECT_NAME}>" "LINKER:--no-whole-archive")
  endif()
endif()
if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE delayimp.lib onecore.lib
                                                uiautomationcore.lib)
  if(_comp_arch_id_lc STREQUAL "arm64" OR _comp_arch_id_lc STREQUAL "aarch64")
    set(MIDL_ENV_TARGET "arm64")
  elseif(
    _comp_arch_id_lc STREQUAL "x64"
    OR _comp_arch_id_lc STREQUAL "amd64"
    OR _comp_arch_id_lc STREQUAL "arm64ec")
    set(MIDL_ENV_TARGET "x64")
  elseif(_comp_arch_id_lc STREQUAL "x86" OR _comp_arch_id_lc STREQUAL "i386")
    set(MIDL_ENV_TARGET "win32")
  else()
    message(
      WARNING
        "Unknown architecture '${_comp_arch_id_lc}' for MIDL. Defaulting to win32."
    )
    set(MIDL_ENV_TARGET "win32")
  endif()
  message(
    STATUS
      "Configuring NVDA Controller MIDL for architecture: ${MIDL_ENV_TARGET}")
  find_program(MIDL_COMPILER midl)
  if(NOT MIDL_COMPILER)
    message(FATAL_ERROR "midl.exe not found. Ensure Windows SDK is installed.")
  endif()
  set(NVDA_CONTROLLER_IDL "${CMAKE_CURRENT_SOURCE_DIR}/idl/nvdaController.idl")
  set(NVDA_CONTROLLER_ACF "${CMAKE_CURRENT_SOURCE_DIR}/idl/nvdaController.acf")
  set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
  file(MAKE_DIRECTORY ${GEN_DIR})
  add_custom_command(
    OUTPUT "${GEN_DIR}/nvda_controller.h" "${GEN_DIR}/nvda_controller.c"
    COMMAND
      ${MIDL_COMPILER} /nologo /env ${MIDL_ENV_TARGET} /client stub /cstub
      "${GEN_DIR}/nvda_controller.c" /header "${GEN_DIR}/nvda_controller.h"
      /server none /prefix all "nvdaController_" /acf "${NVDA_CONTROLLER_ACF}"
      "${NVDA_CONTROLLER_IDL}"
    DEPENDS "${NVDA_CONTROLLER_IDL}" "${NVDA_CONTROLLER_ACF}"
    COMMENT "Generating NVDA controller client stub for ${MIDL_ENV_TARGET}"
    VERBATIM)
  target_sources(${PROJECT_NAME} PRIVATE "${GEN_DIR}/nvda_controller.c")
  target_include_directories(${PROJECT_NAME} PRIVATE ${GEN_DIR})
  target_link_libraries(${PROJECT_NAME} PRIVATE rpcrt4)
  if(_comp_arch_id_lc STREQUAL "x86" OR _comp_arch_id_lc STREQUAL "i386")
    if(PRISM_ENABLE_LEGACY_BACKENDS
       AND PRISM_ENABLE_LEGACY_SYSTEM_ACCESS_BACKEND)
      add_import_library(SystemAccess ${CMAKE_CURRENT_SOURCE_DIR}/defs/sa32.def
                         SAAPI32.dll)
      target_link_libraries(${PROJECT_NAME} PRIVATE SystemAccess)
    endif()
    add_import_library(ZDSR ${CMAKE_CURRENT_SOURCE_DIR}/defs/zdsr32.def
                       ZDSRAPI.dll)
    add_import_library(
      BoyCtrl ${CMAKE_CURRENT_SOURCE_DIR}/defs/boy_pc_reader32.def BoyCtrl.dll)
    add_import_library(
      PCTalker ${CMAKE_CURRENT_SOURCE_DIR}/defs/pc_talker32.def PCTKUSR.dll)
    add_import_library(
      PrismOrcaBridge ${CMAKE_CURRENT_SOURCE_DIR}/defs/prism_orca_bridge32.def
      prism_orca_bridge.dll)
    add_import_library(
      PrismSpeechDispatcherBridge
      ${CMAKE_CURRENT_SOURCE_DIR}/defs/prism_speech_dispatcher_bridge32.def
      prism_speech_dispatcher_bridge.dll)
    target_link_libraries(
      ${PROJECT_NAME} PRIVATE ZDSR BoyCtrl PCTalker PrismOrcaBridge
                              PrismSpeechDispatcherBridge)
  elseif(NOT _comp_arch_id_lc MATCHES "arm64|aarch64|arm64ec")
    if(PRISM_ENABLE_LEGACY_BACKENDS
       AND PRISM_ENABLE_LEGACY_SYSTEM_ACCESS_BACKEND)
      add_import_library(SystemAccess ${CMAKE_CURRENT_SOURCE_DIR}/defs/sa.def
                         SAAPI64.dll)
      target_link_libraries(${PROJECT_NAME} PRIVATE SystemAccess)
    endif()
    add_import_library(ZDSR ${CMAKE_CURRENT_SOURCE_DIR}/defs/zdsr.def
                       ZDSRAPI_x64.dll)
    add_import_library(
      BoyCtrl ${CMAKE_CURRENT_SOURCE_DIR}/defs/boy_pc_reader.def
      BoyCtrl-x64.dll)
    add_import_library(PCTalker ${CMAKE_CURRENT_SOURCE_DIR}/defs/pc_talker.def
                       PCTKUSR.dll)
    add_import_library(
      PrismOrcaBridge ${CMAKE_CURRENT_SOURCE_DIR}/defs/prism_orca_bridge.def
      prism_orca_bridge.dll)
    add_import_library(
      PrismSpeechDispatcherBridge
      ${CMAKE_CURRENT_SOURCE_DIR}/defs/prism_speech_dispatcher_bridge.def
      prism_speech_dispatcher_bridge.dll)
    target_link_libraries(
      ${PROJECT_NAME} PRIVATE ZDSR BoyCtrl PCTalker PrismOrcaBridge
                              PrismSpeechDispatcherBridge)
  else()
    add_import_library(PCTalker ${CMAKE_CURRENT_SOURCE_DIR}/defs/pc_talker.def
                       PCTKUSR.dll)
    add_import_library(
      PrismOrcaBridge ${CMAKE_CURRENT_SOURCE_DIR}/defs/prism_orca_bridge.def
      prism_orca_bridge.dll)
    add_import_library(
      PrismSpeechDispatcherBridge
      ${CMAKE_CURRENT_SOURCE_DIR}/defs/prism_speech_dispatcher_bridge.def
      prism_speech_dispatcher_bridge.dll)
    target_link_libraries(${PROJECT_NAME} PRIVATE PCTalker PrismOrcaBridge
                                                  PrismSpeechDispatcherBridge)
  endif()
endif()
if(APPLE)
  find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
  find_library(AVFOUNDATION_FRAMEWORK AVFoundation REQUIRED)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${FOUNDATION_FRAMEWORK}
                                                ${AVFOUNDATION_FRAMEWORK} c++)
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${APPKIT_FRAMEWORK})
  else()
    find_library(UIKIT_FRAMEWORK UIKit REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${UIKIT_FRAMEWORK})
  endif()
endif()
if(UNIX
   AND NOT APPLE
   AND NOT ANDROID
   AND NOT EMSCRIPTEN)
  if(PRISM_ENABLE_VCPKG_SPECIFIC_OPTIONS)
    find_package(PkgConfig REQUIRED)
    if(PRISM_VCPKG_WANTS_ORCA_BACKEND)
      pkg_check_modules(GIO gio-unix-2.0 REQUIRED)
      find_program(GDBUS_CODEGEN_EXE gdbus-codegen REQUIRED)
      set(XML_SERVICE "${CMAKE_CURRENT_SOURCE_DIR}/dbus/orca-service.xml")
      set(XML_MODULE "${CMAKE_CURRENT_SOURCE_DIR}/dbus/orca-module.xml")
      set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
      file(MAKE_DIRECTORY ${GEN_DIR})
      add_custom_command(
        OUTPUT "${GEN_DIR}/orca-service.h" "${GEN_DIR}/orca-service.c"
        COMMAND
          ${GDBUS_CODEGEN_EXE} --c-namespace OrcaService
          --c-generate-object-manager --c-generate-autocleanup all --pragma-once
          --header --output "${GEN_DIR}/orca-service.h" "${XML_SERVICE}"
        COMMAND
          ${GDBUS_CODEGEN_EXE} --c-namespace OrcaService
          --c-generate-object-manager --c-generate-autocleanup all --pragma-once
          --body --output "${GEN_DIR}/orca-service.c" "${XML_SERVICE}"
        DEPENDS "${XML_SERVICE}"
        COMMENT "Generating Orca Service DBus Code")
      add_custom_command(
        OUTPUT "${GEN_DIR}/orca-module.h" "${GEN_DIR}/orca-module.c"
        COMMAND
          ${GDBUS_CODEGEN_EXE} --c-namespace OrcaModule
          --c-generate-object-manager --c-generate-autocleanup all --pragma-once
          --header --output "${GEN_DIR}/orca-module.h" "${XML_MODULE}"
        COMMAND
          ${GDBUS_CODEGEN_EXE} --c-namespace OrcaModule
          --c-generate-object-manager --c-generate-autocleanup all --pragma-once
          --body --output "${GEN_DIR}/orca-module.c" "${XML_MODULE}"
        DEPENDS "${XML_MODULE}"
        COMMENT "Generating Orca Module DBus Code")
      target_sources(${PROJECT_NAME} PRIVATE "${GEN_DIR}/orca-service.c"
                                             "${GEN_DIR}/orca-module.c")
      target_include_directories(${PROJECT_NAME} PRIVATE ${GEN_DIR})
      target_include_directories(
        ${PROJECT_NAME} PRIVATE ${GIO_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
      target_link_directories(${PROJECT_NAME} PRIVATE ${GIO_LIBRARY_DIRS})
      target_link_libraries(${PROJECT_NAME} PRIVATE ${GIO_LIBRARIES})
    else()
      target_compile_definitions(${PROJECT_NAME} PRIVATE NO_ORCA)
    endif()
    if(PRISM_VCPKG_WANTS_SPEECH_DISPATCHER_BACKEND)
      pkg_check_modules(LIBSPEECHD speech-dispatcher REQUIRED)
      target_include_directories(
        ${PROJECT_NAME} PRIVATE ${LIBSPEECHD_INCLUDE_DIRS}
                                ${CMAKE_CURRENT_BINARY_DIR})
      target_link_directories(${PROJECT_NAME} PRIVATE
                              ${LIBSPEECHD_LIBRARY_DIRS})
      target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBSPEECHD_LIBRARIES})
    else()
      target_compile_definitions(${PROJECT_NAME} PRIVATE NO_LIBSPEECHD)
    endif()
  else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GIO gio-unix-2.0)
    if(GIO_FOUND)
      find_program(GDBUS_CODEGEN_EXE gdbus-codegen REQUIRED)
      set(XML_SERVICE "${CMAKE_CURRENT_SOURCE_DIR}/dbus/orca-service.xml")
      set(XML_MODULE "${CMAKE_CURRENT_SOURCE_DIR}/dbus/orca-module.xml")
      set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
      file(MAKE_DIRECTORY ${GEN_DIR})
      add_custom_command(
        OUTPUT "${GEN_DIR}/orca-service.h" "${GEN_DIR}/orca-service.c"
        COMMAND
          ${GDBUS_CODEGEN_EXE} --c-namespace OrcaService
          --c-generate-object-manager --c-generate-autocleanup all --pragma-once
          --header --output "${GEN_DIR}/orca-service.h" "${XML_SERVICE}"
        COMMAND
          ${GDBUS_CODEGEN_EXE} --c-namespace OrcaService
          --c-generate-object-manager --c-generate-autocleanup all --pragma-once
          --body --output "${GEN_DIR}/orca-service.c" "${XML_SERVICE}"
        DEPENDS "${XML_SERVICE}"
        COMMENT "Generating Orca Service DBus Code")
      add_custom_command(
        OUTPUT "${GEN_DIR}/orca-module.h" "${GEN_DIR}/orca-module.c"
        COMMAND
          ${GDBUS_CODEGEN_EXE} --c-namespace OrcaModule
          --c-generate-object-manager --c-generate-autocleanup all --pragma-once
          --header --output "${GEN_DIR}/orca-module.h" "${XML_MODULE}"
        COMMAND
          ${GDBUS_CODEGEN_EXE} --c-namespace OrcaModule
          --c-generate-object-manager --c-generate-autocleanup all --pragma-once
          --body --output "${GEN_DIR}/orca-module.c" "${XML_MODULE}"
        DEPENDS "${XML_MODULE}"
        COMMENT "Generating Orca Module DBus Code")
      target_sources(${PROJECT_NAME} PRIVATE "${GEN_DIR}/orca-service.c"
                                             "${GEN_DIR}/orca-module.c")
      target_include_directories(${PROJECT_NAME} PRIVATE ${GEN_DIR})
      target_include_directories(
        ${PROJECT_NAME} PRIVATE ${GIO_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
      target_link_directories(${PROJECT_NAME} PRIVATE ${GIO_LIBRARY_DIRS})
      target_link_libraries(${PROJECT_NAME} PRIVATE ${GIO_LIBRARIES})
    else()
      target_compile_definitions(${PROJECT_NAME} PRIVATE NO_ORCA)
    endif()
    pkg_check_modules(LIBSPEECHD speech-dispatcher)
    if(LIBSPEECHD_FOUND)
      target_include_directories(
        ${PROJECT_NAME} PRIVATE ${LIBSPEECHD_INCLUDE_DIRS}
                                ${CMAKE_CURRENT_BINARY_DIR})
      target_link_directories(${PROJECT_NAME} PRIVATE
                              ${LIBSPEECHD_LIBRARY_DIRS})
      target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBSPEECHD_LIBRARIES})
    else()
      target_compile_definitions(${PROJECT_NAME} PRIVATE NO_LIBSPEECHD)
    endif()
  endif()
endif()
if(WIN32 OR WINDOWS_STORE)
  target_compile_definitions(
    ${PROJECT_NAME}
    PUBLIC _CRT_SECURE_NO_WARNINGS _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1
           _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT=1
           BELT_COM_NO_LEAK_DETECTION UNICODE _UNICODE)
endif()
if(MSVC)
  target_link_options(${PROJECT_NAME} PRIVATE "/STACK:8388608")
  if(_comp_arch_id_lc STREQUAL "x86" OR _comp_arch_id_lc STREQUAL "i386")
    target_link_options(
      ${PROJECT_NAME}
      ${_PRISM_LINK_VIS}
      /delayload:ZDSRAPI.dll
      /delayload:BoyCtrl.dll
      /delayload:PCTKUSR.dll
      /delayload:prism_orca_bridge.dll
      /delayload:prism_speech_dispatcher_bridge.dll)
    if(PRISM_ENABLE_LEGACY_BACKENDS
       AND PRISM_ENABLE_LEGACY_SYSTEM_ACCESS_BACKEND)
      target_link_options(${PROJECT_NAME} ${_PRISM_LINK_VIS}
                          /delayload:SAAPI32.dll)
    endif()
  elseif(NOT _comp_arch_id_lc MATCHES "arm64|aarch64|arm64ec")
    target_link_options(
      ${PROJECT_NAME}
      ${_PRISM_LINK_VIS}
      /delayload:ZDSRAPI_x64.dll
      /delayload:BoyCtrl-x64.dll
      /delayload:PCTKUSR.dll
      /delayload:prism_orca_bridge.dll
      /delayload:prism_speech_dispatcher_bridge.dll)
    if(PRISM_ENABLE_LEGACY_BACKENDS
       AND PRISM_ENABLE_LEGACY_SYSTEM_ACCESS_BACKEND)
      target_link_options(${PROJECT_NAME} ${_PRISM_LINK_VIS}
                          /delayload:SAAPI64.dll)
    endif()
  else()
    target_link_options(
      ${PROJECT_NAME} ${_PRISM_LINK_VIS} /delayload:PCTKUSR.dll
      /delayload:prism_orca_bridge.dll
      /delayload:prism_speech_dispatcher_bridge.dll)
  endif()
  check_cxx_compiler_flag(/utf-8 MSVC_HAS_UTF8_OPTION)
  check_cxx_compiler_flag(/bigobj MSVC_HAS_BIGOBJ)
  if(MSVC_HAS_UTF8_OPTION)
    target_compile_options(${PROJECT_NAME} PUBLIC /utf-8)
  endif()
  if(MSVC_HAS_BIGOBJ)
    target_compile_options(${PROJECT_NAME} PUBLIC /bigobj)
  endif()
elseif(
  CMAKE_CXX_COMPILER_ID MATCHES "Clang"
  AND WIN32
  AND NOT MINGW)
  if(_comp_arch_id_lc STREQUAL "x86" OR _comp_arch_id_lc STREQUAL "i386")
    target_link_options(${PROJECT_NAME} PRIVATE "LINKER:/STACK:8388608")
    target_link_options(
      ${PROJECT_NAME}
      ${_PRISM_LINK_VIS}
      "LINKER:/delayload:ZDSRAPI.dll"
      "LINKER:/delayload:BoyCtrl.dll"
      "LINKER:/delayload:PCTKUSR.dll"
      "LINKER:/delayload:prism_orca_bridge.dll"
      "LINKER:/delayload:prism_speech_dispatcher_bridge.dll")
    if(PRISM_ENABLE_LEGACY_BACKENDS
       AND PRISM_ENABLE_LEGACY_SYSTEM_ACCESS_BACKEND)
      target_link_options(${PROJECT_NAME} ${_PRISM_LINK_VIS}
                          "LINKER:/delayload:SAAPI32.dll")
    endif()
  elseif(NOT _comp_arch_id_lc MATCHES "arm64|aarch64|arm64ec")
    target_link_options(${PROJECT_NAME} PRIVATE "LINKER:/STACK:8388608")
    target_link_options(
      ${PROJECT_NAME}
      ${_PRISM_LINK_VIS}
      "LINKER:/delayload:ZDSRAPI_x64.dll"
      "LINKER:/delayload:BoyCtrl-x64.dll"
      "LINKER:/delayload:PCTKUSR.dll"
      "LINKER:/delayload:prism_orca_bridge.dll"
      "LINKER:/delayload:prism_speech_dispatcher_bridge.dll")
    if(PRISM_ENABLE_LEGACY_BACKENDS
       AND PRISM_ENABLE_LEGACY_SYSTEM_ACCESS_BACKEND)
      target_link_options(${PROJECT_NAME} ${_PRISM_LINK_VIS}
                          "LINKER:/delayload:SAAPI64.dll")
    endif()
  else()
    target_link_options(${PROJECT_NAME} PRIVATE "LINKER:/STACK:8388608")
    target_link_options(
      ${PROJECT_NAME} ${_PRISM_LINK_VIS} "LINKER:/delayload:PCTKUSR.dll"
      "LINKER:/delayload:prism_orca_bridge.dll"
      "LINKER:/delayload:prism_speech_dispatcher_bridge.dll")
  endif()
elseif(WIN32 AND MINGW)
  message(
    WARNING "Delay loading will malfunction and is NOT supported by MinGW!")
endif()
if(NOT MSVC)
  check_cxx_compiler_flag(-Wall COMP_HAS_WALL_OPTION)
  check_cxx_compiler_flag(-Wextra COMP_HAS_WEXTRA_OPTION)
  check_cxx_compiler_flag(-Wthread-safety COMP_HAS_WTHREAD_SAFETY_OPTION)
  check_cxx_compiler_flag(-Wthread-safety-negative
                          COMP_HAS_WTHREAD_SAFETY_NEGATIVE_OPTION)
  if(COMP_HAS_WALL_OPTION AND COMP_HAS_WEXTRA_OPTION)
    target_compile_options(${PROJECT_NAME}
                           PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
  endif()
  if(COMP_HAS_WTHREAD_SAFETY_OPTION)
    target_compile_options(
      ${PROJECT_NAME}
      PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-Wthread-safety>
              $<$<COMPILE_LANGUAGE:C,CXX>:-Werror=thread-safety>)
  endif()
  if(COMP_HAS_WTHREAD_SAFETY_NEGATIVE_OPTION)
    target_compile_options(
      ${PROJECT_NAME}
      PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-Wthread-safety-negative>
              $<$<COMPILE_LANGUAGE:C,CXX>:-Werror=thread-safety-negative>)
  endif()
endif()
if(APPLE)
  target_link_options(${PROJECT_NAME} PRIVATE
                      "LINKER:-headerpad_max_install_names")
endif()
if(ANDROID)
  target_link_options(${PROJECT_NAME} PRIVATE "-Wl,-z,max-page-size=16384")
endif()
if(PRISM_ENABLE_LINTING)
  set(SKIP_LINT_FILES
      "${CMAKE_CURRENT_SOURCE_DIR}/source/simdutf.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/source/simdutf.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/dr_wav.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/source/wav.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/source/concurrentqueue.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/fsapi.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/fsapi.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/sense_reader.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/wineyes.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/wineyes.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/zt.h")
  if(ANDROID)
    list(APPEND SKIP_LINT_FILES ${android_sources} ${android_headers})
  endif()
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
    if(CLANG_TIDY_EXE)
      set(CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}")
      set_target_properties(${PROJECT_NAME} PROPERTIES CXX_CLANG_TIDY
                                                       "${CLANG_TIDY_COMMAND}")
      set_source_files_properties(${SKIP_LINT_FILES} PROPERTIES SKIP_LINTING ON)
    endif()
  else()
    if(NOT DEFINED ENV{VSINSTALLDIR})
      message(
        FATAL_ERROR
          "VSINSTALLDIR environment variable is not set. Are you running in a Developer Command Prompt?"
      )
    endif()
    set(_RULESET_DIR
        "$ENV{VSINSTALLDIR}Team Tools\\Static Analysis Tools\\Rule Sets")
    if(NOT EXISTS "${_RULESET_DIR}")
      message(FATAL_ERROR "VS ruleset dir not found at: ${_RULESET_DIR}")
    endif()
    set(_RULESET_FILES
        "NativeRecommendedRules.ruleset" "ConcurrencyCheckRules.ruleset"
        "ConcurrencyRules.ruleset")
    set(_MERGED_RULESET_PATH "${CMAKE_BINARY_DIR}/PrismCombinedRuleset.ruleset")
    set(RULESET_IMPORTS "")
    foreach(_RULESET ${_RULESET_FILES})
      set(RULESET_IMPORTS
          "${RULESET_IMPORTS}    <Include Path=\"${_RULESET_DIR}\\${_RULESET}\" Action=\"Default\" />\n"
      )
    endforeach()
    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PrismRulesetTemplate.ruleset.in"
      "${_MERGED_RULESET_PATH}" @ONLY)
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/nativecodeanalysis")
    set_property(TARGET ${PROJECT_NAME} PROPERTY COMPILE_WARNING_AS_ERROR ON)
    set_property(TARGET ${PROJECT_NAME} PROPERTY COMPILE_WARNING_LEVEL 4)
    target_compile_options(
      ${PROJECT_NAME}
      PRIVATE /external:anglebrackets
              /external:W0
              "/external:I ${CMAKE_BINARY_DIR}/generated"
              "/external:I ${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw"
              /analyze
              /analyze:external-
              /analyze:log
              "${CMAKE_BINARY_DIR}/nativecodeanalysis/"
              /analyze:log:format:sarif
              /analyze:ruleset
              "${_MERGED_RULESET_PATH}")
    foreach(src IN LISTS SKIP_LINT_FILES)
      set_property(
        SOURCE "${src}"
        APPEND
        PROPERTY COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:MSVC>:/analyze->"
                 "$<$<CXX_COMPILER_ID:MSVC>:/WX->"
                 "$<$<CXX_COMPILER_ID:MSVC>:/W0>")
    endforeach()
  endif()
endif()
if(NOT ANDROID)
  check_ipo_supported(RESULT IPO_SUPPORTED)
  if(IPO_SUPPORTED)
    set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION
                                                 TRUE)
  else()
    message(STATUS "IPO not supported for this build")
  endif()
endif()
if(PRISM_ENABLE_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
if(PRISM_ENABLE_DEMOS)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/demos)
endif()
if(DEFINED SKBUILD)
  set(PRISM_PY_NATIVE_DIR "${SKBUILD_PLATLIB_DIR}/prism/_native")
  install(
    TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION "${PRISM_PY_NATIVE_DIR}"
    LIBRARY DESTINATION "${PRISM_PY_NATIVE_DIR}"
    ARCHIVE DESTINATION "${PRISM_PY_NATIVE_DIR}")
  if(WIN32)
    if(_comp_arch_id_lc STREQUAL "x86" OR _comp_arch_id_lc STREQUAL "i386")
      install(
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/ZDSRAPI.dll"
              "${CMAKE_CURRENT_SOURCE_DIR}/SAAPI32.dll"
        DESTINATION "${PRISM_PY_NATIVE_DIR}"
        OPTIONAL)
    elseif(NOT _comp_arch_id_lc MATCHES "arm64|aarch64|arm64ec")
      install(
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/ZDSRAPI_x64.dll"
              "${CMAKE_CURRENT_SOURCE_DIR}/SAAPI64.dll"
        DESTINATION "${PRISM_PY_NATIVE_DIR}"
        OPTIONAL)
    endif()
  endif()
else()
  install(
    TARGETS ${PROJECT_NAME}
    EXPORT prismTargets
    RUNTIME DESTINATION "bin"
    LIBRARY DESTINATION "lib"
    ARCHIVE DESTINATION "lib")
  if(MSVC AND BUILD_SHARED_LIBS)
    install(
      FILES $<TARGET_PDB_FILE:${PROJECT_NAME}>
      DESTINATION "bin"
      OPTIONAL)
  endif()
  install(
    EXPORT prismTargets
    FILE prism-config.cmake
    NAMESPACE prism::
    DESTINATION share/prism)
  include(CMakePackageConfigHelpers)
  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/prism-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/prism-config-version.cmake"
          DESTINATION share/prism)
  install(DIRECTORY include/ DESTINATION include)
endif()
if(PRISM_ENABLE_GDEXTENSION)
  add_subdirectory(gdextension)
endif()
if(PRISM_ENABLE_SHIMS)
  add_subdirectory(shims)
endif()
if(PRISM_BUILD_WINELIBS)
  if(WIN32
     OR APPLE
     OR ANDROID
     OR EMSCRIPTEN)
    message(FATAL_ERROR "PRISM_BUILD_WINELIBS requires a Linux/BSD host build")
  endif()
  include(ExternalProject)
  ExternalProject_Add(
    prism_winelibs
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/winelibs
    CMAKE_ARGS
      -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/winelib-toolchain.cmake
      -DCMAKE_BUILD_TYPE=$<CONFIG>
      -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
      -DPRISM_DBUS_XML_DIR=${CMAKE_CURRENT_SOURCE_DIR}/dbus
    INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/winelibs_install
    BUILD_ALWAYS OFF)
  install(
    DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/winelibs_install/
    DESTINATION .
    USE_SOURCE_PERMISSIONS)
endif()
