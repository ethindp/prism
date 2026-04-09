// SPDX-License-Identifier: MPL-2.0

#include "backends/backend_registry.h" // where namespace Backends lives
#include <cstdint>
#include <prism.h>

#define U64(x) static_cast<std::uint64_t>(x)
#define CHECK(macro, backend_const)                                            \
  static_assert(U64(macro) == U64(backend_const),                              \
                "Backend ID mismatch: " #macro)

static_assert(U64(PRISM_BACKEND_INVALID) == 0ULL,
              "PRISM_BACKEND_INVALID must be 0");
CHECK(PRISM_BACKEND_SAPI, Backends::SAPI);
CHECK(PRISM_BACKEND_AV_SPEECH, Backends::AVSpeech);
CHECK(PRISM_BACKEND_VOICE_OVER, Backends::VoiceOver);
CHECK(PRISM_BACKEND_SPEECH_DISPATCHER, Backends::SpeechDispatcher);
CHECK(PRISM_BACKEND_NVDA, Backends::NVDA);
CHECK(PRISM_BACKEND_JAWS, Backends::JAWS);
CHECK(PRISM_BACKEND_ONE_CORE, Backends::OneCore);
CHECK(PRISM_BACKEND_ORCA, Backends::Orca);
CHECK(PRISM_BACKEND_ANDROID_SCREEN_READER, Backends::AndroidScreenReader);
CHECK(PRISM_BACKEND_ANDROID_TTS, Backends::AndroidTextToSpeech);
CHECK(PRISM_BACKEND_WEB_SPEECH, Backends::WebSpeechSynthesis);
CHECK(PRISM_BACKEND_UIA, Backends::UIA);
CHECK(PRISM_BACKEND_ZDSR, Backends::ZDSR);
CHECK(PRISM_BACKEND_ZOOM_TEXT, Backends::ZoomText);
CHECK(PRISM_BACKEND_BOY_PC_READER, Backends::BoyPCReader);
CHECK(PRISM_BACKEND_PC_TALKER, Backends::PCTalker);
CHECK(PRISM_BACKEND_SYSTEM_ACCESS, Backends::SystemAccess);
CHECK(PRISM_BACKEND_WINDOW_EYES, Backends::WindowEyes);

#undef CHECK
#undef U64
