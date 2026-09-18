# SPDX-License-Identifier: MPL-2.0
include("../common/PrismCompatShim.cmake")

prism_add_compat_shim(sral_shim SRAL "${CMAKE_CURRENT_SOURCE_DIR}/SRAL.h"
                      sral.c)
