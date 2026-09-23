if(NOT GIO_FOUND)
  message(STATUS "winelibs/orca: skipped (need gio-2.0 and gdbus-codegen)")
  return()
endif()
prism_add_winelib(
  orca
  SOURCES
  bridge.c
  SPEC
  ${CMAKE_CURRENT_SOURCE_DIR}/exports.spec
  INCLUDES
  ${CMAKE_CURRENT_BINARY_DIR}/gen
  DEPS
  PkgConfig::GIO)
