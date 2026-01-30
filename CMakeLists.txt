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
  VERSION 0.6.1
  DESCRIPTION "The Platform-agnostic Reader Interface for Speech and Messages"
  LANGUAGES C CXX)
option(PRISM_ENABLE_TESTS "Enable tests" OFF)
option(PRISM_ENABLE_DEMOS "Enable demo applications" OFF)
option(PRISM_ENABLE_CLANG_TIDY
       "Enable execution of clang-tidy for debug builds" OFF)
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
if(APPLE)
  enable_language(Swift)
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
  list(REMOVE_ITEM sources
       "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/fsapi.c"
       "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/wineyes.c")
endif()
if(NOT ANDROID)
  list(REMOVE_ITEM sources ${android_sources})
  list(REMOVE_ITEM headers ${android_headers})
endif()
if(NOT EMSCRIPTEN)
  add_library(${PROJECT_NAME} SHARED ${headers} ${sources})
else()
  add_library(${PROJECT_NAME} STATIC ${headers} ${sources})
endif()
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/source/simdutf.cpp
                            PROPERTIES COMPILE_OPTIONS "-w")
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
  PUBLIC NOMINMAX)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
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
  if(NOT _comp_arch_id_lc MATCHES "arm64|aarch64|arm64ec")
    add_import_library(SystemAccess ${CMAKE_CURRENT_SOURCE_DIR}/defs/sa.def
                       SAAPI64.dll)
    add_import_library(ZDSR ${CMAKE_CURRENT_SOURCE_DIR}/defs/zdsr.def
                       ZDSRAPI_x64.dll)
    target_link_libraries(${PROJECT_NAME} PRIVATE SystemAccess ZDSR)
  endif()
endif()
if(APPLE)
  find_library(AVFOUNDATION_FRAMEWORK AVFoundation REQUIRED)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${AVFOUNDATION_FRAMEWORK})
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
  if(NOT _comp_arch_id_lc MATCHES "arm64|aarch64|arm64ec")
    set_target_properties(
      ${PROJECT_NAME}
      PROPERTIES
        LINK_FLAGS
        "/STACK:8388608 /delayload:SAAPI64.dll /delayload:ZDSRAPI_x64.dll")
  else()
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS
                                                     "/STACK:8388608")
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
  if(NOT _comp_arch_id_lc MATCHES "arm64|aarch64|arm64ec")
    target_link_options(
      ${PROJECT_NAME} PRIVATE "LINKER:/STACK:8388608"
      "LINKER:/delayload:SAAPI64.dll" "LINKER:/delayload:zdsrapi_x64.dll")
  else()
    target_link_options(${PROJECT_NAME} PRIVATE "LINKER:/STACK:8388608")
  endif()
endif()
if(NOT MSVC)
  check_cxx_compiler_flag(-Wall COMP_HAS_WALL_OPTION)
  check_cxx_compiler_flag(-Wextra COMP_HAS_WEXTRA_OPTION)
  if(COMP_HAS_WALL_OPTION AND COMP_HAS_WEXTRA_OPTION)
    target_compile_options(${PROJECT_NAME}
                           PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
  endif()
endif()
if(APPLE)
  target_link_options(${PROJECT_NAME} PRIVATE
                      "LINKER:-headerpad_max_install_names")
  set_target_properties(
    prism PROPERTIES BUILD_RPATH "@loader_path;/usr/lib/swift"
                     INSTALL_RPATH "@loader_path;/usr/lib/swift")
endif()
if(ANDROID)
  target_link_options(${PROJECT_NAME} PRIVATE "-Wl,-z,max-page-size=16384")
endif()
if(PRISM_ENABLE_CLANG_TIDY)
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT CMAKE_CXX_COMPILER_ID
                                                 MATCHES "MSVC")
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
    if(CLANG_TIDY_EXE)
      set(CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}")
      set_target_properties(${PROJECT_NAME} PROPERTIES CXX_CLANG_TIDY
                                                       "${CLANG_TIDY_COMMAND}")
      set(SKIP_LINT_FILES
          "${CMAKE_CURRENT_SOURCE_DIR}/source/simdutf.cpp"
          "${CMAKE_CURRENT_SOURCE_DIR}/source/simdutf.h"
          "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/dr_wav.h"
          "${CMAKE_CURRENT_SOURCE_DIR}/source/wav.c"
          "${CMAKE_CURRENT_SOURCE_DIR}/source/concurrentqueue.h"
          "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/fsapi.c"
          "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/fsapi.h"
          "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/orca-module.c"
          "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/orca-module.h"
          "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/orca-service.c"
          "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/orca-service.h"
          "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/wineyes.c"
          "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/wineyes.h"
          "${CMAKE_CURRENT_SOURCE_DIR}/source/backends/raw/zt.h")
      if(ANDROID)
        list(APPEND SKIP_LINT_FILES ${android_sources} ${android_headers})
      endif()
      set_source_files_properties(${SKIP_LINT_FILES} PROPERTIES SKIP_LINTING ON)
    endif()
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
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_INSTALL_DEBUG_LIBRARIES ON)
else()
  set(CMAKE_INSTALL_DEBUG_LIBRARIES OFF)
endif()
set(CMAKE_INSTALL_UCRT_LIBRARIES ON)
include(InstallRequiredSystemLibraries)
if(PRISM_ENABLE_TESTS)
  enable_testing()
  include(cmake/CPM.cmake)
  cpmaddpackage(
    NAME
    gtest
    GITHUB_REPOSITORY
    google/googletest
    VERSION
    1.17.0
    OPTIONS
    "INSTALL_GTEST OFF"
    "gtest_force_shared_crt ON")
  include(GoogleTest)
  file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
  foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE GTest::gtest_main prism)
    if(WIN32)
      target_link_libraries(${TEST_NAME} PRIVATE Rpcrt4.lib)
    endif()
    target_include_directories(${TEST_NAME}
                               PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    if(MSVC)
      target_compile_options(${TEST_NAME} PRIVATE /W4)
    else()
      target_compile_options(${TEST_NAME} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    gtest_discover_tests(${TEST_NAME})
  endforeach()
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
    if(NOT _comp_arch_id_lc MATCHES "arm64|aarch64|arm64ec")
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
  if(WIN32)
    if(NOT _comp_arch_id_lc MATCHES "arm64|aarch64|arm64ec")
      install(
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/ZDSRAPI_x64.dll"
              "${CMAKE_CURRENT_SOURCE_DIR}/SAAPI64.dll"
        DESTINATION "bin"
        OPTIONAL)
    endif()
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
