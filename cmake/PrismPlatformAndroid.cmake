# SPDX-License-Identifier: MPL-2.0
include_guard(GLOBAL)
include(PrismGuards)
prism_require_targets(prism prism_common prism::dep::djinni)
target_link_libraries(prism_common INTERFACE prism::dep::djinni)
target_include_directories(
  prism_common
  INTERFACE
    "$<BUILD_INTERFACE:${PRISM_SOURCE_ROOT}/source/backends/android>"
    "$<BUILD_INTERFACE:${PRISM_SOURCE_ROOT}/source/backends/android/jni>")
target_link_options(prism PRIVATE "-Wl,-z,max-page-size=16384")
