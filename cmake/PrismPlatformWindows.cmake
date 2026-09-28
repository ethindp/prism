# SPDX-License-Identifier: MPL-2.0
include_guard(GLOBAL)
include(PrismGuards)
prism_require_targets(prism prism_common)
prism_require_vars(PRISM_ARCH_CLASS PRISM_LINK_VIS)
target_compile_definitions(
  prism_common
  INTERFACE _CRT_SECURE_NO_WARNINGS _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1
            _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT=1
            BELT_COM_NO_LEAK_DETECTION UNICODE _UNICODE)
target_link_libraries(
  prism PRIVATE ole32.lib onecore.lib runtimeobject.lib
                uiautomationcore.lib rpcrt4)
if(PRISM_ENABLE_POWER_MANAGEMENT)
  target_compile_definitions(prism_common
                             INTERFACE PRISM_ENABLE_POWER_MANAGEMENT)
  target_link_libraries(prism PRIVATE PowrProf)
endif()
if(NOT MINGW)
  if(MSVC)
    target_link_options(prism PRIVATE "/STACK:8388608")
  else()
    target_link_options(prism PRIVATE "LINKER:/STACK:8388608")
  endif()
endif()
