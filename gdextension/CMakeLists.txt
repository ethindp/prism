cmake_minimum_required(VERSION 3.17)

set(LIBNAME "prismatoid")
set(GODOT_PROJECT_DIR
    "project"
    CACHE STRING "The directory of a Godot project folder")

find_package(Python3 3.4 REQUIRED)
find_program(GIT git REQUIRED)

if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/godot-cpp/src")
  message(NOTICE "godot-cpp bindings source not found")
  message(NOTICE "initializing/updating the godot-cpp submodule...")
  execute_process(
    COMMAND git submodule update --init godot-cpp
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} COMMAND_ERROR_IS_FATAL ANY)
endif()
add_subdirectory(godot-cpp SYSTEM)
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${godot-cpp_SOURCE_DIR}/cmake")
include(GodotCPPModule)
get_target_property(GODOTCPP_SUFFIX godot::cpp GODOTCPP_SUFFIX)
get_target_property(GODOTCPP_PLATFORM godot::cpp GODOTCPP_PLATFORM)
project(
  prismatoid
  VERSION 0.11.3
  DESCRIPTION "Prism bindings for Godot"
  LANGUAGES C CXX)
add_library(${LIBNAME} SHARED)
target_sources(
  ${LIBNAME}
  PRIVATE src/register_types.cpp src/register_types.h
          src/godot_prism_context.cpp src/godot_prism_context.h
          src/godot_prism_backend.cpp src/godot_prism_backend.h)
target_link_libraries(${LIBNAME} PRIVATE godot-cpp prism)
if(APPLE)
  set_target_properties(${LIBNAME} PROPERTIES LINKER_LANGUAGE CXX)
  target_link_directories(${LIBNAME} PRIVATE
                          ${CMAKE_Swift_IMPLICIT_LINK_DIRECTORIES})
  target_link_libraries(${LIBNAME}
                        PRIVATE ${CMAKE_Swift_IMPLICIT_LINK_LIBRARIES})
endif()
set_property(TARGET ${LIBNAME} PROPERTY CXX_STANDARD 23)
set_property(TARGET ${LIBNAME} PROPERTY C_STANDARD 18)
set_target_properties(
  ${LIBNAME}
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY
             "$<1:${PROJECT_SOURCE_DIR}/bin/${GODOTCPP_PLATFORM}>"
             PREFIX ""
             OUTPUT_NAME "${LIBNAME}${GODOTCPP_SUFFIX}")
set(GODOT_PROJECT_BINARY_DIR
    "${PROJECT_SOURCE_DIR}/${GODOT_PROJECT_DIR}/bin/${GODOTCPP_PLATFORM}")
add_custom_command(
  TARGET ${LIBNAME}
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${LIBNAME}>"
          "${GODOT_PROJECT_BINARY_DIR}/$<TARGET_FILE_NAME:${LIBNAME}>")
