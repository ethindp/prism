# SPDX-License-Identifier: MPL-2.0

cmake_minimum_required(VERSION 3.20)

include(CheckCCompilerFlag)
include(CheckIPOSupported)
include(CheckCSourceCompiles)
include(CheckSymbolExists)
include(CheckTypeSize)
include(CheckIncludeFile)
include(GNUInstallDirs)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

project(
  tolk
  VERSION 0.11.3
  DESCRIPTION "Tolk shim for Prism"
  LANGUAGES C)

check_include_file(stdatomic.h HAS_STD_ATOMICS)
if(NOT HAS_STD_ATOMICS)
  if(MSVC)
    check_c_compiler_flag(/experimental:c11atomics NEEDS_C11_ATOMICS_FLAG)
    if(NEEDS_C11_ATOMICS_FLAG)
      unset(HAS_STD_ATOMICS CACHE)
      check_include_file(stdatomic.h HAS_STD_ATOMICS /experimental:c11atomics)
    endif()
  endif()
endif()
if(NOT HAS_STD_ATOMICS)
  message(FATAL_ERROR "This compiler does not support the C11 atomics library.")
endif()
set(CMAKE_EXTRA_INCLUDE_FILES "stdatomic.h")
if(NEEDS_C11_ATOMICS_FLAG)
  set(CMAKE_REQUIRED_FLAGS /experimental:c11atomics)
endif()
check_type_size(atomic_bool ATOMIC_BOOL_SIZE RESULT_VARIABLE HAVE_ATOMIC_BOOL)
check_type_size(atomic_int ATOMIC_INT_SIZE RESULT_VARIABLE HAVE_ATOMIC_INT)
if(NOT HAVE_ATOMIC_BOOL OR NOT HAVE_ATOMIC_INT)
  message(
    FATAL_ERROR
      "Atomic bool and int types must exist on this implementation, but they do not, or could not be found"
  )
endif()
if(NOT ATOMIC_INT_SIZE EQUAL 4 AND LINUX)
  message(FATAL_ERROR "atomic_int size != 4!")
endif()
check_symbol_exists(ATOMIC_BOOL_LOCK_FREE "stdatomic.h"
                    HAS_ATOMIC_BOOL_LOCK_FREE)
check_symbol_exists(ATOMIC_CHAR_LOCK_FREE "stdatomic.h"
                    HAS_ATOMIC_CHAR_LOCK_FREE)
check_symbol_exists(ATOMIC_CHAR16_T_LOCK_FREE "stdatomic.h"
                    HAS_ATOMIC_CHAR16_T_LOCK_FREE)
check_symbol_exists(ATOMIC_CHAR32_T_LOCK_FREE "stdatomic.h"
                    HAS_ATOMIC_CHAR32_T_LOCK_FREE)
check_symbol_exists(ATOMIC_WCHAR_T_LOCK_FREE "stdatomic.h"
                    HAS_ATOMIC_WCHAR_T_LOCK_FREE)
check_symbol_exists(ATOMIC_SHORT_LOCK_FREE "stdatomic.h"
                    HAS_ATOMIC_SHORT_LOCK_FREE)
check_symbol_exists(ATOMIC_INT_LOCK_FREE "stdatomic.h" HAS_ATOMIC_INT_LOCK_FREE)
check_symbol_exists(ATOMIC_LONG_LOCK_FREE "stdatomic.h"
                    HAS_ATOMIC_LONG_LOCK_FREE)
check_symbol_exists(ATOMIC_LLONG_LOCK_FREE "stdatomic.h"
                    HAS_ATOMIC_LLONG_LOCK_FREE)
check_symbol_exists(ATOMIC_POINTER_LOCK_FREE "stdatomic.h"
                    HAS_ATOMIC_POINTER_LOCK_FREE)
