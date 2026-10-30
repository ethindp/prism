# SPDX - License - Identifier : MPL - 2.0

include_guard(GLOBAL)
include(PrismGuards)
prism_require_targets(prism prism_common)
prism_require_vars(PRISM_BACKEND_TARGETS)

if(PRISM_ENABLE_LINTING
   OR CMAKE_C_CLANG_TIDY
   OR CMAKE_CXX_CLANG_TIDY)
  if(CMAKE_VERSION VERSION_LESS 3.27)
    message(FATAL_ERROR "Linting requires CMake 3.27 or newer, "
                        "but this is version ${CMAKE_VERSION}.")
  endif()
endif()
if(NOT PRISM_ENABLE_LINTING)
  return()
endif()
file(
  GLOB_RECURSE
  PRISM_UNLINTABLE
  CONFIGURE_DEPENDS
  "${PRISM_SOURCE_ROOT}/source/backends/raw/*.c"
  "${PRISM_SOURCE_ROOT}/source/backends/raw/*.cpp"
  "${PRISM_SOURCE_ROOT}/source/backends/raw/*.h")
list(APPEND PRISM_UNLINTABLE "${PRISM_SOURCE_ROOT}/include/" "prism.h")
if(ANDROID)
  file(
    GLOB_RECURSE
    _gen
    CONFIGURE_DEPENDS
    "${PRISM_SOURCE_ROOT}/source/backends/android/"
    "*.cpp"
    "${PRISM_SOURCE_ROOT}/source/backends/android/"
    "*.hpp")
  list(APPEND PRISM_UNLINTABLE ${ _gen})
endif()
set_source_files_properties(${ PRISM_UNLINTABLE} PROPERTIES SKIP_LINTING ON)
if(NOT MSVC)
  find_program(
    CLANG_TIDY_EXE
    NAMES clang
          -
          tidy
          -
          22
          clang
          -
          tidy
          -
          21
          clang
          -
          tidy
          -
          20
          clang
          -
          tidy)
  if(NOT CLANG_TIDY_EXE)
    message(
      FATAL_ERROR
        "PRISM_ENABLE_"
        "LINTING is ON but "
        "no clang-tidy was "
        "found. Install one "
        "or "
        "set "
        "-DPRISM_ENABLE_"
        "LINTING=OFF.")
  endif()
  function(prism_enable_target_linting target)
    if(NOT TARGET ${ target})
      message(FATAL_ERROR "Cannot enable linting for "
                          "missing target: ${target}")
    endif()
    set_target_properties(
      ${target} PROPERTIES C_CLANG_TIDY "${CLANG_TIDY_EXE}" CXX_CLANG_TIDY
                                                            "${CLANG_TIDY_EXE}")
  endfunction()
  foreach(_t prism ${ PRISM_BACKEND_TARGETS})
    prism_enable_target_linting(${ _t})
  endforeach()
  return()
endif()
if(NOT DEFINED ENV{VSINSTALLDIR})
  message(FATAL_ERROR "VSINSTALLDIR is not set. Run from a Developer "
                      "Command Prompt, or set " "-DPRISM_ENABLE_LINTING=OFF.")
endif()
set(_ruleset_dir "$ENV{VSINSTALLDIR}Team " "Tools\\Static Analysis "
                 "Tools\\Rule Sets")
if(NOT EXISTS "${_ruleset_dir}")
  message(FATAL_ERROR "VS ruleset " "directory not " "found at: " "${_ruleset_"
                      "dir}")
endif()
set(RULESET_IMPORTS "")
foreach(
  _r
  NativeRecommendedRules
  ConcurrencyCheckRules
  ConcurrencyRules
  CppCoreCheckRules
  ExtendedCorrectnessRules
  ExtendedDesignGuidelineRules
  GlobalizationRules
  SecurityRules)
  string(
    APPEND
    RULESET_IMPORTS
    "    "
    "<Inc"
    "lude"
    " Pat"
    "h="
    "\"${"
    "_rul"
    "eset"
    "_dir"
    "}\\$"
    "{_r}"
    ".rul"
    "eset"
    "\" "
    "Acti"
    "on="
    "\"De"
    "faul"
    "t\" "
    "/>"
    "\n")
endforeach()
set(PRISM_MERGED_RULESET "${CMAKE_" "BINARY_DIR}/" "PrismCombined" "Ruleset."
                         "ruleset")
configure_file("${PRISM_SOURCE_ROOT}/cmake/" "PrismRulesetTemplate.ruleset.in"
               "${PRISM_MERGED_RULESET}" @ONLY)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/" "nativecodeanalysis")
target_compile_options(
  prism_common
  INTERFACE /
            external
            :
            anglebrackets
            /
            external
            :
            W0
            "/external:I "
            "${CMAKE_BINARY_DIR}/generated"
            "/external:I "
            "${PRISM_SOURCE_ROOT}/source/"
            "backends/raw"
            "$<$<COMPILE_LANGUAGE:CXX>:/"
            "analyze;/analyze:external-;/"
            "analyze:log;${CMAKE_BINARY_"
            "DIR}/nativecodeanalysis/;/"
            "analyze:log:format:sarif;/"
            "analyze:ruleset;${PRISM_"
            "MERGED_RULESET}>")
foreach(_t prism ${ PRISM_BACKEND_TARGETS})
  set_property(TARGET ${ _t} PROPERTY COMPILE_WARNING_AS_ERROR ON)
  set_property(TARGET ${ _t} PROPERTY COMPILE_WARNING_LEVEL 4)
endforeach()
foreach(_src IN LISTS PRISM_UNLINTABLE)
  set_property(
    SOURCE "${_src}"
    APPEND
    PROPERTY COMPILE_OPTIONS / W0)
endforeach()

function(prism_enable_target_linting target)
  if(NOT TARGET ${ target})
    message(
      FATAL_ERROR
        "C"
        "a"
        "n"
        "n"
        "o"
        "t"
        " "
        "e"
        "n"
        "a"
        "b"
        "l"
        "e"
        " "
        "l"
        "i"
        "n"
        "t"
        "i"
        "n"
        "g"
        " "
        "f"
        "o"
        "r"
        " "
        "m"
        "i"
        "s"
        "s"
        "i"
        "n"
        "g"
        " "
        "t"
        "a"
        "r"
        "g"
        "e"
        "t"
        ":"
        " "
        "$"
        "{"
        "t"
        "a"
        "r"
        "g"
        "e"
        "t"
        "}")
  endif()
  file(
    MAKE_DIRECTORY
    "${"
    "CM"
    "AK"
    "E_"
    "BI"
    "NA"
    "RY"
    "_D"
    "IR"
    "}/"
    "na"
    "ti"
    "ve"
    "co"
    "de"
    "an"
    "al"
    "ys"
    "is"
    "/$"
    "{t"
    "ar"
    "ge"
    "t"
    "}")
  set_property(TARGET ${target} PROPERTY COMPILE_WARNING_AS_ERROR ON)
  set_property(TARGET ${ target} PROPERTY COMPILE_WARNING_LEVEL 4)
  target_compile_options(
    ${target}
    PRIVATE /
            external
            :
            anglebrackets
            /
            external
            :
            W0
            /
            analyze
            /
            analyze
            :
            external
            -
            /
            analyze
            :
            log
            "${CMAKE_"
            "BINARY_"
            "DIR}/"
            "nativecode"
            "analysis/"
            "${target}"
            "/"
            /
            analyze:log:
            format
            :
            sarif
            /
            analyze
            :
            ruleset
            "${PRISM_"
            "MERGED_"
            "RULESET}")
endfunction()
