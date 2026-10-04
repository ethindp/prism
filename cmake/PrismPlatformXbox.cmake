# SPDX-License-Identifier: MPL-2.0
include_guard(GLOBAL)
include(PrismGuards)
prism_require_targets(prism prism_common)
target_compile_definitions(prism_common INTERFACE PRISM_CONSOLE=1 PRISM_XBOX=1)
if(TARGET prism_backend_xbox_speech)
  target_link_libraries(prism PRIVATE xgameruntime.lib xgameplatform.lib)
endif()
# Do not explicitly link desktop umbrella libraries such as
# onecore.lib/kernel32.lib.
