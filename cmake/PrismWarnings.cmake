# SPDX-License-Identifier: MPL-2.0
include_guard(GLOBAL)
include(PrismGuards)
include(CheckCXXCompilerFlag)
prism_require_targets(prism_common)
if(MSVC)
  check_cxx_compiler_flag(/utf-8 PRISM_MSVC_UTF8)
  check_cxx_compiler_flag(/bigobj PRISM_MSVC_BIGOBJ)
  if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    check_cxx_compiler_flag(/Zc:enumTypes PRISM_MSVC_ZC_ENUMTYPES)
  endif()
  if(PRISM_MSVC_UTF8)
    target_compile_options(prism_common
                           INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:/utf-8>)
  endif()
  if(PRISM_MSVC_BIGOBJ)
    target_compile_options(prism_common
                           INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:/bigobj>)
  endif()
  if(PRISM_MSVC_ZC_ENUMTYPES)
    target_compile_options(prism_common
                           INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:/Zc:enumTypes>)
  endif()
  # Angle-bracket and SYSTEM includes (Windows SDK, CRT, STL, third_party) are
  # external: no compiler warnings and no analysis, so what is left is Prism's.
  # Linting reuses these and adds warnings-as-errors and SARIF logs.
  set(PRISM_MSVC_EXTERNAL_OPTIONS
      /external:anglebrackets /external:W0
      "/external:I ${PRISM_SOURCE_ROOT}/source/backends/raw")
  set(PRISM_MSVC_ANALYSIS_OPTIONS
      /analyze
      /analyze:external-
      /analyze:plugin
      EspXEngine.dll
      /analyze:ruleset
      "${CMAKE_CURRENT_LIST_DIR}/PrismAnalysis.ruleset")
  if(PRISM_MSVC_ANALYSIS
     AND NOT PRISM_ENABLE_LINTING
     AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(
      prism_common
      INTERFACE
        "$<$<COMPILE_LANGUAGE:C,CXX>:/W4;${PRISM_MSVC_EXTERNAL_OPTIONS};${PRISM_MSVC_ANALYSIS_OPTIONS};/analyze:autolog->"
    )
  endif()
else()
  check_cxx_compiler_flag(-Wall PRISM_HAS_WALL)
  check_cxx_compiler_flag(-Wextra PRISM_HAS_WEXTRA)
  check_cxx_compiler_flag(-Wthread-safety PRISM_HAS_WTHREAD_SAFETY)
  if(PRISM_HAS_WALL AND PRISM_HAS_WEXTRA)
    target_compile_options(prism_common
                           INTERFACE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
  endif()
  # GCC warns that the interference sizes may differ between targets, which
  # matters only for types shared across an ABI boundary. Prism's are not.
  check_cxx_compiler_flag(-Winterference-size PRISM_HAS_WINTERFERENCE_SIZE)
  if(PRISM_HAS_WINTERFERENCE_SIZE)
    target_compile_options(
      prism_common INTERFACE $<$<COMPILE_LANGUAGE:CXX>:-Wno-interference-size>)
  endif()
  if(PRISM_HAS_WTHREAD_SAFETY)
    target_compile_options(
      prism_common INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:-Wthread-safety
                             -Werror=thread-safety>)
  endif()
endif()
# Covers lifetimebound, lifetime_capture_by and gsl::Pointer; clang-cl takes
# these too.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  check_cxx_compiler_flag(-Wdangling PRISM_HAS_WDANGLING)
  if(PRISM_HAS_WDANGLING)
    target_compile_options(
      prism_common INTERFACE $<$<COMPILE_LANGUAGE:CXX>:-Wdangling
                             -Werror=dangling>)
  endif()
endif()
if(PRISM_DIAGNOSE_VECTORIZATION)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(
      prism_common INTERFACE $<$<NOT:$<CONFIG:Debug>>:-fopt-info-vec-missed=2>)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(
      prism_common
      INTERFACE $<$<NOT:$<CONFIG:Debug>>:
                -Rpass=loop-unroll
                -Rpass-missed=loop-unroll
                -Rpass=loop-vectorize
                -Rpass-missed=loop-vectorize
                -Rpass-analysis=loop-vectorize
                -Rpass=slp-vectorize
                -Rpass-missed=slp-vectorize
                -Rpass-analysis=slp-vectorize>)
  elseif(MSVC)
    target_compile_options(prism_common
                           INTERFACE $<$<NOT:$<CONFIG:Debug>>:/Qvec-report:2>)
  endif()
endif()
