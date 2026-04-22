if(NOT GIO_FOUND OR NOT GDBUS_CODEGEN)
  message(STATUS "winelibs/orca: skipped (need gio-2.0 and gdbus-codegen)")
  return()
endif()
prism_gdbus_codegen(orca-service OrcaService ORCA_SERVICE_SRC)
prism_gdbus_codegen(orca-module OrcaModule ORCA_MODULE_SRC)
prism_add_winelib(
  orca
  SOURCES
  bridge.cpp
  ${ORCA_SERVICE_SRC}
  ${ORCA_MODULE_SRC}
  SPEC
  ${CMAKE_CURRENT_SOURCE_DIR}/exports.spec
  INCLUDES
  ${CMAKE_CURRENT_BINARY_DIR}/gen
  DEPS
  PkgConfig::GIO)
