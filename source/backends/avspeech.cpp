// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#include "utils.h"
#include <limits>
#ifdef __APPLE__
#include "raw/avspeech.h"
#include <objc/message.h>
#include <objc/runtime.h>

class AVSpeechBackend final : public TextToSpeechBackend {
private:
  AVSpeechContext *ctx{nullptr};

public:
  ~AVSpeechBackend() {
    if (ctx != nullptr)
      avspeech_cleanup(ctx);
  }

  std::string_view get_name() const override { return "AVSpeech"; }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    Class cls = objc_getClass("AVSpeechSynthesisVoice");
    if (cls) {
      SEL sel = sel_registerName("speechVoices");
      id voices = reinterpret_cast<id (*)(Class, SEL)>(objc_msgSend)(cls, sel);
      SEL countSel = sel_registerName("count");
      auto count = reinterpret_cast<unsigned long (*)(id, SEL)>(objc_msgSend)(
          voices, countSel);
      if (count > 0) {
        features |= IS_SUPPORTED_AT_RUNTIME;
      }
    }
    features |= SUPPORTS_SPEAK | SUPPORTS_SPEAK_TO_MEMORY | SUPPORTS_OUTPUT |
                SUPPORTS_IS_SPEAKING | SUPPORTS_STOP | SUPPORTS_PAUSE |
                SUPPORTS_RESUME | SUPPORTS_SET_VOLUME | SUPPORTS_GET_VOLUME |
                SUPPORTS_SET_RATE | SUPPORTS_GET_RATE | SUPPORTS_SET_PITCH |
                SUPPORTS_GET_PITCH | SUPPORTS_REFRESH_VOICES |
                SUPPORTS_COUNT_VOICES | SUPPORTS_GET_VOICE_NAME |
                SUPPORTS_GET_VOICE_LANGUAGE | SUPPORTS_GET_VOICE |
                SUPPORTS_SET_VOICE | SUPPORTS_GET_CHANNELS |
                SUPPORTS_GET_SAMPLE_RATE | SUPPORTS_GET_BIT_DEPTH;
    return features;
  }

  BackendResult<> initialize() override {
    if (ctx != nullptr)
      return std::unexpected(BackendError::AlreadyInitialized);
    if (const auto res = avspeech_initialize(&ctx); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (ctx == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (interrupt) {
      if (const auto res = stop(); !res)
        return res;
    }
    if (!simdutf::validate_utf8(text.data(), text.size())) {
      return std::unexpected(BackendError::InvalidUtf8);
    }
    if (const auto res = avspeech_speak(ctx, text.data()); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<bool> is_speaking() override {
    if (ctx == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    return avspeech_is_speaking(ctx);
  }

  BackendResult<> stop() override {
    if (ctx == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = avspeech_stop(ctx); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return {};
  }

  BackendResult<> pause() override {
    if (ctx == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = avspeech_pause(ctx); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return {};
  }

  BackendResult<> resume() override {
    if (ctx == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = avspeech_resume(ctx); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return {};
  }

  BackendResult<> set_volume(float volume) override {
    if (ctx == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (volume < 0.0F || volume > 1.0F) {
      return std::unexpected(BackendError::RangeOutOfBounds);
    }
    if (const auto res = avspeech_set_volume(ctx, volume); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return {};
  }

  BackendResult<float> get_volume() override {
    if (ctx == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    float volume = 0.0F;
    if (const auto res = avspeech_get_volume(ctx, &volume);
        res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return volume;
  }

  BackendResult<> set_rate(float rate) override {
    if (ctx == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (rate < 0.0F || rate > 1.0F) {
      return std::unexpected(BackendError::RangeOutOfBounds);
    }
    const auto val = range_convert_midpoint(
        rate, 0.0, 0.5, 1.0, avspeech_get_rate_min(),
        avspeech_get_rate_default(), avspeech_get_rate_max());
    if (const auto res = avspeech_set_rate(ctx, val); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return {};
  }

  BackendResult<float> get_rate() override {
    if (ctx == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    float rate = 0.0F;
    if (const auto res = avspeech_get_rate(ctx, &rate); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return range_convert_midpoint(rate, avspeech_get_rate_min(),
                                  avspeech_get_rate_default(),
                                  avspeech_get_rate_max(), 0.0, 0.5, 1.0);
  }

  BackendResult<> set_pitch(float pitch) override {
    if (ctx == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (pitch < 0.0F || pitch > 1.0F) {
      return std::unexpected(BackendError::RangeOutOfBounds);
    }
    if (const auto res = avspeech_set_pitch(ctx, pitch); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return {};
  }

  BackendResult<float> get_pitch() override {
    if (ctx == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    float pitch = 0.0F;
    if (const auto res = avspeech_get_pitch(ctx, &pitch); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return pitch;
  }

  BackendResult<> refresh_voices() override {
    if (ctx == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = avspeech_refresh_voices(ctx); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return {};
  }

  BackendResult<std::size_t> count_voices() override {
    if (ctx == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    int count;
    if (const auto res = avspeech_count_voices(ctx, &count);
        res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return count;
  }

  BackendResult<std::string> get_voice_name(std::size_t id) override {
    if (ctx == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (id >= std::numeric_limits<int>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    const char *name;
    if (const auto res =
            avspeech_get_voice_name(ctx, static_cast<int>(id), &name);
        res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return std::string(name);
  }

  BackendResult<std::string> get_voice_language(std::size_t id) override {
    if (ctx == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (id >= std::numeric_limits<int>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    const char *lang;
    if (const auto res =
            avspeech_get_voice_language(ctx, static_cast<int>(id), &lang);
        res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return std::string(lang);
  }

  BackendResult<> set_voice(std::size_t id) override {
    if (ctx == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (id >= std::numeric_limits<int>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    if (const auto res = avspeech_set_voice(ctx, static_cast<int>(id));
        res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return {};
  }

  BackendResult<std::size_t> get_voice() override {
    if (ctx == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    int id;
    if (const auto res = avspeech_get_voice(ctx, &id); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return static_cast<std::size_t>(id);
  }
};

REGISTER_BACKEND_WITH_ID(AVSpeechBackend, Backends::AVSpeech, "AVSpeech", 97);
#endif
