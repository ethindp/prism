# SPDX-License-Identifier: MPL-2.0

include_guard(GLOBAL)
include(PrismGuards)
include(CMakePackageConfigHelpers)
prism_require_targets(prism prism_iface)
prism_require_vars(PRISM_LIB_TYPE)
if(DEFINED SKBUILD)
  set(_native "${SKBUILD_PLATLIB_DIR}/prism/_native")
  find_package(Python REQUIRED COMPONENTS Interpreter Development.Module
                                          ${SKBUILD_SABI_COMPONENT})
  set(_cffi_c "${CMAKE_CURRENT_BINARY_DIR}/_prism_cffi.c")
  add_custom_command(
    OUTPUT "${_cffi_c}"
    COMMAND ${Python_EXECUTABLE} "${PRISM_SOURCE_ROOT}/tools/generate_cffi.py"
            "${PRISM_SOURCE_ROOT}/include/prism.h" "${_cffi_c}"
    DEPENDS "${PRISM_SOURCE_ROOT}/include/prism.h"
            "${PRISM_SOURCE_ROOT}/tools/generate_cffi.py"
    VERBATIM)
  set(_sabi "")
  if(NOT "${SKBUILD_SABI_VERSION}" STREQUAL "")
    set(_sabi USE_SABI ${SKBUILD_SABI_VERSION})
  endif()
  python_add_library(_prism_cffi MODULE ${_sabi} WITH_SOABI "${_cffi_c}")
  # MSVC does not implement C23 nullptr, which cffi's generated source uses.
  if(MSVC)
    set_target_properties(_prism_cffi PROPERTIES C_STANDARD 17
                                                 C_STANDARD_REQUIRED ON)
    target_compile_options(_prism_cffi PRIVATE /wd4113)
  endif()
  set_property(TARGET _prism_cffi PROPERTY SKIP_LINTING ON)
  target_include_directories(_prism_cffi PRIVATE "${PRISM_SOURCE_ROOT}/include")
  target_link_libraries(_prism_cffi PRIVATE prism)
  if(APPLE)
    set_target_properties(_prism_cffi PROPERTIES INSTALL_RPATH "@loader_path")
  else()
    set_target_properties(_prism_cffi PROPERTIES INSTALL_RPATH "$ORIGIN")
  endif()
  install(
    TARGETS _prism_cffi prism
    LIBRARY DESTINATION "${_native}"
    RUNTIME DESTINATION "${_native}"
    ARCHIVE DESTINATION "${_native}")
  return()
endif()
install(
  TARGETS prism prism_iface
  EXPORT prismTargets
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)
if(MSVC AND BUILD_SHARED_LIBS)
  install(
    FILES $<TARGET_PDB_FILE:prism>
    DESTINATION bin
    OPTIONAL)
endif()
install(
  EXPORT prismTargets
  FILE prism-targets.cmake
  NAMESPACE prism::
  DESTINATION share/prism)
install(DIRECTORY include/ DESTINATION include)
set(PRISM_CONFIG_FIND_DEPS "")
if(PRISM_LIB_TYPE STREQUAL "STATIC_LIBRARY")
  foreach(_d IN ITEMS FMT SIMDUTF CONCURRENTQUEUE)
    if(DEFINED PRISM_${_d}_PROVIDER AND PRISM_${_d}_PROVIDER STREQUAL "SYSTEM")
      string(TOLOWER "${_d}" _l)
      if(_d STREQUAL "CONCURRENTQUEUE")
        set(_l "unofficial-concurrentqueue")
      endif()
      string(APPEND PRISM_CONFIG_FIND_DEPS "find_dependency(${_l} CONFIG)\n")
    endif()
  endforeach()
endif()
if(WIN32 AND PRISM_LIB_TYPE STREQUAL "STATIC_LIBRARY")
  foreach(_imp IN LISTS PRISM_WIN_IMPORT_LIBS)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${_imp}.lib" DESTINATION lib)
  endforeach()
  set(PRISM_WIN_STATIC_IMPORT_LIBS "${PRISM_WIN_IMPORT_LIBS}")
else()
  set(PRISM_WIN_STATIC_IMPORT_LIBS "")
endif()
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/prism-config-version.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)
configure_package_config_file(
  "${PRISM_SOURCE_ROOT}/cmake/prism-config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/prism-config.cmake"
  INSTALL_DESTINATION share/prism)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/prism-config.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/prism-config-version.cmake"
        DESTINATION share/prism)
