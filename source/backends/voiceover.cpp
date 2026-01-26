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
    features |=
        SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_IS_SPEAKING | SUPPORTS_STOP;
    bool is_runtime_active = false;
#if defined(TARGET_OS_OSX) && TARGET_OS_OSX
    const Class ns_workspace_class = objc_getClass("NSWorkspace");
    if (ns_workspace_class) {
      const SEL shared_workspace_sel = sel_registerName("sharedWorkspace");
      using SharedWorkspaceFn = id (*)(Class, SEL);
      const auto get_shared_workspace =
          reinterpret_cast<SharedWorkspaceFn>(objc_msgSend);
      if (id workspace =
              get_shared_workspace(ns_workspace_class, shared_workspace_sel)) {
        const SEL is_enabled_sel = sel_registerName("isVoiceOverEnabled");
        using IsEnabledFn = BOOL (*)(id, SEL);
        const auto is_enabled_call =
            reinterpret_cast<IsEnabledFn>(objc_msgSend);
        if (is_enabled_call(workspace, is_enabled_sel)) {
          is_runtime_active = true;
        }
      }
    }
#else
    auto const ui_accessibility_class = objc_getClass("UIAccessibility");
    if (ui_accessibility_class) {
      auto const is_running_sel = sel_registerName("isVoiceOverRunning");
      using IsRunningFn = BOOL (*)(Class, SEL);
      auto const is_running_call = reinterpret_cast<IsRunningFn>(objc_msgSend);
      if (is_running_call(ui_accessibility_class, is_running_sel)) {
        is_runtime_active = true;
      }
    }
#endif
    if (is_runtime_active) {
      features |= IS_SUPPORTED_AT_RUNTIME;
    }
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