check_symbol_exists(ATOMIC_VAR_INIT "stdatomic.h" HAS_ATOMIC_VAR_INIT)
check_symbol_exists(ATOMIC_FLAG_INIT "stdatomic.h" HAS_ATOMIC_FLAG_INIT)
check_symbol_exists(kill_dependency "stdatomic.h" HAS_KILL_DEPENDENCY)
check_symbol_exists(atomic_is_lock_free "stdatomic.h" HAS_ATOMIC_IS_LOCK_FREE)
check_symbol_exists(atomic_init "stdatomic.h" HAS_ATOMIC_INIT)
check_symbol_exists(atomic_store "stdatomic.h" HAS_ATOMIC_STORE)
check_symbol_exists(atomic_store_explicit "stdatomic.h"
                    HAS_ATOMIC_STORE_EXPLICIT)
check_symbol_exists(atomic_load "stdatomic.h" HAS_ATOMIC_LOAD)
check_symbol_exists(atomic_load_explicit "stdatomic.h" HAS_ATOMIC_LOAD_EXPLICIT)
check_symbol_exists(atomic_exchange "stdatomic.h" HAS_ATOMIC_EXCHANGE)
check_symbol_exists(atomic_exchange_explicit "stdatomic.h"
                    HAS_ATOMIC_EXCHANGE_EXPLICIT)
check_symbol_exists(atomic_compare_exchange_strong "stdatomic.h"
                    HAS_ATOMIC_COMPARE_EXCHANGE_STRONG)
check_symbol_exists(atomic_compare_exchange_strong_explicit "stdatomic.h"
                    HAS_ATOMIC_COMPARE_EXCHANGE_STRONG_EXPLICIT)
check_symbol_exists(atomic_compare_exchange_weak "stdatomic.h"
                    HAS_ATOMIC_COMPARE_EXCHANGE_WEAK)
check_symbol_exists(atomic_compare_exchange_weak_explicit "stdatomic.h"
                    HAS_ATOMIC_COMPARE_EXCHANGE_WEAK_EXPLICIT)
check_symbol_exists(atomic_fetch_add "stdatomic.h" HAS_ATOMIC_FETCH_ADD)
check_symbol_exists(atomic_fetch_add_explicit "stdatomic.h"
                    HAS_ATOMIC_FETCH_ADD_EXPLICIT)
check_symbol_exists(atomic_fetch_sub "stdatomic.h" HAS_ATOMIC_FETCH_SUB)
check_symbol_exists(atomic_fetch_sub_explicit "stdatomic.h"
                    HAS_ATOMIC_FETCH_SUB_EXPLICIT)
check_symbol_exists(atomic_fetch_or "stdatomic.h" HAS_ATOMIC_FETCH_OR)
check_symbol_exists(atomic_fetch_or_explicit "stdatomic.h"
                    HAS_ATOMIC_FETCH_OR_EXPLICIT)
check_symbol_exists(atomic_fetch_xor "stdatomic.h" HAS_ATOMIC_FETCH_XOR)
check_symbol_exists(atomic_fetch_xor_explicit "stdatomic.h"
                    HAS_ATOMIC_FETCH_XOR_EXPLICIT)
check_symbol_exists(atomic_fetch_and "stdatomic.h" HAS_ATOMIC_FETCH_AND)
check_symbol_exists(atomic_fetch_and_explicit "stdatomic.h"
                    HAS_ATOMIC_FETCH_AND_EXPLICIT)
check_symbol_exists(atomic_flag_test_and_set "stdatomic.h"
                    HAS_ATOMIC_FLAG_TEST_AND_SET)
check_symbol_exists(atomic_flag_test_and_set_explicit "stdatomic.h"
                    HAS_ATOMIC_FLAG_TEST_AND_SET_EXPLICIT)
check_symbol_exists(atomic_flag_clear "stdatomic.h" HAS_ATOMIC_FLAG_CLEAR)
check_symbol_exists(atomic_flag_clear_explicit "stdatomic.h"
                    HAS_ATOMIC_FLAG_CLEAR_EXPLICIT)
