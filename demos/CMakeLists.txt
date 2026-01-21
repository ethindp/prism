cmake_minimum_required(VERSION 3.22)
project(prism_demos LANGUAGES C CXX)
if(WIN32)
  add_executable(uia_backend_demo WIN32 uia_backend_demo.cpp)
  target_compile_features(uia_backend_demo PRIVATE cxx_std_23)
  target_compile_definitions(uia_backend_demo PRIVATE UNICODE _UNICODE)
  target_link_libraries(uia_backend_demo PRIVATE user32 gdi32 prism)
  set_property(TARGET uia_backend_demo PROPERTY CXX_CLANG_TIDY
                                                "${CMAKE_CXX_CLANG_TIDY}")
  set_target_properties(uia_backend_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                                    "${CMAKE_BINARY_DIR}")
  if(CMAKE_CONFIGURATION_TYPES)
    foreach(cfg ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${cfg}" cfg_u)
      set_target_properties(
        uia_backend_demo PROPERTIES "RUNTIME_OUTPUT_DIRECTORY_${cfg_u}"
                                    "${CMAKE_BINARY_DIR}/${cfg}")
    endforeach()
  endif()
endif()
add_executable(nvda_backend_demo nvda_backend_demo.cpp)
target_compile_features(nvda_backend_demo PRIVATE cxx_std_23)
target_link_libraries(nvda_backend_demo PRIVATE prism)
set_property(TARGET nvda_backend_demo PROPERTY CXX_CLANG_TIDY
                                               "${CMAKE_CXX_CLANG_TIDY}")
set_target_properties(nvda_backend_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                                   "${CMAKE_BINARY_DIR}")
if(CMAKE_CONFIGURATION_TYPES)
  foreach(cfg ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${cfg}" cfg_u)
    set_target_properties(
      nvda_backend_demo PROPERTIES "RUNTIME_OUTPUT_DIRECTORY_${cfg_u}"
                                   "${CMAKE_BINARY_DIR}/${cfg}")
  endforeach()
endif()
