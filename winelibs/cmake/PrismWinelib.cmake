get_filename_component(_prism_winegcc_dir "${CMAKE_C_COMPILER}" DIRECTORY)
find_program(
  PRISM_WINEBUILD
  NAMES winebuild
  HINTS "${_prism_winegcc_dir}" "${_prism_winegcc_dir}/../lib/wine"
        "${_prism_winegcc_dir}/../lib64/wine" REQUIRED)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(PRISM_WINEBUILD_ARCH -m64)
else()
  set(PRISM_WINEBUILD_ARCH -m32)
endif()
if(PRISM_ENABLE_LINTING)
  find_program(PRISM_WINELIB_CLANG_TIDY NAMES clang-tidy-22 clang-tidy-21
                                              clang-tidy-20 clang-tidy REQUIRED)
endif()
function(prism_add_winelib NAME)
  cmake_parse_arguments(PAW "" "SPEC" "SOURCES;DEPS;INCLUDES;DEFINES" ${ARGN})
  if(NOT PAW_SOURCES)
    message(FATAL_ERROR "prism_add_winelib(${NAME}): SOURCES is required")
  endif()
  if(NOT PAW_SPEC)
    message(FATAL_ERROR "prism_add_winelib(${NAME}): SPEC is required")
  endif()
  if(PAW_UNPARSED_ARGUMENTS)
    message(
      FATAL_ERROR
        "prism_add_winelib(${NAME}): unknown args ${PAW_UNPARSED_ARGUMENTS}")
  endif()
  set(_tgt prism_${NAME}_bridge)
  add_library(${_tgt} SHARED ${PAW_SOURCES})
  target_link_options(${_tgt} PRIVATE "${PAW_SPEC}")
  set_property(
    TARGET ${_tgt}
    APPEND
    PROPERTY LINK_DEPENDS "${PAW_SPEC}")
  set_target_properties(${_tgt} PROPERTIES NO_SONAME TRUE)
  target_compile_features(
    ${_tgt} PRIVATE cxx_std_23 c_std_17 c_function_prototypes c_restrict
                    c_static_assert c_variadic_macros)
  target_include_directories(${_tgt}
                             PRIVATE "${PRISM_WINELIB_SHARED_INCLUDE_DIR}")
  if(PAW_INCLUDES)
    target_include_directories(${_tgt} PRIVATE ${PAW_INCLUDES})
  endif()
  if(PAW_DEFINES)
    target_compile_definitions(${_tgt} PRIVATE ${PAW_DEFINES})
  endif()
  if(PAW_DEPS)
    target_link_libraries(${_tgt} PRIVATE ${PAW_DEPS})
  endif()
  target_compile_options(${_tgt} PRIVATE -Wall -Wextra)
  if(PRISM_ENABLE_LINTING)
    set_target_properties(
      ${_tgt} PROPERTIES C_CLANG_TIDY "${PRISM_WINELIB_CLANG_TIDY}"
                         CXX_CLANG_TIDY "${PRISM_WINELIB_CLANG_TIDY}")
  endif()
  set(_placeholder "${CMAKE_CURRENT_BINARY_DIR}/placeholder/${_tgt}.dll")
  add_custom_command(
    OUTPUT "${_placeholder}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/placeholder"
    COMMAND "${PRISM_WINEBUILD}" --dll --fake-module ${PRISM_WINEBUILD_ARCH} -E
            "${PAW_SPEC}" -F "${_tgt}.dll" -o "${_placeholder}"
    DEPENDS "${PAW_SPEC}"
    VERBATIM)
  add_custom_target(${_tgt}_placeholder ALL DEPENDS "${_placeholder}")
  install(FILES "${_placeholder}" DESTINATION wine/placeholders)
  install(
    FILES "$<TARGET_FILE_DIR:${_tgt}>/${_tgt}.dll.so"
    DESTINATION wine
    PERMISSIONS
      OWNER_READ
      OWNER_WRITE
      OWNER_EXECUTE
      GROUP_READ
      GROUP_EXECUTE
      WORLD_READ
      WORLD_EXECUTE)
endfunction()
