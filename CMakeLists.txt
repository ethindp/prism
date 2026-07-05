# SPDX-License-Identifier: MPL-2.0
cmake_minimum_required(VERSION 3.24)

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(CheckLinkerFlag)
include(CheckIPOSupported)
include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)
include(CMakePackageConfigHelpers)
include(ExternalProject)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

project(
  prism
  VERSION 0.16.7
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
  option(
    PRISM_VCPKG_WANTS_SPIEL_BACKEND
    "Indicates that the Prism vcpkg port is being built with the spiel feature enabled"
    OFF)
endif()
option(PRISM_ENABLE_GDEXTENSION "Enable building of the Godot GDExtension" ON)
option(PRISM_ENABLE_POWER_MANAGEMENT
       "Enable OS power-state-aware availability polling where supported." ON)
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
option(PRISM_DIAGNOSE_VECTORIZATION
       "Emit vectorization reports for optimized builds" OFF)
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
  list(APPEND _PRISM_WIN_IMPORT_LIBS "${target}")
  set(_PRISM_WIN_IMPORT_LIBS
      "${_PRISM_WIN_IMPORT_LIBS}"
      PARENT_SCOPE)
endfunction()
function(prism_add_backend src)
  get_filename_component(name "${src}" NAME_WE)
  set(tgt "prism_backend_${name}")
  add_library(${tgt} OBJECT "${src}")
  target_link_libraries(${tgt} PRIVATE prism_backend_iface)
  set_target_properties(
    ${tgt}
    PROPERTIES POSITION_INDEPENDENT_CODE ON
               CXX_VISIBILITY_PRESET hidden
               C_VISIBILITY_PRESET hidden
               VISIBILITY_INLINES_HIDDEN ON)
  list(APPEND _prism_backend_targets ${tgt})
  set(_prism_backend_targets
      "${_prism_backend_targets}"
      PARENT_SCOPE)
endfunction()
function(prism_target_external_include target visibility)
  set(_wrapped "")
  foreach(_dir IN LISTS ARGN)
    list(APPEND _wrapped "$<BUILD_INTERFACE:${_dir}>")
  endforeach()
  target_include_directories(${target} SYSTEM ${visibility} ${_wrapped})
  if(PRISM_ENABLE_LINTING AND MSVC)
    foreach(_dir IN LISTS ARGN)
      target_compile_options(${target} ${visibility} "/external:I ${_dir}")
    endforeach()
  endif()
endfunction()
add_library(prism_backend_iface INTERFACE)
target_compile_features(prism_backend_iface INTERFACE cxx_std_23 c_std_17)
set(_prism_backend_targets "")
file(
  GLOB_RECURSE
  headers
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/source/*.hpp")
file(GLOB_RECURSE android_headers CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/android/*.hpp")
file(GLOB_RECURSE android_sources CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/android/*.cpp")
file(GLOB_RECURSE sources CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/source/*.c")
file(GLOB backend_sources CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/*.cpp")
list(REMOVE_ITEM backend_sources
     "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/custom_backend.cpp")
list(REMOVE_ITEM sources ${backend_sources})
foreach(src ${backend_sources})
  prism_add_backend("${src}")
endforeach()
add_library(prism_third_party_simdutf OBJECT third_party/simdutf/simdutf.cpp)
prism_target_external_include(prism_third_party_simdutf PUBLIC
                              "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set_target_properties(prism_third_party_simdutf
                      PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  target_compile_options(prism_third_party_simdutf PRIVATE /analyze- /W0 /WX-)
else()
  target_compile_options(prism_third_party_simdutf PRIVATE -w)
endif()
add_library(prism_third_party_dr_wav OBJECT third_party/dr_wav/wav.c)
prism_target_external_include(prism_third_party_dr_wav PUBLIC
                              "${CMAKE_CURRENT_SOURCE_DIR}/third_party/dr_wav")
set_target_properties(prism_third_party_dr_wav
                      PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  target_compile_options(prism_third_party_dr_wav PRIVATE /analyze- /W0 /WX-)
else()
  target_compile_options(prism_third_party_dr_wav PRIVATE -w)
endif()
add_library(prism_third_party_fmt OBJECT third_party/fmt/src/format.cc
                                         third_party/fmt/src/os.cc)
prism_target_external_include(
  prism_third_party_fmt PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt/include")
set_target_properties(prism_third_party_fmt PROPERTIES POSITION_INDEPENDENT_CODE
                                                       ON)
if(MSVC)
  target_compile_options(prism_third_party_fmt PRIVATE /analyze- /W0 /WX- /utf-8)
else()
  target_compile_options(prism_third_party_fmt PRIVATE -w)
endif()
if(ANDROID)
  add_library(
    prism_third_party_djinni OBJECT
    third_party/djinni/support/cpp/DataRef.cpp
    third_party/djinni/support/jni/djinni_support.cpp
    third_party/djinni/support/jni/djinni_main.cpp
    third_party/djinni/support/jni/DataRef_jni.cpp
    third_party/djinni/support/jni/Future_jni.cpp)
  prism_target_external_include(
    prism_third_party_djinni
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/djinni/support"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/djinni/support/cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/djinni/support/jni")
  set_target_properties(prism_third_party_djinni
                        PROPERTIES POSITION_INDEPENDENT_CODE ON)
  if(MSVC)
    target_compile_options(prism_third_party_djinni PRIVATE /analyze- /W0 /WX-)
  else()
    target_compile_options(prism_third_party_djinni PRIVATE -w)
  endif()
endif()
prism_target_external_include(
  prism_backend_iface
  INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/simdutf"
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/moderncom"
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/concurrentqueue"
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/dr_wav"
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt/include")
if(ANDROID)
  prism_target_external_include(
    prism_backend_iface
    INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/djinni/support"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/djinni/support/cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/djinni/support/jni")
endif()
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
foreach(_tgt ${_prism_backend_targets})
  target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:${_tgt}>)
endforeach()
target_link_libraries(${PROJECT_NAME}
                      PRIVATE "$<BUILD_INTERFACE:prism_backend_iface>")
target_sources(
  ${PROJECT_NAME}
  PRIVATE $<TARGET_OBJECTS:prism_third_party_simdutf>
          $<TARGET_OBJECTS:prism_third_party_dr_wav>
          $<TARGET_OBJECTS:prism_third_party_fmt>)
if(ANDROID)
  target_sources(${PROJECT_NAME}
                 PRIVATE $<TARGET_OBJECTS:prism_third_party_djinni>)
endif()
set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES POSITION_INDEPENDENT_CODE ON
             CXX_VISIBILITY_PRESET hidden
             C_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN ON)
if(EMSCRIPTEN)
  add_library(prism_emscripten_threads INTERFACE)
  target_compile_options(prism_emscripten_threads INTERFACE -pthread)
  target_link_options(prism_emscripten_threads INTERFACE -pthread)
  foreach(_t prism_third_party_simdutf prism_third_party_dr_wav
             prism_third_party_fmt ${_prism_backend_targets})
    target_link_libraries(${_t} PRIVATE prism_emscripten_threads)
  endforeach()
  target_link_libraries(${PROJECT_NAME}
                        PRIVATE "$<BUILD_INTERFACE:prism_emscripten_threads>")
endif()
if(APPLE)
  set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/avspeech.cpp" TARGET_DIRECTORY
    prism_backend_avspeech PROPERTIES COMPILE_OPTIONS
                                      "-x;objective-c++;-fobjc-arc")
  set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/voiceover.cpp" TARGET_DIRECTORY
    prism_backend_voiceover PROPERTIES COMPILE_OPTIONS
                                       "-x;objective-c++;-fobjc-arc")
  set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/source/power_notifier.cpp" TARGET_DIRECTORY
    ${PROJECT_NAME} PROPERTIES COMPILE_OPTIONS "-x;objective-c++;-fobjc-arc")
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
  ${PROJECT_NAME}
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
         "$<INSTALL_INTERFACE:include>")
target_include_directories(
  prism_backend_iface
  INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
            "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/source>"
            "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/source/backends>")
if(ANDROID)
  target_include_directories(
    ${PROJECT_NAME}
    PRIVATE "${PROJECT_SOURCE_DIR}/source/backends"
            "${PROJECT_SOURCE_DIR}/source/backends/android"
            "${PROJECT_SOURCE_DIR}/source/backends/android/jni")
endif()
target_compile_definitions(
  ${PROJECT_NAME}
  PRIVATE PRISM_BUILDING
  PUBLIC
    NOMINMAX
    $<$<STREQUAL:$<TARGET_PROPERTY:${PROJECT_NAME},TYPE>,STATIC_LIBRARY>:PRISM_STATIC>
)
target_compile_definitions(
  prism_backend_iface
  INTERFACE
    PRISM_BUILDING
    NOMINMAX
    $<$<STREQUAL:$<TARGET_PROPERTY:${PROJECT_NAME},TYPE>,STATIC_LIBRARY>:PRISM_STATIC>
)
if(PRISM_ENABLE_LEGACY_BACKENDS)
  target_compile_definitions(${PROJECT_NAME}
                             PRIVATE PRISM_ENABLE_LEGACY_BACKENDS)
  target_compile_definitions(prism_backend_iface
                             INTERFACE PRISM_ENABLE_LEGACY_BACKENDS)
  if(PRISM_ENABLE_LEGACY_SYSTEM_ACCESS_BACKEND)
    target_compile_definitions(
      ${PROJECT_NAME} PRIVATE PRISM_ENABLE_SYSTEM_ACCESS_LEGACY_BACKEND)
    target_compile_definitions(
      prism_backend_iface INTERFACE PRISM_ENABLE_SYSTEM_ACCESS_LEGACY_BACKEND)
  endif()
  if(PRISM_ENABLE_LEGACY_WINDOW_EYES_BACKEND)
    target_compile_definitions(${PROJECT_NAME}
                               PRIVATE PRISM_ENABLE_WINDOW_EYES_LEGACY_BACKEND)
    target_compile_definitions(
      prism_backend_iface INTERFACE PRISM_ENABLE_WINDOW_EYES_LEGACY_BACKEND)
  endif()
endif()
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
get_target_property(_PRISM_LIB_TYPE ${PROJECT_NAME} TYPE)
if(_PRISM_LIB_TYPE STREQUAL "STATIC_LIBRARY")
  set(_PRISM_LINK_VIS INTERFACE)
else()
  set(_PRISM_LINK_VIS PRIVATE)
endif()
set(_PRISM_WIN_IMPORT_LIBS "")
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
  if(NOT DEFINED CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
    get_filename_component(
      WIN_SDK_ROOT
      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]"
      ABSOLUTE
      CACHE)
  else()
    set(WIN_SDK_ROOT "$ENV{ProgramFiles\(x86\)}/Windows Kits/10")
    set(WIN_SDK_VERSION "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
  endif()
  find_program(
    MIDL_COMPILER
    NAMES midl.exe
    PATHS "${WIN_SDK_ROOT}/bin/${WIN_SDK_VERSION}/${_comp_arch_id_lc}"
          "${WIN_SDK_ROOT}/bin/${_sdk_arch}"
    DOC "Microsoft IDL Compiler" REQUIRED)
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
  target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${GEN_DIR})
  target_include_directories(prism_backend_iface SYSTEM
                             INTERFACE "$<BUILD_INTERFACE:${GEN_DIR}>")
  add_custom_target(prism_nvda_codegen DEPENDS "${GEN_DIR}/nvda_controller.h"
                                               "${GEN_DIR}/nvda_controller.c")
  if(TARGET prism_backend_nvda)
    add_dependencies(prism_backend_nvda prism_nvda_codegen)
  endif()
  target_link_libraries(${PROJECT_NAME} PRIVATE rpcrt4)
  if(PRISM_ENABLEPOWER_MANAGEMENT)
    target_compile_definitions(${PROJECT_NAME}
                               PRIVATE PRISM_ENABLE_POWER_MANAGEMENT)
    target_link_libraries(${PROJECT_NAME} PRIVATE PowrProf)
  endif()
  if(_comp_arch_id_lc STREQUAL "x86" OR _comp_arch_id_lc STREQUAL "i386")
    add_import_library(ZDSR ${CMAKE_CURRENT_SOURCE_DIR}/defs/zdsr32.def
                       ZDSRAPI.dll)
    add_import_library(
      byctrl ${CMAKE_CURRENT_SOURCE_DIR}/defs/boy_pc_reader32.def byctrl.dll)
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
      ${PROJECT_NAME}
      PRIVATE "$<BUILD_INTERFACE:ZDSR>"
              "$<BUILD_INTERFACE:byctrl>"
              "$<BUILD_INTERFACE:PCTalker>"
              "$<BUILD_INTERFACE:PrismOrcaBridge>"
              "$<BUILD_INTERFACE:PrismSpeechDispatcherBridge>")
  elseif(NOT _comp_arch_id_lc MATCHES "arm64|aarch64|arm64ec")
    add_import_library(ZDSR ${CMAKE_CURRENT_SOURCE_DIR}/defs/zdsr.def
                       ZDSRAPI_x64.dll)
    add_import_library(
      byctrl ${CMAKE_CURRENT_SOURCE_DIR}/defs/boy_pc_reader.def byctrl-x64.dll)
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
      ${PROJECT_NAME}
      PRIVATE "$<BUILD_INTERFACE:ZDSR>"
              "$<BUILD_INTERFACE:byctrl>"
              "$<BUILD_INTERFACE:PCTalker>"
              "$<BUILD_INTERFACE:PrismOrcaBridge>"
              "$<BUILD_INTERFACE:PrismSpeechDispatcherBridge>")
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
    target_link_libraries(
      ${PROJECT_NAME}
      PRIVATE "$<BUILD_INTERFACE:PCTalker>"
              "$<BUILD_INTERFACE:PrismOrcaBridge>"
              "$<BUILD_INTERFACE:PrismSpeechDispatcherBridge>")
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
  elseif(CMAKE_SYSTEM_NAME STREQUAL "watchOS")
    find_library(WATCHKIT_FRAMEWORK WatchKit REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${WATCHKIT_FRAMEWORK})
  else()
    find_library(UIKIT_FRAMEWORK UIKit REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${UIKIT_FRAMEWORK})
  endif()
  if(PRISM_ENABLE_POWER_MANAGEMENT AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    target_compile_definitions(${PROJECT_NAME}
                               PRIVATE PRISM_ENABLE_POWER_MANAGEMENT)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${IOKIT_FRAMEWORK}
                                                  ${COREFOUNDATION_FRAMEWORK})
  endif()
endif()
if(UNIX
   AND NOT APPLE
   AND NOT ANDROID
   AND NOT EMSCRIPTEN)
  if(PRISM_ENABLE_POWER_MANAGEMENT)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GIOMM_PM giomm-2.68>=2.68.0)
    if(GIOMM_PM_FOUND)
      target_compile_definitions(${PROJECT_NAME}
                                 PRIVATE PRISM_ENABLE_POWER_MANAGEMENT)
      target_include_directories(${PROJECT_NAME} SYSTEM
                                 PRIVATE ${GIOMM_PM_INCLUDE_DIRS})
      target_link_directories(${PROJECT_NAME} PRIVATE ${GIOMM_PM_LIBRARY_DIRS})
      target_link_libraries(${PROJECT_NAME} PRIVATE ${GIOMM_PM_LIBRARIES})
    else()
      message(
        STATUS "Prism: giomm not found; availability power management disabled")
    endif()
  endif()
  if(PRISM_ENABLE_VCPKG_SPECIFIC_OPTIONS)
    find_package(PkgConfig REQUIRED)
    if(PRISM_VCPKG_WANTS_ORCA_BACKEND)
      pkg_check_modules(GLIBMM glibmm-2.68>=2.68.0 REQUIRED)
      pkg_check_modules(GIOMM giomm-2.68>=2.68.0 REQUIRED)
      target_include_directories(
        ${PROJECT_NAME} SYSTEM PRIVATE ${GLIBMM_INCLUDE_DIRS}
                                       ${GIOMM_INCLUDE_DIRS})
      target_include_directories(
        prism_backend_iface SYSTEM INTERFACE ${GLIBMM_INCLUDE_DIRS}
                                             ${GIOMM_INCLUDE_DIRS})
      target_link_directories(${PROJECT_NAME} PRIVATE ${GLIBMM_LIBRARY_DIRS}
                              ${GIOMM_LIBRARY_DIRS})
      target_link_libraries(${PROJECT_NAME} PRIVATE ${GLIBMM_LIBRARIES}
                                                    ${GIOMM_LIBRARIES})
    else()
      target_compile_definitions(${PROJECT_NAME} PRIVATE NO_ORCA)
      target_compile_definitions(prism_backend_iface INTERFACE NO_ORCA)
    endif()
    if(PRISM_VCPKG_WANTS_SPEECH_DISPATCHER_BACKEND)
      pkg_check_modules(LIBSPEECHD speech-dispatcher REQUIRED)
      target_include_directories(
        ${PROJECT_NAME} SYSTEM PRIVATE ${LIBSPEECHD_INCLUDE_DIRS}
                                       ${CMAKE_CURRENT_BINARY_DIR})
      target_include_directories(prism_backend_iface SYSTEM
                                 INTERFACE ${LIBSPEECHD_INCLUDE_DIRS})
      target_link_directories(${PROJECT_NAME} PRIVATE
                              ${LIBSPEECHD_LIBRARY_DIRS})
      target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBSPEECHD_LIBRARIES})
    else()
      target_compile_definitions(${PROJECT_NAME} PRIVATE NO_LIBSPEECHD)
      target_compile_definitions(prism_backend_iface INTERFACE NO_LIBSPEECHD)
    endif()
    if(PRISM_VCPKG_WANTS_SPIEL_BACKEND)
      pkg_check_modules(SPIEL spiel-1.0 REQUIRED)
      target_include_directories(${PROJECT_NAME} SYSTEM
                                 PRIVATE ${SPIEL_INCLUDE_DIRS})
      target_include_directories(prism_backend_iface SYSTEM
                                 INTERFACE ${SPIEL_INCLUDE_DIRS})
      target_link_directories(${PROJECT_NAME} PRIVATE ${SPIEL_LIBRARY_DIRS})
      target_link_libraries(${PROJECT_NAME} PRIVATE ${SPIEL_LIBRARIES})
    else()
      target_compile_definitions(${PROJECT_NAME} PRIVATE NO_LIBSPIEL)
      target_compile_definitions(prism_backend_iface INTERFACE NO_LIBSPIEL)
    endif()
  else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLIBMM glibmm-2.68>=2.68.0)
    pkg_check_modules(GIOMM giomm-2.68>=2.68.0)
    if(GLIBMM_FOUND AND GIOMM_FOUND)
      target_include_directories(
        ${PROJECT_NAME} SYSTEM PRIVATE ${GLIBMM_INCLUDE_DIRS}
                                       ${GIOMM_INCLUDE_DIRS})
      target_include_directories(
        prism_backend_iface SYSTEM INTERFACE ${GLIBMM_INCLUDE_DIRS}
                                             ${GIOMM_INCLUDE_DIRS})
      target_link_directories(${PROJECT_NAME} PRIVATE ${GLIBMM_LIBRARY_DIRS}
                              ${GIOMM_LIBRARY_DIRS})
      target_link_libraries(${PROJECT_NAME} PRIVATE ${GLIBMM_LIBRARIES}
                                                    ${GIOMM_LIBRARIES})
    else()
      target_compile_definitions(${PROJECT_NAME} PRIVATE NO_ORCA)
      target_compile_definitions(prism_backend_iface INTERFACE NO_ORCA)
    endif()
    pkg_check_modules(LIBSPEECHD speech-dispatcher)
    if(LIBSPEECHD_FOUND)
      target_include_directories(
        ${PROJECT_NAME} SYSTEM PRIVATE ${LIBSPEECHD_INCLUDE_DIRS}
                                       ${CMAKE_CURRENT_BINARY_DIR})
      target_include_directories(prism_backend_iface SYSTEM
                                 INTERFACE ${LIBSPEECHD_INCLUDE_DIRS})
      target_link_directories(${PROJECT_NAME} PRIVATE
                              ${LIBSPEECHD_LIBRARY_DIRS})
      target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBSPEECHD_LIBRARIES})
    else()
      target_compile_definitions(${PROJECT_NAME} PRIVATE NO_LIBSPEECHD)
      target_compile_definitions(prism_backend_iface INTERFACE NO_LIBSPEECHD)
    endif()
    pkg_check_modules(SPIEL spiel-1.0)
    if(SPIEL_FOUND)
      target_include_directories(${PROJECT_NAME} SYSTEM
                                 PRIVATE ${SPIEL_INCLUDE_DIRS})
      target_include_directories(prism_backend_iface SYSTEM
                                 INTERFACE ${SPIEL_INCLUDE_DIRS})
      target_link_directories(${PROJECT_NAME} PRIVATE ${SPIEL_LIBRARY_DIRS})
      target_link_libraries(${PROJECT_NAME} PRIVATE ${SPIEL_LIBRARIES})
    else()
      target_compile_definitions(${PROJECT_NAME} PRIVATE NO_LIBSPIEL)
      target_compile_definitions(prism_backend_iface INTERFACE NO_LIBSPIEL)
    endif()
  endif()
endif()
if(WIN32 OR WINDOWS_STORE)
  target_compile_definitions(
    ${PROJECT_NAME}
    PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1
            _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT=1
            BELT_COM_NO_LEAK_DETECTION UNICODE _UNICODE)
  target_compile_definitions(
    prism_backend_iface
    INTERFACE _CRT_SECURE_NO_WARNINGS _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1
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
      /delayload:byctrl.dll
      /delayload:PCTKUSR.dll
      /delayload:prism_orca_bridge.dll
      /delayload:prism_speech_dispatcher_bridge.dll)
  elseif(NOT _comp_arch_id_lc MATCHES "arm64|aarch64|arm64ec")
    target_link_options(
      ${PROJECT_NAME}
      ${_PRISM_LINK_VIS}
      /delayload:ZDSRAPI_x64.dll
      /delayload:byctrl-x64.dll
      /delayload:PCTKUSR.dll
      /delayload:prism_orca_bridge.dll
      /delayload:prism_speech_dispatcher_bridge.dll)
  else()
    target_link_options(
      ${PROJECT_NAME} ${_PRISM_LINK_VIS} /delayload:PCTKUSR.dll
      /delayload:prism_orca_bridge.dll
      /delayload:prism_speech_dispatcher_bridge.dll)
  endif()
  check_cxx_compiler_flag(/utf-8 MSVC_HAS_UTF8_OPTION)
  check_cxx_compiler_flag(/bigobj MSVC_HAS_BIGOBJ)
  check_cxx_compiler_flag(/Zc:enumTypes MSVC_HAS_ZC_ENUM_TYPES)
  if(MSVC_HAS_UTF8_OPTION)
    target_compile_options(${PROJECT_NAME}
                           PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:/utf-8>)
    target_compile_options(prism_backend_iface
                           INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:/utf-8>)
  endif()
  if(MSVC_HAS_BIGOBJ)
    target_compile_options(${PROJECT_NAME}
                           PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:/bigobj>)
    target_compile_options(prism_backend_iface
                           INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:/bigobj>)
  endif()
  if(MSVC_HAS_ZC_ENUM_TYPES)
    target_compile_options(${PROJECT_NAME}
                           PUBLIC $<$<COMPILE_LANGUAGE:C,CXX>:/Zc:enumTypes>)
    target_compile_options(prism_backend_iface
                           INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:/Zc:enumTypes>)
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
      "LINKER:/delayload:byctrl.dll"
      "LINKER:/delayload:PCTKUSR.dll"
      "LINKER:/delayload:prism_orca_bridge.dll"
      "LINKER:/delayload:prism_speech_dispatcher_bridge.dll")
  elseif(NOT _comp_arch_id_lc MATCHES "arm64|aarch64|arm64ec")
    target_link_options(${PROJECT_NAME} PRIVATE "LINKER:/STACK:8388608")
    target_link_options(
      ${PROJECT_NAME}
      ${_PRISM_LINK_VIS}
      "LINKER:/delayload:ZDSRAPI_x64.dll"
      "LINKER:/delayload:byctrl-x64.dll"
      "LINKER:/delayload:PCTKUSR.dll"
      "LINKER:/delayload:prism_orca_bridge.dll"
      "LINKER:/delayload:prism_speech_dispatcher_bridge.dll")
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
  if(COMP_HAS_WALL_OPTION AND COMP_HAS_WEXTRA_OPTION)
    target_compile_options(${PROJECT_NAME}
                           PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
    target_compile_options(prism_backend_iface
                           INTERFACE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
  endif()
  if(COMP_HAS_WTHREAD_SAFETY_OPTION)
    target_compile_options(
      ${PROJECT_NAME}
      PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-Wthread-safety>
              $<$<COMPILE_LANGUAGE:C,CXX>:-Werror=thread-safety>)
    target_compile_options(
      prism_backend_iface
      INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:-Wthread-safety>
                $<$<COMPILE_LANGUAGE:C,CXX>:-Werror=thread-safety>)
  endif()
endif()
if(APPLE)
  target_link_options(${PROJECT_NAME} PRIVATE
                      "LINKER:-headerpad_max_install_names")
endif()
if(ANDROID)
  target_link_options(${PROJECT_NAME} PRIVATE "-Wl,-z,max-page-size=16384")
endif()
if(PRISM_DIAGNOSE_VECTORIZATION)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(
      ${PROJECT_NAME} PRIVATE $<$<NOT:$<CONFIG:Debug>>:-fopt-info-vec-missed=2>)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(
      ${PROJECT_NAME}
      PRIVATE $<$<NOT:$<CONFIG:Debug>>:-Rpass=loop-unroll
              -Rpass-missed=loop-unroll
              -Rpass=loop-vectorize
              -Rpass-missed=loop-vectorize
              -Rpass-analysis=loop-vectorize
              -Rpass-missed=slp-vectorize
              -Rpass-analysis=slp-vectorize
              -Rpass=slp-vectorize>)
  elseif(MSVC)
    target_compile_options(${PROJECT_NAME}
                           PRIVATE $<$<NOT:$<CONFIG:Debug>>:/Qvec-report:2>)
  endif()
endif()
if(PRISM_ENABLE_LINTING)
  file(
    GLOB_RECURSE
    SKIP_LINT_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/**")
  if(ANDROID)
    list(APPEND SKIP_LINT_FILES ${android_sources} ${android_headers})
  endif()
  if(WIN32)
    list(APPEND SKIP_LINT_FILES "${GEN_DIR}/nvda_controller.c")
  endif()
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy-22" "clang-tidy-21"
                                      "clang-tidy-20" "clang-tidy")
    if(CLANG_TIDY_EXE)
      set(CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}")
      set_target_properties(${PROJECT_NAME} PROPERTIES CXX_CLANG_TIDY
                                                       "${CLANG_TIDY_COMMAND}")
      foreach(_tgt ${_prism_backend_targets})
        set_target_properties(${_tgt} PROPERTIES CXX_CLANG_TIDY
                                                 "${CLANG_TIDY_COMMAND}")
      endforeach()
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
        "NativeRecommendedRules.ruleset"
        "ConcurrencyCheckRules.ruleset"
        "ConcurrencyRules.ruleset"
        "CppCoreCheckRules.ruleset"
        "ExtendedCorrectnessRules.ruleset"
        "ExtendedDesignGuidelineRules.ruleset"
        "GlobalizationRules.ruleset"
        "SecurityRules.ruleset")
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
    foreach(_tgt ${_prism_backend_targets})
      set_property(TARGET ${_tgt} PROPERTY COMPILE_WARNING_AS_ERROR ON)
      set_property(TARGET ${_tgt} PROPERTY COMPILE_WARNING_LEVEL 4)
    endforeach()
    target_compile_options(
      prism_backend_iface
      INTERFACE /external:anglebrackets
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
        PROPERTY COMPILE_OPTIONS /analyze- /WX- /W0)
    endforeach()
  endif()
endif()
if(NOT ANDROID)
  check_ipo_supported(RESULT IPO_SUPPORTED)
  if(IPO_SUPPORTED AND NOT PRISM_DIAGNOSE_VECTORIZATION)
    set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION
                                                 TRUE)
    foreach(_tgt ${_prism_backend_targets})
      set_property(TARGET ${_tgt} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endforeach()
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
set_target_properties(${PROJECT_NAME} PROPERTIES EXPORT_NAME _prism_lib)
add_library(${PROJECT_NAME}::_prism_lib ALIAS ${PROJECT_NAME})
add_library(${PROJECT_NAME}_iface INTERFACE)
get_target_property(_prism_type ${PROJECT_NAME} TYPE)
if(_prism_type STREQUAL "STATIC_LIBRARY")
  target_link_libraries(
    ${PROJECT_NAME}_iface
    INTERFACE
      "$<BUILD_INTERFACE:$<LINK_LIBRARY:WHOLE_ARCHIVE,${PROJECT_NAME}>>"
      "$<INSTALL_INTERFACE:$<LINK_LIBRARY:WHOLE_ARCHIVE,${PROJECT_NAME}::_prism_lib>>"
  )
else()
  target_link_libraries(
    ${PROJECT_NAME}_iface
    INTERFACE "$<BUILD_INTERFACE:${PROJECT_NAME}>"
              "$<INSTALL_INTERFACE:${PROJECT_NAME}::_prism_lib>")
endif()
unset(_prism_type)
set_target_properties(${PROJECT_NAME}_iface PROPERTIES EXPORT_NAME
                                                       ${PROJECT_NAME})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME}_iface)
if(DEFINED SKBUILD)
  set(PRISM_PY_NATIVE_DIR "${SKBUILD_PLATLIB_DIR}/prism/_native")
  install(
    TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION "${PRISM_PY_NATIVE_DIR}"
    LIBRARY DESTINATION "${PRISM_PY_NATIVE_DIR}"
    ARCHIVE DESTINATION "${PRISM_PY_NATIVE_DIR}")
else()
  install(
    TARGETS ${PROJECT_NAME} ${PROJECT_NAME}_iface
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
    FILE prism-targets.cmake
    NAMESPACE prism::
    DESTINATION share/prism)
  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/prism-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
  install(DIRECTORY include/ DESTINATION include)
  if(WIN32 AND _PRISM_LIB_TYPE STREQUAL "STATIC_LIBRARY")
    foreach(_imp IN LISTS _PRISM_WIN_IMPORT_LIBS)
      install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${_imp}.lib" DESTINATION lib)
    endforeach()
    set(PRISM_WIN_STATIC_IMPORT_LIBS "${_PRISM_WIN_IMPORT_LIBS}")
  else()
    set(PRISM_WIN_STATIC_IMPORT_LIBS "")
  endif()
  configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/prism-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/prism-config.cmake"
    INSTALL_DESTINATION share/prism)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/prism-config.cmake"
                "${CMAKE_CURRENT_BINARY_DIR}/prism-config-version.cmake"
          DESTINATION share/prism)
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
  ExternalProject_Add(
    prism_winelibs
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/winelibs
    CMAKE_ARGS
      -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/winelib-toolchain.cmake
      -DCMAKE_BUILD_TYPE=$<CONFIG>
      -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
      -DPRISM_ENABLE_LINTING=${PRISM_ENABLE_LINTING}
    INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/winelibs_install
    BUILD_ALWAYS OFF)
  install(
    DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/winelibs_install/
    DESTINATION .
    USE_SOURCE_PERMISSIONS)
endif()
