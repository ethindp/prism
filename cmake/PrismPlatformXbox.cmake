# SPDX-License-Identifier: MPL-2.0
include_guard(GLOBAL)
include(PrismGuards)
prism_require_targets(prism prism_common)
target_compile_definitions(prism_common INTERFACE PRISM_CONSOLE=1 PRISM_XBOX=1)
