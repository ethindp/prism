# SPDX-License-Identifier: MPL-2.0
include("../common/PrismCompatShim.cmake")

prism_add_compat_shim(
  universal_speech_shim UniversalSpeech
  "${CMAKE_CURRENT_SOURCE_DIR}/UniversalSpeech.h" universal_speech.c)

if(WIN32)
  target_sources(universal_speech_shim PRIVATE UniversalSpeech.def)
  if(MINGW)
    target_link_options(
      universal_speech_shim PRIVATE "LINKER:--add-stdcall-alias"
      "LINKER:--enable-stdcall-fixup")
  endif()
endif()
