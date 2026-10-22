if(NOT DBUS_FOUND)
  message(STATUS "winelibs/orca: skipped (needs dbus-1)")
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
  PkgConfig::DBUS)
