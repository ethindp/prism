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
  if(PAW_INCLUDES)
    target_include_directories(${_tgt} PRIVATE ${PAW_INCLUDES})
  endif()
  if(PAW_DEFINES)
    target_compile_definitions(${_tgt} PRIVATE ${PAW_DEFINES})
  endif()
  if(PAW_DEPS)
    target_link_libraries(${_tgt} PRIVATE ${PAW_DEPS})
  endif()
  install(
    FILES "$<TARGET_FILE_DIR:${_tgt}>/${_tgt}.dll.so"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/wine
    PERMISSIONS
      OWNER_READ
      OWNER_WRITE
      OWNER_EXECUTE
      GROUP_READ
      GROUP_EXECUTE
      WORLD_READ
      WORLD_EXECUTE)
endfunction()

function(prism_gdbus_codegen STEM NAMESPACE OUT_SOURCES_VAR)
  if(NOT GDBUS_CODEGEN)
    message(FATAL_ERROR "prism_gdbus_codegen: GDBUS_CODEGEN not found")
  endif()
  if(NOT PRISM_DBUS_XML_DIR)
    message(FATAL_ERROR "prism_gdbus_codegen: PRISM_DBUS_XML_DIR not set")
  endif()
  set(_xml ${PRISM_DBUS_XML_DIR}/${STEM}.xml)
  set(_h ${CMAKE_CURRENT_BINARY_DIR}/gen/${STEM}.h)
  set(_c ${CMAKE_CURRENT_BINARY_DIR}/gen/${STEM}.c)
  add_custom_command(
    OUTPUT ${_h} ${_c}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/gen
    COMMAND
      ${GDBUS_CODEGEN} --c-namespace ${NAMESPACE} --c-generate-object-manager
      --c-generate-autocleanup all --pragma-once --header --output ${_h} ${_xml}
    COMMAND
      ${GDBUS_CODEGEN} --c-namespace ${NAMESPACE} --c-generate-object-manager
      --c-generate-autocleanup all --pragma-once --body --output ${_c} ${_xml}
    DEPENDS ${_xml}
    VERBATIM)
  set(${OUT_SOURCES_VAR}
      ${_c} ${_h}
      PARENT_SCOPE)
endfunction()
