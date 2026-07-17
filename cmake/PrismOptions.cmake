# SPDX-License-Identifier: MPL-2.0

include_guard(GLOBAL)
include(PrismGuards)
option(PRISM_ENABLE_TESTS "Enable tests" OFF)
option(PRISM_ENABLE_DEMOS "Enable demo applications" OFF)
option(PRISM_ENABLE_GDEXTENSION "Enable building of the Godot GDExtension" OFF)
option(
  PRISM_ENABLE_SHIMS
  "Enable shim shared libraries for compatibility with older screen reader libraries"
  OFF)
option(PRISM_ENABLE_TOLK_SHIM "Enable the Tolk compatibility shim" ON)
option(PRISM_ENABLE_LEGACY_BACKENDS
       "Build backends for discontinued screen readers" OFF)
option(PRISM_ENABLE_POWER_MANAGEMENT
       "Enable OS power-state-aware availability polling where supported" ON)
option(PRISM_ENABLE_LINTING
       "Run available linters as part of the build (implies warnings as errors)"
       OFF)
option(PRISM_DIAGNOSE_VECTORIZATION
       "Emit vectorization reports for optimized builds" OFF)
option(PRISM_REGENERATE_DJINNI
       "Re-derive the committed Djinni bindings from the IDL (MAINTAINER ONLY)"
       OFF)
option(PRISM_BUILD_WINELIBS
       "Build Winelib bridges so Wine-hosted Prism can reach Linux TTS hosts"
       OFF)
set(PRISM_DEPENDENCY_PROVIDER
    "BUNDLED"
    CACHE
      STRING
      "Default provider for vendorable third-party dependencies (BUNDLED or SYSTEM)"
)
set_property(CACHE PRISM_DEPENDENCY_PROVIDER PROPERTY STRINGS BUNDLED SYSTEM)
prism_require_enum(PRISM_DEPENDENCY_PROVIDER BUNDLED SYSTEM)
set(PRISM_BACKEND_DEFAULT
    "AUTO"
    CACHE STRING "Default state for every backend (AUTO, ON or OFF)")
set_property(CACHE PRISM_BACKEND_DEFAULT PROPERTY STRINGS AUTO ON OFF)
prism_require_enum(PRISM_BACKEND_DEFAULT AUTO ON OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT EMSCRIPTEN)
  set(BUILD_SHARED_LIBS
      ON
      CACHE BOOL "Build shared libraries")
endif()
if(WIN32)
  if(PRISM_ENABLE_GDEXTENSION)
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreadedDLL"
        CACHE STRING "Match godot-cpp non-debug CRT")
  elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreadedDebug"
        CACHE STRING "MSVC CRT library type")
  else()
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded"
        CACHE STRING "MSVC CRT library type")
  endif()
endif()
if(PRISM_BUILD_WINELIBS
   AND (WIN32
        OR APPLE
        OR ANDROID
        OR EMSCRIPTEN
       ))
  message(FATAL_ERROR "PRISM_BUILD_WINELIBS requires a Linux/BSD host build")
endif()
if(PRISM_BUILD_WINELIBS
   AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64|i[3-6]86)$")
  message(FATAL_ERROR
          "PRISM_BUILD_WINELIBS requires an x86 host; winegcc cannot build "
          "Winelib objects on ${CMAKE_SYSTEM_PROCESSOR}")
endif()
