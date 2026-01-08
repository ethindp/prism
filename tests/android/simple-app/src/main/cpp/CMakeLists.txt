cmake_minimum_required(VERSION 3.22.1)
project(prism_android_test LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(PRISM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../../..")
set(PRISM_ENABLE_TESTS
    OFF
    CACHE BOOL "Disable Prism tests for Android test app" FORCE)
add_subdirectory("${PRISM_ROOT}" "${CMAKE_BINARY_DIR}/prism_build")
add_library(prism_android_test SHARED prism_android_test_jni.cpp)
target_include_directories(prism_android_test PRIVATE "${PRISM_ROOT}/include")
target_link_libraries(prism_android_test PRIVATE prism android log)
