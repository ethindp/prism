# SPDX-License-Identifier: MPL-2.0
include_guard(GLOBAL)
include(PrismGuards)
include(CheckCXXCompilerFlag)
prism_require_targets(prism_common)
if(MSVC)
  check_cxx_compiler_flag(/utf-8 PRISM_MSVC_UTF8)
  check_cxx_compiler_flag(/bigobj PRISM_MSVC_BIGOBJ)
  check_cxx_compiler_flag(/Zc:enumTypes PRISM_MSVC_ZC_ENUMTYPES)
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
else()
  check_cxx_compiler_flag(-Wall PRISM_HAS_WALL)
  check_cxx_compiler_flag(-Wextra PRISM_HAS_WEXTRA)
  check_cxx_compiler_flag(-Wthread-safety PRISM_HAS_WTHREAD_SAFETY)
  if(PRISM_HAS_WALL AND PRISM_HAS_WEXTRA)
    target_compile_options(prism_common
                           INTERFACE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
  endif()
  if(PRISM_HAS_WTHREAD_SAFETY)
    target_compile_options(
      prism_common INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:-Wthread-safety
                             -Werror=thread-safety>)
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
