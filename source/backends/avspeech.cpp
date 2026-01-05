// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#include "utils.h"
#include <limits>
#ifdef __APPLE__
#include "raw/avspeech.h"

class AVSpeechBackend final : public TextToSpeechBackend {
private:
  AVSpeechContext *ctx;

public:
  ~AVSpeechBackend() {
    if (ctx)
      avspeech_cleanup(ctx);
  }

  std::string_view get_name() const override { return "AVSpeech"; }

  BackendResult<> initialize() override {
    if (ctx)
      return std::unexpected(BackendError::AlreadyInitialized);
    if (const auto res = avspeech_initialize(&ctx); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!ctx)
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
    if (!ctx)
      return std::unexpected(BackendError::NotInitialized);
    return avspeech_is_speaking(ctx);
  }

  BackendResult<> stop() override {
    if (!ctx)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = avspeech_stop(ctx); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return {};
  }

  BackendResult<> pause() override {
    if (!ctx)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = avspeech_pause(ctx); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return {};
  }

  BackendResult<> resume() override {
    if (!ctx)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = avspeech_resume(ctx); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return {};
  }

  BackendResult<> set_volume(float volume) override {
    if (!ctx)
      return std::unexpected(BackendError::NotInitialized);
    if (volume < 0.0 || volume > 1.0) {
      return std::unexpected(BackendError::RangeOutOfBounds);
    }
    if (const auto res = avspeech_set_volume(ctx, volume); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return {};
  }

  BackendResult<float> get_volume() override {
    if (!ctx)
      return std::unexpected(BackendError::NotInitialized);
    float volume = 0.0;
    if (const auto res = avspeech_get_volume(ctx, &volume);
        res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return volume;
  }

  BackendResult<> set_rate(float rate) override {
    if (!ctx)
      return std::unexpected(BackendError::NotInitialized);
    if (rate < 0.0 || rate > 1.0) {
      return std::unexpected(BackendError::RangeOutOfBounds);
    }
    const auto val = range_convert_midpoint(
        rate, 0.0, 0.5, 1.0, avspeech_get_rate_min(ctx),
        avspeech_get_rate_default(ctx), avspeech_get_rate_max(ctx));
    if (const auto res = avspeech_set_rate(ctx, val); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return {};
  }

  BackendResult<float> get_rate() override {
    if (!ctx)
      return std::unexpected(BackendError::NotInitialized);
    float rate = 0.0;
    if (const auto res = avspeech_get_rate(ctx, &rate); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return range_convert_midpoint(rate, avspeech_get_rate_min(ctx),
                                  avspeech_get_rate_default(ctx),
                                  avspeech_get_rate_max(ctx), 0.0, 0.5, 1.0);
  }

  BackendResult<> set_pitch(float pitch) override {
    if (!ctx)
      return std::unexpected(BackendError::NotInitialized);
    if (pitch < 0.0 || pitch > 1.0) {
      return std::unexpected(BackendError::RangeOutOfBounds);
    }
    if (const auto res = avspeech_set_pitch(ctx, pitch); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return {};
  }

  BackendResult<float> get_pitch() override {
    if (!ctx)
      return std::unexpected(BackendError::NotInitialized);
    float pitch = 0.0;
    if (const auto res = avspeech_get_pitch(ctx, &pitch); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return pitch;
  }

  BackendResult<> refresh_voices() override {
    if (!ctx)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = avspeech_refresh_voices(ctx); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return {};
  }

  BackendResult<std::size_t> count_voices() override {
    if (!ctx)
      return std::unexpected(BackendError::NotInitialized);
    int count;
    if (const auto res = avspeech_count_voices(ctx, &count);
        res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return count;
  }

  BackendResult<std::string> get_voice_name(std::size_t id) override {
    if (!ctx)
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

  BackendResult<std::string> get_voice_name(std::size_t id) override {
    if (!ctx)
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
    if (!ctx)
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
    if (!ctx)
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
    if (!ctx)
      return std::unexpected(BackendError::NotInitialized);
    int id;
    if (const auto res = avspeech_get_voice(ctx, &id); res != AVSPEECH_OK) {
      return std::unexpected(static_cast<BackendError>(res));
    }
    return static_cast<std::size_t>(id);
  }
};

REGISTER_BACKEND_WITH_ID(AVSpeechBackend, Backends::AVSpeech, "AVSpeech", 98);
#endif
