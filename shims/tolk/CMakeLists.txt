# SPDX-License-Identifier: MPL-2.0

include(CheckCCompilerFlag)
include(CheckIPOSupported)
include(CheckCSourceCompiles)
include(CheckSymbolExists)
include(CheckTypeSize)
include(CheckIncludeFile)
include(GNUInstallDirs)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT
   (WIN32
    OR APPLE
    OR LINUX
    OR ANDROID))
  find_package(Threads REQUIRED)
endif()
add_library(tolk SHARED tolk.c lock.c)
target_link_libraries(tolk PRIVATE prism::prism)
target_compile_definitions(tolk PRIVATE TOLK_BUILDING)
if(PRISM_HAS_NO_UNDEFINED_LINK_FLAG)
  target_link_options(tolk PRIVATE "LINKER:--no-undefined")
endif()
if(NOT MSVC)
  check_cxx_compiler_flag(-Wall COMP_HAS_WALL_OPTION)
  check_cxx_compiler_flag(-Wextra COMP_HAS_WEXTRA_OPTION)
  check_cxx_compiler_flag(-Wthread-safety COMP_HAS_WTHREAD_SAFETY_OPTION)
  if(COMP_HAS_WALL_OPTION AND COMP_HAS_WEXTRA_OPTION)
    target_compile_options(tolk PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall
                                        -Wextra>)
  endif()
  if(COMP_HAS_WTHREAD_SAFETY_OPTION)
    target_compile_options(
      tolk PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-Wthread-safety>
                   $<$<COMPILE_LANGUAGE:C,CXX>:-Werror=thread-safety>)
  endif()
endif()
if(PRISM_ENABLE_LINTING)
  if(NOT CMAKE_C_COMPILER_ID MATCHES "MSVC")
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy-22" "clang-tidy-21"
                                      "clang-tidy-20" "clang-tidy")
    if(CLANG_TIDY_EXE)
      set_target_properties(tolk PROPERTIES C_CLANG_TIDY "${CLANG_TIDY_EXE}")
    endif()
  else()
    set(_MERGED_RULESET_PATH "${PRISM_MERGED_RULESET}")
    if(NOT EXISTS "${_MERGED_RULESET_PATH}")
      message(
        FATAL_ERROR
          "Prism combined ruleset not found at: ${_MERGED_RULESET_PATH}. Ensure the parent prism project is configured first."
      )
    endif()
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/nativecodeanalysis/tolk")
    set_property(TARGET tolk PROPERTY COMPILE_WARNING_AS_ERROR ON)
    set_property(TARGET tolk PROPERTY COMPILE_WARNING_LEVEL 4)
    target_compile_options(
      tolk
      PRIVATE /external:anglebrackets
              /external:W0
              /analyze
              /analyze:external-
              /analyze:log
              "${CMAKE_BINARY_DIR}/nativecodeanalysis/tolk/"
              /analyze:log:format:sarif
              /analyze:ruleset
              "${_MERGED_RULESET_PATH}")
  endif()
endif()
set_target_properties(tolk PROPERTIES VERSION ${PROJECT_VERSION}
                                      SOVERSION ${PROJECT_VERSION_MAJOR})
if(WIN32 OR MINGW)
  target_link_libraries(tolk PRIVATE synchronization)
elseif(
  NOT WIN32
  AND NOT APPLE
  AND NOT LINUX
  AND NOT ANDROID)
  target_link_libraries(tolk PRIVATE Threads::Threads)
endif()
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if(ipo_supported AND PRISM_USE_IPO)
  set_target_properties(tolk PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
  if(PRISM_IPO_FAT_OBJECTS)
    target_compile_options(
      tolk PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-ffat-lto-objects>)
  endif()
endif()
target_include_directories(
  tolk PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
              $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
set_target_properties(
  tolk
  PROPERTIES CXX_VISIBILITY_PRESET hidden
             C_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN ON)
if(EMSCRIPTEN)
  target_compile_options(tolk PRIVATE -pthread)
  target_link_options(tolk PRIVATE -pthread)
endif()
install(
  TARGETS tolk
  EXPORT tolkTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(MSVC)
  install(
    FILES $<TARGET_PDB_FILE:tolk>
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    OPTIONAL)
endif()
install(FILES tolk.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
