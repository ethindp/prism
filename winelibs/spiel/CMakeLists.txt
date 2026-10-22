if(NOT SPIEL_FOUND)
  message(STATUS "winelibs/spiel: skipped (needs spiel-1.0)")
  return()
endif()
prism_add_winelib(
  spiel
  SOURCES
  bridge.cpp
  ../../source/dbus/client.cpp
  ../../source/dbus/bus.cpp
  ../../source/dbus/spiel.cpp
  SPEC
  ${CMAKE_CURRENT_SOURCE_DIR}/exports.spec
  DEPS
  PkgConfig::SPIEL
  PkgConfig::DBUS)
