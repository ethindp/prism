# SPDX-License-Identifier: MPL-2.0
include("../common/PrismCompatShim.cmake")

prism_add_compat_shim(speech_core_shim SpeechCore
                      "${CMAKE_CURRENT_SOURCE_DIR}/SpeechCore.h" speech_core.c)
