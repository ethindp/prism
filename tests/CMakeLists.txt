include(${PRISM_SOURCE_ROOT}/cmake/CPM.cmake)
cpmaddpackage(
  NAME
  gtest
  GITHUB_REPOSITORY
  google/googletest
  VERSION
  1.17.0
  OPTIONS
  "INSTALL_GTEST OFF"
  "gtest_force_shared_crt ON")
include(GoogleTest)

function(prism_add_test TEST_NAME)
  add_executable(${TEST_NAME} ${ARGN})
  target_link_libraries(${TEST_NAME} PRIVATE GTest::gtest_main prism::prism)
  if(WIN32)
    target_link_libraries(${TEST_NAME} PRIVATE Rpcrt4.lib)
  endif()
  if(MSVC)
    target_compile_options(${TEST_NAME} PRIVATE /W4)
  else()
    target_compile_options(${TEST_NAME} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  gtest_discover_tests(${TEST_NAME})
endfunction()

if(WIN32)
  set(PRISM_TEST_PLATFORM "win32")
elseif(IOS)
  set(PRISM_TEST_PLATFORM "ios")
elseif(ANDROID)
  set(PRISM_TEST_PLATFORM "android")
elseif(APPLE)
  set(PRISM_TEST_PLATFORM "macos")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(PRISM_TEST_PLATFORM "linux")
else()
  message(
    STATUS
      "No platform test directory for ${CMAKE_SYSTEM_NAME}, skipping platform tests"
  )
  set(PRISM_TEST_PLATFORM "")
endif()
if(PRISM_TEST_PLATFORM)
  set(PLATFORM_TEST_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/${PRISM_TEST_PLATFORM})
  if(IS_DIRECTORY ${PLATFORM_TEST_ROOT})
    message(STATUS "Discovering tests in tests/${PRISM_TEST_PLATFORM}")
    file(
      GLOB TEST_DIRS
      RELATIVE ${PLATFORM_TEST_ROOT}
      CONFIGURE_DEPENDS ${PLATFORM_TEST_ROOT}/*)
    foreach(DIR ${TEST_DIRS})
      set(FULL_PATH ${PLATFORM_TEST_ROOT}/${DIR})
      if(IS_DIRECTORY ${FULL_PATH} AND EXISTS ${FULL_PATH}/CMakeLists.txt)
        add_subdirectory(${PRISM_TEST_PLATFORM}/${DIR})
      endif()
    endforeach()
  else()
    message(
      STATUS "tests/${PRISM_TEST_PLATFORM} not found, skipping platform tests")
  endif()
endif()
