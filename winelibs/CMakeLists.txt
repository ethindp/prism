cmake_minimum_required(VERSION 3.20)
option(
  PRISM_ENABLE_LINTING
  "Enable execution of linting this build with whatever linting tools are available (implicitly enables warnings as errors)"
  OFF)
project(prism_winelibs LANGUAGES C CXX)
include(GNUInstallDirs)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(PRISM_WINELIB_SHARED_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../source/backends/raw")
include(PrismWinelib)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GIO IMPORTED_TARGET gio-2.0)
pkg_check_modules(SPEECHD IMPORTED_TARGET speech-dispatcher)
pkg_check_modules(SPIEL IMPORTED_TARGET spiel-1.0)
if(GIO_FOUND)
  add_subdirectory(orca)
endif()
if(SPEECHD_FOUND)
  add_subdirectory(speechd)
endif()
if(SPIEL_FOUND)
  add_subdirectory(spiel)
endif()
