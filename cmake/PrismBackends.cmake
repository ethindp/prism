# SPDX-License-Identifier: MPL-2.0

include_guard(GLOBAL)
include(PrismGuards)
prism_require_vars(PRISM_SOURCE_ROOT PRISM_ARCH_CLASS PRISM_BACKEND_DEFAULT
                   PRISM_USE_IPO)
prism_require_targets(prism prism_common)
set(PRISM_BACKEND_TARGETS "")
set(PRISM_BACKEND_SUMMARY "")
set(PRISM_BACKEND_ANCHORS "")

function(prism_declare_backend NAME)
  cmake_parse_arguments(
    PB "LEGACY" "SOURCE;DOC;DEFAULT;LANGUAGE;FEATURE"
    "PLATFORM;ARCH;PKG_CONFIG;RUNTIME_PKG_CONFIG;DEFINES" ${ARGN})
  if(PB_UNPARSED_ARGUMENTS)
    message(
      FATAL_ERROR
        "prism_declare_backend(${NAME}): unknown args: ${PB_UNPARSED_ARGUMENTS}"
    )
  endif()
  if(NOT PB_SOURCE OR NOT PB_DOC)
    message(
      FATAL_ERROR "prism_declare_backend(${NAME}): SOURCE and DOC are required")
  endif()
  if(NOT DEFINED PB_DEFAULT OR PB_DEFAULT STREQUAL "")
    set(PB_DEFAULT "${PRISM_BACKEND_DEFAULT}")
  endif()
  string(TOUPPER "${NAME}" _UP)
  set(_opt PRISM_ENABLE_${_UP}_BACKEND)
  set(${_opt}
      "${PB_DEFAULT}"
      CACHE STRING "${PB_DOC} (AUTO, ON or OFF)")
  set_property(CACHE ${_opt} PROPERTY STRINGS AUTO ON OFF)
  prism_require_enum(${_opt} AUTO ON OFF)
  if(${_opt} STREQUAL "OFF")
    list(APPEND PRISM_BACKEND_SUMMARY "${NAME}=off")
    set(PRISM_BACKEND_SUMMARY
        "${PRISM_BACKEND_SUMMARY}"
        PARENT_SCOPE)
    return()
  endif()
  set(_missing "")
  set(_libs "")
  set(_headers "")
  if(PB_PLATFORM)
    set(_this "")
    if(PRISM_XBOX)
      set(_this XBOX CONSOLE)
    elseif(WIN32)
      set(_this WINDOWS)
    elseif(ANDROID)
      set(_this ANDROID)
    elseif(EMSCRIPTEN)
      set(_this EMSCRIPTEN)
    elseif(APPLE)
      set(_this APPLE)
      if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        list(APPEND _this DARWIN)
      endif()
    elseif(UNIX)
      set(_this UNIX)
    endif()
    set(_hit OFF)
    foreach(_p IN LISTS PB_PLATFORM)
      if(_p IN_LIST _this)
        set(_hit ON)
      endif()
    endforeach()
    if(NOT _hit)
      set(PRISM_BACKEND_SUMMARY
          "${PRISM_BACKEND_SUMMARY}"
          PARENT_SCOPE)
      return()
    endif()
  endif()
  if(PB_LEGACY AND NOT PRISM_ENABLE_LEGACY_BACKENDS)
    if(${_opt} STREQUAL "ON")
      message(
        FATAL_ERROR
          "${_opt}=ON but PRISM_ENABLE_LEGACY_BACKENDS is OFF. ${NAME} is a legacy backend."
      )
    endif()
    list(APPEND PRISM_BACKEND_SUMMARY "${NAME}=legacy-off")
    set(PRISM_BACKEND_SUMMARY
        "${PRISM_BACKEND_SUMMARY}"
        PARENT_SCOPE)
    return()
  endif()
  if(PB_ARCH AND NOT PRISM_ARCH_CLASS IN_LIST PB_ARCH)
    string(REPLACE ";" "/" _a "${PB_ARCH}")
    list(APPEND _missing "${_a} target (this is ${PRISM_ARCH_CLASS})")
  endif()
  if(NOT COMMAND pkg_check_modules)
    set(PB_PKG_CONFIG "")
    set(PB_RUNTIME_PKG_CONFIG "")
  endif()
  # A RUNTIME_PKG_CONFIG module is only compiled against. The backend loads it
  # itself, so a machine without it still loads prism.
  foreach(_mod IN LISTS PB_PKG_CONFIG PB_RUNTIME_PKG_CONFIG)
    string(REGEX REPLACE "[><=].*$" "" _bare "${_mod}")
    string(MAKE_C_IDENTIFIER "${_bare}" _id)
    string(TOUPPER "${_id}" _id)
    if(NOT TARGET PkgConfig::PRISM_PC_${_id})
      pkg_check_modules(PRISM_PC_${_id} QUIET IMPORTED_TARGET "${_mod}")
    endif()
    if(PRISM_PC_${_id}_FOUND AND _mod IN_LIST PB_RUNTIME_PKG_CONFIG)
      list(APPEND _headers PkgConfig::PRISM_PC_${_id})
    elseif(PRISM_PC_${_id}_FOUND)
      list(APPEND _libs PkgConfig::PRISM_PC_${_id})
      set(_pcd "${PRISM_PKGCONFIG_FIND_DEPENDS}")
      list(
        APPEND
        _pcd
        "pkg_check_modules(PRISM_PC_${_id} REQUIRED IMPORTED_TARGET \"${_mod}\")"
      )
      set(PRISM_PKGCONFIG_FIND_DEPENDS
          "${_pcd}"
          CACHE INTERNAL "")
    else()
      list(APPEND _missing "${_mod}")
    endif()
  endforeach()
  if(_missing)
    string(REPLACE ";" ", " _m "${_missing}")
    if(${_opt} STREQUAL "ON")
      message(
        FATAL_ERROR
          "${_opt}=ON but ${NAME} cannot be built: missing ${_m}.\n"
          "Install what is missing, set -D${_opt}=AUTO to skip it, or =OFF to disable it."
      )
    endif()
    message(STATUS "Prism backend ${NAME}: skipped (missing ${_m})")
    list(APPEND PRISM_BACKEND_SUMMARY "${NAME}=skipped")
    set(PRISM_BACKEND_SUMMARY
        "${PRISM_BACKEND_SUMMARY}"
        PARENT_SCOPE)
    return()
  endif()
  set(_tgt prism_backend_${NAME})
  add_library(${_tgt} OBJECT
              "${PRISM_SOURCE_ROOT}/source/backends/${PB_SOURCE}")
  target_link_libraries(${_tgt} PRIVATE prism_common ${_libs})
  foreach(_h IN LISTS _headers)
    target_include_directories(
      ${_tgt} PRIVATE $<TARGET_PROPERTY:${_h},INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_options(
      ${_tgt} PRIVATE $<TARGET_PROPERTY:${_h},INTERFACE_COMPILE_OPTIONS>)
  endforeach()
  target_compile_definitions(${_tgt}
                             PRIVATE PRISM_BACKEND_ANCHOR=prism_anchor_${NAME})
  set_target_properties(
    ${_tgt}
    PROPERTIES POSITION_INDEPENDENT_CODE ON
               CXX_VISIBILITY_PRESET hidden
               C_VISIBILITY_PRESET hidden
               VISIBILITY_INLINES_HIDDEN ON
               INTERPROCEDURAL_OPTIMIZATION ${PRISM_USE_IPO})
  if(PB_DEFINES)
    target_compile_definitions(${_tgt} PRIVATE ${PB_DEFINES})
  endif()
  if(PB_LANGUAGE STREQUAL "OBJCXX")
    set_source_files_properties(
      "${PRISM_SOURCE_ROOT}/source/backends/${PB_SOURCE}" TARGET_DIRECTORY
      ${_tgt} PROPERTIES COMPILE_OPTIONS "-x;objective-c++;-fobjc-arc")
  endif()
  target_sources(prism PRIVATE $<TARGET_OBJECTS:${_tgt}>)
  list(APPEND PRISM_BACKEND_ANCHORS "${NAME}")
  set(PRISM_BACKEND_ANCHORS
      "${PRISM_BACKEND_ANCHORS}"
      PARENT_SCOPE)
  if(_libs)
    target_link_libraries(prism PRIVATE ${_libs})
  endif()
  if(PB_FEATURE)
    target_compile_definitions(prism_common INTERFACE ${PB_FEATURE})
  endif()
  message(STATUS "Prism backend ${NAME}: enabled")
  list(APPEND PRISM_BACKEND_TARGETS ${_tgt})
  list(APPEND PRISM_BACKEND_SUMMARY "${NAME}=on")
  set(PRISM_BACKEND_TARGETS
      "${PRISM_BACKEND_TARGETS}"
      PARENT_SCOPE)
  set(PRISM_BACKEND_SUMMARY
      "${PRISM_BACKEND_SUMMARY}"
      PARENT_SCOPE)
endfunction()

if(UNIX
   AND NOT APPLE
   AND NOT ANDROID
   AND NOT EMSCRIPTEN
   AND NOT PRISM_CONSOLE)
  find_package(PkgConfig REQUIRED)
endif()
prism_declare_backend(
  nvda
  SOURCE
  nvda.cpp
  PLATFORM
  WINDOWS
  DOC
  "NVDA screen reader")
prism_declare_backend(
  jaws
  SOURCE
  jaws.cpp
  PLATFORM
  WINDOWS
  DOC
  "JAWS screen reader")
prism_declare_backend(
  sapi
  SOURCE
  sapi.cpp
  PLATFORM
  WINDOWS
  DOC
  "Microsoft SAPI 5")
prism_declare_backend(
  onecore
  SOURCE
  onecore.cpp
  PLATFORM
  WINDOWS
  DOC
  "Windows OneCore")
prism_declare_backend(
  uia
  SOURCE
  uia.cpp
  PLATFORM
  WINDOWS
  DOC
  "UI Automation")
prism_declare_backend(
  zoom_text
  SOURCE
  zoom_text.cpp
  PLATFORM
  WINDOWS
  DOC
  "ZoomText")
prism_declare_backend(
  sense_reader
  SOURCE
  sense_reader.cpp
  PLATFORM
  WINDOWS
  DOC
  "SenseReader")
prism_declare_backend(
  pc_talker
  SOURCE
  pc_talker.cpp
  PLATFORM
  WINDOWS
  DOC
  "PC-Talker")
prism_declare_backend(
  zdsr
  SOURCE
  zdsr.cpp
  PLATFORM
  WINDOWS
  ARCH
  x86
  x64
  DOC
  "ZDSR")
prism_declare_backend(
  boy_pc_reader
  SOURCE
  boy_pc_reader.cpp
  PLATFORM
  WINDOWS
  ARCH
  x86
  x64
  DOC
  "BoyPCReader")
prism_declare_backend(
  system_access
  LEGACY
  SOURCE
  system_access.cpp
  PLATFORM
  WINDOWS
  DOC
  "System Access"
  DEFINES
  PRISM_ENABLE_LEGACY_BACKENDS
  PRISM_ENABLE_SYSTEM_ACCESS_LEGACY_BACKEND)
prism_declare_backend(
  window_eyes
  LEGACY
  SOURCE
  window_eyes.cpp
  PLATFORM
  WINDOWS
  ARCH
  x86
  x64
  DOC
  "Window-Eyes"
  DEFINES
  PRISM_ENABLE_LEGACY_BACKENDS
  PRISM_ENABLE_WINDOW_EYES_LEGACY_BACKEND)
prism_declare_backend(
  avspeech
  SOURCE
  avspeech.cpp
  PLATFORM
  APPLE
  LANGUAGE
  OBJCXX
  DOC
  "AVSpeechSynthesizer")
prism_declare_backend(
  voiceover
  SOURCE
  voiceover.cpp
  PLATFORM
  APPLE
  LANGUAGE
  OBJCXX
  DOC
  "VoiceOver")
prism_declare_backend(
  orca
  SOURCE
  orca.cpp
  PLATFORM
  UNIX
  WINDOWS
  DOC
  "Orca screen reader over D-Bus"
  FEATURE
  PRISM_HAVE_ORCA
  PKG_CONFIG
  "dbus-1")
prism_declare_backend(
  speech_dispatcher
  SOURCE
  speech_dispatcher.cpp
  PLATFORM
  UNIX
  WINDOWS
  DOC
  "speech-dispatcher"
  FEATURE
  PRISM_HAVE_SPEECHD
  RUNTIME_PKG_CONFIG
  "speech-dispatcher")
prism_declare_backend(
  spiel
  SOURCE
  spiel.cpp
  PLATFORM
  UNIX
  WINDOWS
  DOC
  "Spiel speech framework"
  FEATURE
  PRISM_HAVE_SPIEL
  PKG_CONFIG
  "gio-2.0"
  RUNTIME_PKG_CONFIG
  "spiel-1.0")
# The GDK speech API is also supported by desktop Windows. Detect a usable
# development SDK without requiring GDK for other Windows backends.
if(WIN32 OR PRISM_XBOX)
  set(_prism_gdk_option "${PRISM_ENABLE_GDK_SPEECH_BACKEND}")
  if(NOT _prism_gdk_option STREQUAL "OFF")
    find_path(
      PRISM_GDK_INCLUDE_DIR XSpeechSynthesizer.h
      HINTS "$ENV{GamingGRDK}" "$ENV{GRDKLatest}" "$ENV{GRDK}" "${GamingGRDK}"
            "${GRDKLatest}" "${GRDK}"
      PATH_SUFFIXES GameKit/Include Include)
    if(PRISM_XBOX)
      find_library(
        PRISM_GDK_RUNTIME_LIBRARY
        NAMES xgameruntime
        HINTS "$ENV{GamingGRDK}" "$ENV{GRDKLatest}" "$ENV{GRDK}" "${GamingGRDK}"
              "${GRDKLatest}" "${GRDK}"
        PATH_SUFFIXES GameKit/Lib/amd64 GameKit/Lib/x64 Lib/amd64 Lib/x64 Lib)
    endif()
    unset(PRISM_GDK_SPEECH_LINKS CACHE)
    if(PRISM_GDK_INCLUDE_DIR AND (NOT PRISM_XBOX OR PRISM_GDK_RUNTIME_LIBRARY))
      include(CheckCXXSourceCompiles)
      set(_prism_saved_required_includes "${CMAKE_REQUIRED_INCLUDES}")
      set(_prism_saved_required_libraries "${CMAKE_REQUIRED_LIBRARIES}")
      set(CMAKE_REQUIRED_INCLUDES "${PRISM_GDK_INCLUDE_DIR}")
      if(PRISM_XBOX)
        set(CMAKE_REQUIRED_LIBRARIES "${PRISM_GDK_RUNTIME_LIBRARY}")
      else()
        set(CMAKE_REQUIRED_LIBRARIES "")
      endif()
      check_cxx_source_compiles(
        "#include <XSpeechSynthesizer.h>
#include <XPackage.h>
#include <XGameRuntimeInit.h>
#include <xaudio2.h>
int main() {
  XSpeechSynthesizerHandle handle{};
  using Synth = decltype(&XSpeechSynthesizerCreate);
  using Runtime = decltype(&XGameRuntimeInitialize);
  static_assert(sizeof(Synth) == sizeof(void (*)()));
  static_assert(sizeof(Runtime) == sizeof(void (*)()));
  return 0;
}"
        PRISM_GDK_SPEECH_LINKS)
      set(CMAKE_REQUIRED_INCLUDES "${_prism_saved_required_includes}")
      set(CMAKE_REQUIRED_LIBRARIES "${_prism_saved_required_libraries}")
    endif()
    if(NOT PRISM_GDK_SPEECH_LINKS)
      if(_prism_gdk_option STREQUAL "ON")
        message(
          FATAL_ERROR
            "GDK speech backend requested but its SDK headers or required console link library are unavailable"
        )
      endif()
      message(
        STATUS
          "Prism: GDK speech backend unavailable (SDK headers or required console library)"
      )
    else()
      if(PRISM_XBOX AND NOT TARGET prism_gdk_runtime)
        add_library(prism_gdk_runtime UNKNOWN IMPORTED)
        set_target_properties(
          prism_gdk_runtime
          PROPERTIES IMPORTED_LOCATION "${PRISM_GDK_RUNTIME_LIBRARY}"
                     INTERFACE_INCLUDE_DIRECTORIES "${PRISM_GDK_INCLUDE_DIR}")
      endif()
    endif()
  endif()
endif()
if(PRISM_GDK_SPEECH_LINKS AND NOT PRISM_ENABLE_GDK_SPEECH_BACKEND STREQUAL
                              "OFF")
  prism_declare_backend(
    gdk_speech
    SOURCE
    gdk_speech.cpp
    PLATFORM
    WINDOWS
    XBOX
    DEFINES
    PRISM_HAVE_GDK_SPEECH=1
    UNICODE
    _UNICODE
    DOC
    "Microsoft GDK XSpeechSynthesizer")
  if(TARGET prism_backend_gdk_speech)
    target_include_directories(prism_backend_gdk_speech
                               PRIVATE "${PRISM_GDK_INCLUDE_DIR}")
    target_link_libraries(prism PRIVATE xmllite ole32)
    if(PRISM_XBOX)
      target_link_libraries(prism_backend_gdk_speech PRIVATE prism_gdk_runtime)
      target_link_libraries(prism PRIVATE prism_gdk_runtime)
    endif()
  endif()
elseif(PRISM_XBOX AND PRISM_ENABLE_GDK_SPEECH_BACKEND STREQUAL "ON")
  message(
    FATAL_ERROR
      "Xbox speech backend explicitly enabled but GDK dependencies were not found"
  )
endif()
prism_declare_backend(
  android_tts
  SOURCE
  android_tts.cpp
  PLATFORM
  ANDROID
  DOC
  "Android TextToSpeech")
prism_declare_backend(
  android_screen_reader
  SOURCE
  android_screen_reader.cpp
  PLATFORM
  ANDROID
  DOC
  "Android screen readers")
prism_declare_backend(
  web_speech_synthesis_backend
  SOURCE
  web_speech_synthesis_backend.cpp
  PLATFORM
  EMSCRIPTEN
  DOC
  "Web Speech API")
if(NOT PRISM_BACKEND_TARGETS)
  if(PRISM_CONSOLE)
    message(
      STATUS
        "Prism: no built-in console backend selected; custom/static backend registration remains available"
    )
  else()
    message(
      FATAL_ERROR
        "No backends will be built. Prism would compile but do nothing at runtime."
    )
  endif()
endif()
list(JOIN PRISM_BACKEND_SUMMARY " " _s)
message(STATUS "Prism backends: ${_s}")
# A static link only pulls an archive member in when something references it,
# and nothing references a backend's object, so prism.cpp does. MSVC is told per
# object with /include:; elsewhere prism.cpp takes each anchor's address.
set(_anchors "")
if(MSVC)
  foreach(_name IN LISTS PRISM_BACKEND_ANCHORS)
    if(PRISM_ARCH_CLASS STREQUAL "x86")
      # cdecl decorates with a leading underscore on x86 only.
      set(_sym "_prism_anchor_${_name}")
    else()
      set(_sym "prism_anchor_${_name}")
    endif()
    string(APPEND _anchors "#pragma comment(linker, \"/include:${_sym}\")
")
  endforeach()
else()
  set(_refs "")
  foreach(_name IN LISTS PRISM_BACKEND_ANCHORS)
    string(APPEND _anchors "extern \"C\" void prism_anchor_${_name}();
")
    string(APPEND _refs "    prism_anchor_${_name},
")
  endforeach()
  string(
    APPEND
    _anchors
    "// NOLINTNEXTLINE(modernize-avoid-c-arrays)
[[gnu::used]] static void (*const prism_backend_anchors[])() = {
${_refs}};
")
endif()
# PrismCodegen, which owns PRISM_GEN_DIR, is included after this file.
set(_gen "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${_gen}")
# A header so the references land in prism.cpp. The linker reads them only from
# objects it already links, so a source file holding nothing else would be
# dropped before they were read.
configure_file("${PRISM_SOURCE_ROOT}/cmake/backend_anchors.h.in"
               "${_gen}/backend_anchors.h" @ONLY)
target_include_directories(prism PRIVATE "${_gen}")
target_compile_definitions(prism PRIVATE PRISM_HAVE_BACKEND_ANCHORS)
