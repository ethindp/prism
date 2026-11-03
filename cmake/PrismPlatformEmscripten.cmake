# SPDX-License-Identifier: MPL-2.0
include_guard(GLOBAL)
include(PrismGuards)
prism_require_targets(prism prism_common)
target_compile_options(prism_common INTERFACE -pthread)
target_link_options(prism_common INTERFACE -pthread)
target_link_options(prism PRIVATE -pthread)
# A program linking the static library needs the same thread support, and the
# Web Speech backend needs embind.
target_compile_options(prism INTERFACE -pthread)
target_link_options(prism INTERFACE -pthread)
if(TARGET prism_backend_web_speech_synthesis)
  target_link_options(prism INTERFACE -lembind)
endif()
foreach(_d IN LISTS PRISM_COMPILED_DEP_TARGETS)
  target_compile_options(${_d} PRIVATE -pthread)
  target_link_options(${_d} PRIVATE -pthread)
endforeach()
