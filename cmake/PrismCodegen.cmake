# SPDX-License-Identifier: MPL-2.0

include_guard(GLOBAL)
include(PrismGuards)
prism_require_targets(prism prism_common)
if(WIN32)
  prism_require_vars(PRISM_MIDL_HOST_DIR PRISM_MIDL_ENV)
  if(NOT DEFINED CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
    get_filename_component(
      WIN_SDK_ROOT
      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]"
      ABSOLUTE
      CACHE)
  else()
    set(WIN_SDK_ROOT "$ENV{ProgramFiles\(x86\)}/Windows Kits/10")
    set(WIN_SDK_VERSION "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
  endif()
  find_program(
    PRISM_MIDL_COMPILER
    NAMES midl.exe
    PATHS "${WIN_SDK_ROOT}/bin/${WIN_SDK_VERSION}/${PRISM_MIDL_HOST_DIR}"
          "${WIN_SDK_ROOT}/bin/${PRISM_MIDL_HOST_DIR}"
    DOC "Microsoft IDL Compiler")
  if(NOT PRISM_MIDL_COMPILER)
    message(
      FATAL_ERROR
        "midl.exe not found for host ${PRISM_MIDL_HOST_DIR} under ${WIN_SDK_ROOT}.\n"
    )
  endif()
  set(PRISM_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
  file(MAKE_DIRECTORY "${PRISM_GEN_DIR}")
  add_custom_command(
    OUTPUT "${PRISM_GEN_DIR}/nvda_controller.h"
           "${PRISM_GEN_DIR}/nvda_controller.c"
    COMMAND
      ${PRISM_MIDL_COMPILER} /nologo /env ${PRISM_MIDL_ENV} /client stub /cstub
      "${PRISM_GEN_DIR}/nvda_controller.c" /header
      "${PRISM_GEN_DIR}/nvda_controller.h" /server none /prefix all
      "nvdaController_" /acf "${PRISM_SOURCE_ROOT}/idl/nvdaController.acf"
      "${PRISM_SOURCE_ROOT}/idl/nvdaController.idl"
    DEPENDS "${PRISM_SOURCE_ROOT}/idl/nvdaController.idl"
            "${PRISM_SOURCE_ROOT}/idl/nvdaController.acf"
    COMMENT "MIDL: nvdaController targeting ${PRISM_MIDL_ENV}"
    VERBATIM)
  add_custom_target(
    prism_nvda_codegen DEPENDS "${PRISM_GEN_DIR}/nvda_controller.h"
                               "${PRISM_GEN_DIR}/nvda_controller.c")
  target_sources(prism PRIVATE "${PRISM_GEN_DIR}/nvda_controller.c")
  set_source_files_properties("${PRISM_GEN_DIR}/nvda_controller.c"
                              PROPERTIES SKIP_LINTING ON)
  target_include_directories(prism_common SYSTEM
                             INTERFACE "$<BUILD_INTERFACE:${PRISM_GEN_DIR}>")
  if(TARGET prism_backend_nvda)
    add_dependencies(prism_backend_nvda prism_nvda_codegen)
  endif()
  add_dependencies(prism prism_nvda_codegen)
endif()
if(PRISM_REGENERATE_DJINNI)
  set(_idl "${PRISM_SOURCE_ROOT}/idl/tts_backend.djinni")
  set(DJINNI_JAR
      ""
      CACHE
        FILEPATH
        "Path to a djinni JAR or executable (used when no 'djinni' launcher is on PATH)"
  )
  find_program(DJINNI_EXECUTABLE djinni)
  if(DJINNI_EXECUTABLE)
    set(_djinni "${DJINNI_EXECUTABLE}")
  elseif(DJINNI_JAR)
    find_package(Java REQUIRED COMPONENTS Runtime)
    set(_djinni "${Java_JAVA_EXECUTABLE}" -jar "${DJINNI_JAR}")
  else()
    message(
      FATAL_ERROR
        "PRISM_REGENERATE_DJINNI is ON but no generator was found. Put 'djinni' on "
        "PATH or set DJINNI_JAR.")
  endif()
  set(_out "${PRISM_SOURCE_ROOT}/source/backends/android")
  set(_flags
      --idl
      "${_idl}"
      --idl-include-path
      "${PRISM_SOURCE_ROOT}/third_party"
      --java-out
      "${_out}"
      --java-package
      com.github.ethindp.prism
      --java-use-final-for-record
      true
      --cpp-out
      "${_out}"
      --cpp-namespace
      prism::java
      --cpp-include-prefix
      android/
      --cpp-base-lib-include-prefix
      djinni/support/cpp/
      --jni-out
      "${_out}/jni"
      --jni-namespace
      prism::jni
      --jni-include-prefix
      android/jni/
      --jni-include-cpp-prefix
      android/
      --jni-base-lib-include-prefix
      djinni/support/jni/
      --jni-use-on-load-initializer
      true)
  set(_manifest "${CMAKE_CURRENT_BINARY_DIR}/djinni_outputs.txt")
  execute_process(
    COMMAND ${_djinni} ${_flags} --skip-generation true --list-out-files
            "${_manifest}"
    RESULT_VARIABLE _rc
    ERROR_VARIABLE _err)
  if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "Djinni manifest generation failed (${_rc}):\n${_err}")
  endif()
  file(STRINGS "${_manifest}" _raw)
  set(_outputs "")
  foreach(_p IN LISTS _raw)
    file(TO_CMAKE_PATH "${_p}" _p)
    list(APPEND _outputs "${_p}")
  endforeach()
  add_custom_command(
    OUTPUT ${_outputs}
    COMMAND ${_djinni} ${_flags}
    DEPENDS "${_idl}"
    COMMENT "Regenerating Djinni bindings"
    VERBATIM)
  add_custom_target(regen-djinni DEPENDS ${_outputs})
  if(ANDROID)
    add_dependencies(prism regen-djinni)
  endif()
endif()
