// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#include "utils.h"
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <vector>
#ifdef _WIN32
#include "raw/sapibridge.h"

class SapiBackend final : public TextToSpeechBackend {
private:
  sb_sapi *sapi{nullptr};
  std::atomic_flag initialized;
  std::atomic_flag paused;

public:
  ~SapiBackend() override {
    if (sapi != nullptr) {
      sb_sapi_cleanup(sapi);
      delete sapi;
      sapi = nullptr;
    }
  }

  std::string_view get_name() const override { return "SAPI"; }

  BackendResult<> initialize() override {
    if (sapi != nullptr) {
      return std::unexpected(BackendError::AlreadyInitialized);
    }
    sapi = new sb_sapi{};
    if (sb_sapi_initialise(sapi) == 0) {
      delete sapi;
      return std::unexpected(BackendError::InternalBackendError);
    }
    initialized.test_and_set();
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (sapi == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (!simdutf::validate_utf8(text.data(), text.size())) {
      return std::unexpected(BackendError::InvalidUtf8);
    }
    if (text.size() >= std::numeric_limits<int>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    auto str = std::string(text);
    if (interrupt) {
      if (sb_sapi_is_speaking(sapi) != 0) {
        if (sb_sapi_stop(sapi) != 0) {
          return std::unexpected(BackendError::InternalBackendError);
        }
      }
    }
    if (sb_sapi_speak(sapi, str.data(), static_cast<int>(str.size())) == 0)
      return std::unexpected(BackendError::SpeakFailure);
    return {};
  }

  BackendResult<> speak_to_memory(std::string_view text, AudioCallback callback,
                                  void *userdata) override {
    if (sapi == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (!simdutf::validate_utf8(text.data(), text.size())) {
      return std::unexpected(BackendError::InvalidUtf8);
    }
    if (text.size() >= std::numeric_limits<int>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    auto str = std::string(text);
    void *buffer = nullptr;
    int size = 0;
    if (sb_sapi_speak_to_memory(sapi, str.data(), &buffer, &size) == 0)
      return std::unexpected(BackendError::InternalBackendError);
    int channels = sb_sapi_get_channels(sapi);
    int sample_rate = sb_sapi_get_sample_rate(sapi);
    int bit_depth = sb_sapi_get_bit_depth(sapi);
    std::size_t sample_count = size / (bit_depth / 8);
    std::vector<float> samples(sample_count);
    if (bit_depth == 16) {
      const auto *src = static_cast<const int16_t *>(buffer);
      for (std::size_t i = 0; i < sample_count; ++i) {
        samples[i] = static_cast<float>(src[i]) / 32768.0F;
      }
    } else if (bit_depth == 8) {
      const auto *src = static_cast<const uint8_t *>(buffer);
      for (std::size_t i = 0; i < sample_count; ++i) {
        samples[i] = static_cast<float>(src[i] - 128) / 128.0F;
      }
    } else {
      std::free(buffer);
      return std::unexpected(BackendError::InternalBackendError);
    }
    std::free(buffer);
    callback(userdata, samples.data(), sample_count, channels, sample_rate);
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<bool> is_speaking() override {
    if (sapi == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    return sb_sapi_is_speaking(sapi);
  }

  BackendResult<> stop() override {
    if (sapi == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (sb_sapi_stop(sapi) != 0) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }

  BackendResult<> pause() override {
    if (sapi == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (paused.test())
      return std::unexpected(BackendError::AlreadyPaused);
    if (sb_sapi_pause(sapi) == 0)
      return std::unexpected(BackendError::InternalBackendError);
    paused.test_and_set();
    return {};
  }

  BackendResult<> resume() override {
    if (sapi == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (!paused.test())
      return std::unexpected(BackendError::NotPaused);
    if (sb_sapi_resume(sapi) == 0)
      return std::unexpected(BackendError::InternalBackendError);
    paused.clear();
    return {};
  }

  BackendResult<> set_volume(float volume) override {
    if (sapi == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (volume < 0.0F || volume > 1.0F)
      return std::unexpected(BackendError::RangeOutOfBounds);
    const auto val = std::round(
        range_convert_midpoint(volume, 0.0, 0.5, 1.0, -10.0, 0.0, 10.0));
    if (val < -10.0F || val > 10.0F)
      return std::unexpected(BackendError::RangeOutOfBounds);
    if (sb_sapi_set_volume(sapi, static_cast<int>(val)) == 0)
      return std::unexpected(BackendError::InternalBackendError);
    return {};
  }

  BackendResult<float> get_volume() override {
    if (sapi == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    const auto val = sb_sapi_get_volume(sapi);
    return range_convert_midpoint(static_cast<float>(val), -10, 0, 10, 0.0, 0.5,
                                  1.0);
  }

  BackendResult<> set_rate(float rate) override {
    if (sapi == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (rate < 0.0F || rate > 1.0F)
      return std::unexpected(BackendError::RangeOutOfBounds);
    const auto val =
        range_convert_midpoint(rate, 0.0, 0.5, 1.0, -10.0, 0.0, 10.0);
    if (val < -10.0F || val > 10.0F)
      return std::unexpected(BackendError::RangeOutOfBounds);
    if (sb_sapi_set_rate(sapi, static_cast<int>(val)) == 0)
      return std::unexpected(BackendError::InternalBackendError);
    return {};
  }

  BackendResult<float> get_rate() override {
    if (sapi == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    const auto val = sb_sapi_get_rate(sapi);
    return range_convert_midpoint(static_cast<float>(val), -10, 0, 10, 0.0, 0.5,
                                  1.0);
  }

  BackendResult<> set_pitch(float pitch) override {
    if (sapi == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (pitch < 0.0F || pitch > 1.0F)
      return std::unexpected(BackendError::RangeOutOfBounds);
    const auto val =
        range_convert_midpoint(pitch, 0.0, 0.5, 1.0, -10.0, 0.0, 10.0);
    if (val < -10.0F || val > 10.0F)
      return std::unexpected(BackendError::RangeOutOfBounds);
    if (sb_sapi_set_pitch(sapi, static_cast<int>(val)) == 0)
      return std::unexpected(BackendError::InternalBackendError);
    return {};
  }

  BackendResult<float> get_pitch() override {
    if (sapi == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    const auto val = sb_sapi_get_pitch(sapi);
    return range_convert_midpoint(static_cast<float>(val), -10, 0, 10, 0.0, 0.5,
                                  1.0);
  }

  BackendResult<> refresh_voices() override {
    if (sapi == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (sb_sapi_refresh_voices(sapi) == 0)
      return std::unexpected(BackendError::InternalBackendError);
    return {};
  }

  BackendResult<std::size_t> count_voices() override {
    if (sapi == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    return sb_sapi_count_voices(sapi);
  }

  BackendResult<std::string> get_voice_name(std::size_t id) override {
    if (sapi == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (id >= std::numeric_limits<int>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    if (auto *const ret = sb_sapi_get_voice_name(sapi, static_cast<int>(id));
        ret == nullptr) {
      return std::unexpected(BackendError::VoiceNotFound);
    } else {
      auto str = std::string(ret);
      return str;
    }
    return "";
  }

  BackendResult<std::string> get_voice_language(std::size_t id) override {
    if (sapi == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (id >= std::numeric_limits<int>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    if (auto *const ret =
            sb_sapi_get_voice_language(sapi, static_cast<int>(id));
        ret == nullptr) {
      return std::unexpected(BackendError::VoiceNotFound);
    } else {
      auto str = std::string(ret);
      return str;
    }
    return "";
  }

  BackendResult<> set_voice(std::size_t id) override {
    if (sapi == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (id >= std::numeric_limits<int>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    if (sb_sapi_set_voice(sapi, static_cast<int>(id)) == 0)
      return std::unexpected(BackendError::VoiceNotFound);
    return {};
  }

  BackendResult<std::size_t> get_voice() override {
    if (sapi == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    return sb_sapi_get_voice(sapi);
  }
};

REGISTER_BACKEND_WITH_ID(SapiBackend, Backends::SAPI, "SAPI", 98);
#endif
