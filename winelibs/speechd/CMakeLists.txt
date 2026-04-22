if(NOT SPEECHD_FOUND)
  message(STATUS "winelibs/speechd: skipped (needs libspeechd)")
  return()
endif()
prism_add_winelib(
  speech_dispatcher
  SOURCES
  bridge.cpp
  SPEC
  ${CMAKE_CURRENT_SOURCE_DIR}/exports.spec
  DEPS
  PkgConfig::SPEECHD)
