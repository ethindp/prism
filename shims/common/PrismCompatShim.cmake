# SPDX-License-Identifier: MPL-2.0
include(GNUInstallDirs)

function(prism_add_compat_shim target output_name public_header)
  add_library(
    ${target} SHARED ${ARGN} "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/shim_common.c"
                     "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/lock.c")
  target_link_libraries(${target} PRIVATE prism::prism)
  target_include_directories(
    ${target}
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
           $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
  if(PRISM_HAS_NO_UNDEFINED_LINK_FLAG)
    target_link_options(${target} PRIVATE "LINKER:--no-undefined")
  endif()
  if(MINGW)
    target_link_libraries(${target} PRIVATE mincore)
  elseif(WIN32)
    target_link_libraries(${target} PRIVATE synchronization)
  elseif(
    NOT APPLE
    AND NOT LINUX
    AND NOT ANDROID)
    find_package(Threads REQUIRED)
    target_link_libraries(${target} PRIVATE Threads::Threads)
  endif()
  if(EMSCRIPTEN)
    target_compile_options(${target} PRIVATE -pthread)
    target_link_options(${target} PRIVATE -pthread)
  endif()
  if(NOT MSVC)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  if(PRISM_ENABLE_LINTING)
    prism_enable_target_linting(${target})
  endif()
  set_target_properties(
    ${target}
    PROPERTIES OUTPUT_NAME "${output_name}"
               VERSION ${PROJECT_VERSION}
               SOVERSION ${PROJECT_VERSION_MAJOR}
               C_VISIBILITY_PRESET hidden
               CXX_VISIBILITY_PRESET hidden
               VISIBILITY_INLINES_HIDDEN ON)
  install(
    TARGETS ${target}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  if(MSVC)
    install(
      FILES $<TARGET_PDB_FILE:${target}>
      DESTINATION ${CMAKE_INSTALL_BINDIR}
      OPTIONAL)
  endif()
  install(FILES "${public_header}" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endfunction()
