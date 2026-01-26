// SPDX-License-Identifier: MPL-2.0
#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#include <atomic>
#ifdef __APPLE__
#include "raw/voiceover.h"
#include <TargetConditionals.h>
#include <objc/message.h>
#include <objc/runtime.h>
#if TARGET_OS_OSX
#include <ApplicationServices/ApplicationServices.h>
#endif

class VoiceOverBackend final : public TextToSpeechBackend {
  std::atomic_flag inited;

public:
  ~VoiceOverBackend() override {
    if (inited.test()) {
#if TARGET_OS_OSX
      voiceover_macos_shutdown();
#else
      voiceover_ios_shutdown();
#endif
    }
  }

  std::string_view get_name() const override {
#if TARGET_OS_OSX
    return "VoiceOver (macOS)";
#elif TARGET_OS_VISION
    return "VoiceOver (visionOS)";
#elif TARGET_OS_TV
    return "VoiceOver (tvOS)";
#elif TARGET_OS_WATCH
    return "VoiceOver (watchOS)";
#else
    return "VoiceOver (iOS)";
#endif
  }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
#if TARGET_OS_OSX
    if (AXIsVoiceOverRunning()) {
      features |= IS_SUPPORTED_AT_RUNTIME;
    }
#else
    Class cls = objc_getClass("UIAccessibility");
    if (cls) {
      SEL sel = sel_registerName("isVoiceOverRunning");
      if (reinterpret_cast<BOOL (*)(Class, SEL)>(objc_msgSend)(cls, sel)) {
        features |= IS_SUPPORTED_AT_RUNTIME;
      }
    }
#endif
    features |=
        SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_IS_SPEAKING | SUPPORTS_STOP;
    return features;
  }

  BackendResult<> initialize() override {
    if (inited.test())
      return std::unexpected(BackendError::AlreadyInitialized);
#if TARGET_OS_OSX
    if (const auto r = voiceover_macos_initialize(); r != 0)
      return std::unexpected(static_cast<BackendError>(r));
#else
    if (const auto r = voiceover_ios_initialize(); r != 0)
      return std::unexpected(static_cast<BackendError>(r));
#endif
    inited.test_and_set();
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!inited.test())
      return std::unexpected(BackendError::NotInitialized);
    if (!simdutf::validate_utf8(text.data(), text.size()))
      return std::unexpected(BackendError::InvalidUtf8);
#if TARGET_OS_OSX
    if (const auto r = voiceover_macos_speak(text.data(), interrupt); r != 0)
      return std::unexpected(static_cast<BackendError>(r));
#else
    if (const auto r = voiceover_ios_speak(text.data(), interrupt); r != 0)
      return std::unexpected(static_cast<BackendError>(r));
#endif
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<bool> is_speaking() override {
    if (!inited.test())
      return std::unexpected(BackendError::NotInitialized);
    bool out = false;
#if TARGET_OS_OSX
    if (const auto r = voiceover_macos_is_speaking(&out); r != 0)
      return std::unexpected(static_cast<BackendError>(r));
#else
    if (const auto r = voiceover_ios_is_speaking(&out); r != 0)
      return std::unexpected(static_cast<BackendError>(r));
#endif
    return out;
  }

  BackendResult<> stop() override {
    if (!inited.test())
      return std::unexpected(BackendError::NotInitialized);
#if TARGET_OS_OSX
    if (const auto r = voiceover_macos_stop(); r != 0)
      return std::unexpected(static_cast<BackendError>(r));
#else
    if (const auto r = voiceover_ios_stop(); r != 0)
      return std::unexpected(static_cast<BackendError>(r));
#endif
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(VoiceOverBackend, Backends::VoiceOver, "VoiceOver",
                         102);
#endif