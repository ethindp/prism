if(NOT DBUS_FOUND)
  message(STATUS "winelibs/orca: skipped (needs dbus-1)")
  return()
endif()
prism_add_winelib(
  orca
  SOURCES
  bridge.cpp
  ../../source/dbus/client.cpp
  ../../source/dbus/bus.cpp
  ../../source/dbus/orca.cpp
  SPEC
  ${CMAKE_CURRENT_SOURCE_DIR}/exports.spec
  DEPS
  PkgConfig::DBUS)
