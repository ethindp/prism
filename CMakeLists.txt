# SPDX-License-Identifier: MPL-2.0
cmake_minimum_required(VERSION 3.24)

project(
  prism
  VERSION 0.17.3
  DESCRIPTION "The Platform-agnostic Reader Interface for Speech and Messages"
  LANGUAGES C CXX)
set(PRISM_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(PrismGuards)
include(PrismOptions)
include(PrismFacts)
include(PrismDeps)
include(PrismTargets)
include(PrismBackends)
include(PrismCodegen)
include(Prism${PRISM_PLATFORM_MODULE})
include(PrismWarnings)
include(PrismLinting)
include(PrismInstall)
if(PRISM_ENABLE_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
if(PRISM_ENABLE_DEMOS)
  add_subdirectory(demos)
endif()
if(PRISM_ENABLE_GDEXTENSION)
  add_subdirectory(gdextension)
endif()
if(PRISM_ENABLE_SHIMS)
  add_subdirectory(shims)
endif()
