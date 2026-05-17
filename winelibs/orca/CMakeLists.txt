include(CheckCCompilerFlag)
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
check_c_compiler_flag(-Wall COMP_HAS_WALL_OPTION)
check_c_compiler_flag(-Wextra COMP_HAS_WEXTRA_OPTION)
if(COMP_HAS_WALL_OPTION AND COMP_HAS_WEXTRA_OPTION)
  target_compile_options(prism_orca_bridge PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wall
                                                   -Wextra>)
endif()
if(PRISM_ENABLE_LINTING)
  find_program(CLANG_TIDY_EXE NAMES "clang-tidy-22" "clang-tidy-21"
                                    "clang-tidy-20" "clang-tidy")
  if(CLANG_TIDY_EXE)
    set(CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}")
    set_target_properties(prism_orca_bridge PROPERTIES CXX_CLANG_TIDY
                                                       "${CLANG_TIDY_COMMAND}")
  endif()
endif()
