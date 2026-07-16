# SPDX-License-Identifier: MPL-2.0

include_guard(GLOBAL)
include(PrismGuards)
prism_require_vars(PRISM_SOURCE_ROOT PRISM_ARCH_CLASS PRISM_BACKEND_DEFAULT
                   PRISM_USE_IPO)
prism_require_targets(prism prism_common)
set(PRISM_BACKEND_TARGETS "")
set(PRISM_BACKEND_SUMMARY "")

function(prism_declare_backend NAME)
  cmake_parse_arguments(PB "LEGACY" "SOURCE;DOC;DEFAULT;LANGUAGE;FEATURE"
                        "PLATFORM;ARCH;PKG_CONFIG;DEFINES" ${ARGN})
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
  if(PB_PLATFORM)
    set(_this "")
    if(WIN32)
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
  foreach(_mod IN LISTS PB_PKG_CONFIG)
    string(REGEX REPLACE "[><=].*$" "" _bare "${_mod}")
    string(MAKE_C_IDENTIFIER "${_bare}" _id)
    string(TOUPPER "${_id}" _id)
    if(NOT DEFINED PRISM_PC_${_id}_FOUND)
      pkg_check_modules(PRISM_PC_${_id} QUIET IMPORTED_TARGET "${_mod}")
    endif()
    if(PRISM_PC_${_id}_FOUND)
      list(APPEND _libs PkgConfig::PRISM_PC_${_id})
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
   AND NOT EMSCRIPTEN)
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
  DOC
  "Orca screen reader over D-Bus"
  FEATURE
  PRISM_HAVE_ORCA
  PKG_CONFIG
  "glibmm-2.68>=2.68.0"
  "giomm-2.68>=2.68.0")
prism_declare_backend(
  speech_dispatcher
  SOURCE
  speech_dispatcher.cpp
  PLATFORM
  UNIX
  DOC
  "speech-dispatcher"
  FEATURE
  PRISM_HAVE_SPEECHD
  PKG_CONFIG
  "speech-dispatcher")
prism_declare_backend(
  spiel
  SOURCE
  spiel.cpp
  PLATFORM
  UNIX
  DOC
  "Spiel speech framework"
  FEATURE
  PRISM_HAVE_SPIEL
  PKG_CONFIG
  "spiel-1.0")
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
  message(
    FATAL_ERROR
      "No backends will be built. Prism would compile but do nothing at runtime."
  )
endif()
list(JOIN PRISM_BACKEND_SUMMARY " " _s)
message(STATUS "Prism backends: ${_s}")
