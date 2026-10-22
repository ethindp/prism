# SPDX-License-Identifier: MPL-2.0

include_guard(GLOBAL)
include(PrismGuards)
prism_require_targets(prism prism_common)
if(PRISM_ENABLE_POWER_MANAGEMENT)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(PRISM_PM QUIET IMPORTED_TARGET "dbus-1")
  if(PRISM_PM_FOUND)
    target_compile_definitions(prism_common
                               INTERFACE PRISM_ENABLE_POWER_MANAGEMENT)
    target_link_libraries(prism PRIVATE PkgConfig::PRISM_PM)
    set(_pcd "${PRISM_PKGCONFIG_FIND_DEPENDS}")
    list(APPEND _pcd
         "pkg_check_modules(PRISM_PM REQUIRED IMPORTED_TARGET \"dbus-1\")")
    set(PRISM_PKGCONFIG_FIND_DEPENDS
        "${_pcd}"
        CACHE INTERNAL "")
  else()
    message(
      STATUS
        "Prism: dbus-1 not found; power-aware availability polling disabled")
  endif()
endif()
# Orca also requires the compiled D-Bus bindings when power management is off.
if(TARGET prism_backend_orca AND NOT PRISM_PM_FOUND)
  pkg_check_modules(PRISM_ORCA_DBUS REQUIRED IMPORTED_TARGET "dbus-1")
  target_link_libraries(prism PRIVATE PkgConfig::PRISM_ORCA_DBUS)
endif()
if(PRISM_PM_FOUND OR TARGET prism_backend_orca)
  target_sources(
    prism
    PRIVATE ${PRISM_SOURCE_ROOT}/source/dbus/client.cpp
            ${PRISM_SOURCE_ROOT}/source/dbus/bus.cpp
            ${PRISM_SOURCE_ROOT}/source/dbus/orca.cpp
            ${PRISM_SOURCE_ROOT}/source/dbus/login1.cpp
            ${PRISM_SOURCE_ROOT}/source/dbus/spiel.cpp)
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
