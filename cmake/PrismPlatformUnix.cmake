# SPDX-License-Identifier: MPL-2.0

include_guard(GLOBAL)
include(PrismGuards)
prism_require_targets(prism prism_common)
if(PRISM_ENABLE_POWER_MANAGEMENT)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(PRISM_PM QUIET IMPORTED_TARGET "giomm-2.68>=2.68.0")
  if(PRISM_PM_FOUND)
    target_compile_definitions(prism_common
                               INTERFACE PRISM_ENABLE_POWER_MANAGEMENT)
    target_link_libraries(prism PRIVATE PkgConfig::PRISM_PM)
  else()
    message(
      STATUS "Prism: giomm not found; power-aware availability polling disabled"
    )
  endif()
endif()
if(PRISM_BUILD_WINELIBS)
  include(ExternalProject)
  ExternalProject_Add(
    prism_winelibs
    SOURCE_DIR "${PRISM_SOURCE_ROOT}/winelibs"
    CMAKE_ARGS
      -DCMAKE_TOOLCHAIN_FILE=${PRISM_SOURCE_ROOT}/cmake/winelib-toolchain.cmake
      -DCMAKE_BUILD_TYPE=$<CONFIG> -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
      -DPRISM_ENABLE_LINTING=${PRISM_ENABLE_LINTING}
    INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/winelibs_install"
    BUILD_ALWAYS OFF)
  install(
    DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/winelibs_install/"
    DESTINATION .
    USE_SOURCE_PERMISSIONS)
endif()