check_symbol_exists(atomic_thread_fence "stdatomic.h" HAS_ATOMIC_THREAD_FENCE)
check_symbol_exists(atomic_signal_fence "stdatomic.h" HAS_ATOMIC_SIGNAL_FENCE)
if(NOT HAS_ATOMIC_COMPARE_EXCHANGE_STRONG_EXPLICIT
   OR NOT HAS_ATOMIC_FETCH_SUB_EXPLICIT
   OR NOT HAS_ATOMIC_STORE_EXPLICIT
   OR NOT HAS_ATOMIC_LOAD
   OR NOT HAS_ATOMIC_STORE)
  message(
    FATAL_ERROR
      "This compiler is missing one or more atomic functions needed by this shim in stdatomic.h. Please update your implementation."
  )
endif()
if(WIN32 OR MINGW)
  set(CMAKE_REQUIRED_LIBRARIES synchronization)
  check_symbol_exists(WaitOnAddress "windows.h" HAS_WAIT_ON_ADDRESS)
  check_symbol_exists(WakeByAddressSingle "windows.h"
                      HAS_WAKE_BY_ADDRESS_SINGLE)
  if(NOT HAS_WAIT_ON_ADDRESS OR NOT HAS_WAKE_BY_ADDRESS_SINGLE)
    message(
      FATAL_ERROR
        "WaitOnAddress or WakeByAddressSingle are not defined by this implementation"
    )
  endif()
  unset(CMAKE_REQUIRED_LIBRARIES)
elseif(APPLE)
  check_symbol_exists(os_unfair_lock_lock "os/lock.h" HAS_OS_UNFAIR_LOCK_LOCK)
  check_symbol_exists(os_unfair_lock_unlock "os/lock.h"
                      HAS_OS_UNFAIR_LOCK_UNLOCK)
  if(NOT HAS_OS_UNFAIR_LOCK_LOCK OR NOT HAS_OS_UNFAIR_LOCK_UNLOCK)
    message(FATAL_ERROR "os_unfair_lock is not implemented by this target?")
  endif()
elseif(LINUX)
  check_c_source_compiles(
    "
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

int main(void) {
    int val = 0;
    syscall(SYS_futex, &val, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, 0, 0, 0, 0);
    syscall(SYS_futex, &val, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1, 0, 0, 0);
    return 0;
}
"
    HAS_FUTEX)
  if(NOT HAS_FUTEX)
    message(
      FATAL_ERROR
        "Futexes are not supported by the Linux system you are targeting.")
  endif()
else()
  find_package(Threads REQUIRED)
endif()
add_library(tolk SHARED tolk.c lock.c unicode.c)
target_compile_definitions(tolk PRIVATE TOLK_BUILDING)
if(NEEDS_C11_ATOMICS_FLAG)
  target_compile_options(tolk PRIVATE /experimental:c11atomics)
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
if(NOT MSVC)
  set_target_properties(tolk PROPERTIES C_VISIBILITY_PRESET hidden
                                        VISIBILITY_INLINES_HIDDEN ON)
endif()
if(PRISM_ENABLE_LINTING)
  if(NOT CMAKE_C_COMPILER_ID MATCHES "MSVC")
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
    if(CLANG_TIDY_EXE)
      set_target_properties(tolk PROPERTIES C_CLANG_TIDY "${CLANG_TIDY_EXE}")
    endif()
  else()
    set(_MERGED_RULESET_PATH "${CMAKE_BINARY_DIR}/PrismCombinedRuleset.ruleset")
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
      PRIVATE
        /external:anglebrackets
        /external:W0
        "/external:I$<TARGET_PROPERTY:prism,INTERFACE_INCLUDE_DIRECTORIES>"
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
target_link_libraries(tolk PRIVATE prism)
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
if(ipo_supported)
  set_target_properties(tolk PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
endif()
target_include_directories(
  tolk PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
              $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
install(
  TARGETS tolk
  EXPORT tolkTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES tolk.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
