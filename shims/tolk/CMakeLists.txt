# SPDX-License-Identifier: MPL-2.0
include("../common/PrismCompatShim.cmake")

prism_add_compat_shim(tolk tolk "${CMAKE_CURRENT_SOURCE_DIR}/tolk.h" tolk.c)
target_compile_definitions(tolk PRIVATE TOLK_BUILDING)
