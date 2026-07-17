# SPDX-License-Identifier: MPL-2.0
include_guard(GLOBAL)
include(PrismGuards)
prism_require_targets(prism prism_common)
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_library(AVFOUNDATION_FRAMEWORK AVFoundation REQUIRED)
target_link_libraries(prism PRIVATE ${FOUNDATION_FRAMEWORK}
                                    ${AVFOUNDATION_FRAMEWORK} c++)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
  target_link_libraries(prism PRIVATE ${APPKIT_FRAMEWORK})
elseif(CMAKE_SYSTEM_NAME STREQUAL "watchOS")
  find_library(WATCHKIT_FRAMEWORK WatchKit REQUIRED)
  target_link_libraries(prism PRIVATE ${WATCHKIT_FRAMEWORK})
else()
  find_library(UIKIT_FRAMEWORK UIKit REQUIRED)
  target_link_libraries(prism PRIVATE ${UIKIT_FRAMEWORK})
endif()
if(PRISM_ENABLE_POWER_MANAGEMENT AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
  find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
  target_compile_definitions(prism_common
                             INTERFACE PRISM_ENABLE_POWER_MANAGEMENT)
  target_link_libraries(prism PRIVATE ${IOKIT_FRAMEWORK}
                                      ${COREFOUNDATION_FRAMEWORK})
endif()
set_source_files_properties(
  "${PRISM_SOURCE_ROOT}/source/power_notifier.cpp" TARGET_DIRECTORY prism
  PROPERTIES COMPILE_OPTIONS "-x;objective-c++;-fobjc-arc")
target_link_options(prism PRIVATE "LINKER:-headerpad_max_install_names")
