// SPDX-License-Identifier: MPL-2.0

#include "backend.h"
#include "backend_registry.h"
#include "utils.h"
#include <atomic>
#include <cmath>
#include <limits>
#ifdef _WIN32
#include "raw/sapibridge.h"

class SapiBackend final : public TextToSpeechBackend {
private:
  sb_sapi *sapi{nullptr};
  std::atomic_flag initialized;
  std::atomic_flag paused;

public:
  ~SapiBackend() override {
    if (sapi) {
      sb_sapi_cleanup(sapi);
      delete sapi;
      sapi = nullptr;
    }
  }

  std::string_view get_name() const override { return "SAPI"; }

  BackendResult<> initialize() override {
    if (sapi) {
      sb_sapi_cleanup(sapi);
      delete sapi;
      sapi = nullptr;
    }
    sapi = new sb_sapi{};
    if (!sb_sapi_initialise(sapi))
      return std::unexpected(BackendError::InternalBackendError);
    initialized.test_and_set();
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!sapi || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    auto str = std::string(text);
    if (interrupt) {
      if (sb_sapi_is_speaking(sapi)) {
        if (!sb_sapi_stop(sapi)) {
          return std::unexpected(BackendError::InternalBackendError);
        }
      }
    }
    if (!sb_sapi_speak(sapi, str.data(), str.size()))
      return std::unexpected(BackendError::SpeakFailure);
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<bool> is_speaking() override {
    if (!sapi || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    return sb_sapi_is_speaking(sapi);
  }

  BackendResult<> stop() override {
    if (!sapi || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (!sb_sapi_stop(sapi)) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }

  BackendResult<> pause() override {
    if (!sapi || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (paused.test())
      return std::unexpected(BackendError::AlreadyPaused);
    if (!sb_sapi_pause(sapi))
      return std::unexpected(BackendError::InternalBackendError);
    paused.test_and_set();
    return {};
  }

  BackendResult<> resume() override {
    if (!sapi || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (!paused.test())
      return std::unexpected(BackendError::NotPaused);
    if (!sb_sapi_resume(sapi))
      return std::unexpected(BackendError::InternalBackendError);
    paused.clear();
    return {};
  }

  BackendResult<> set_volume(float volume) override {
    if (!sapi || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (volume < 0.0f || volume > 1.0f)
      return std::unexpected(BackendError::RangeOutOfBounds);
    const auto val = std::round(
        range_convert_midpoint(volume, 0.0, 0.5, 1.0, -10.0, 0.0, 10.0));
    if (val < -10.0f || val > 10.0f)
      return std::unexpected(BackendError::RangeOutOfBounds);
    if (!sb_sapi_set_volume(sapi, static_cast<int>(val)))
      return std::unexpected(BackendError::InternalBackendError);
    return {};
  }

  BackendResult<float> get_volume() override {
    if (!sapi || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    const auto val = sb_sapi_get_volume(sapi);
    return std::round(range_convert_midpoint(val, -10, 0, 10, 0.0, 0.5, 1.0));
  }

  BackendResult<> set_rate(float rate) override {
    if (!sapi || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (rate < 0.0f || rate > 1.0f)
      return std::unexpected(BackendError::RangeOutOfBounds);
    const auto val = std::round(
        range_convert_midpoint(rate, 0.0, 0.5, 1.0, -10.0, 0.0, 10.0));
    if (val < -10.0f || val > 10.0f)
      return std::unexpected(BackendError::RangeOutOfBounds);
    if (!sb_sapi_set_rate(sapi, static_cast<int>(val)))
      return std::unexpected(BackendError::InternalBackendError);
    return {};
  }

  BackendResult<float> get_rate() override {
    if (!sapi || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    const auto val = sb_sapi_get_rate(sapi);
    return std::round(range_convert_midpoint(val, -10, 0, 10, 0.0, 0.5, 1.0));
  }

  BackendResult<> set_pitch(float pitch) override {
    if (!sapi || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (pitch < 0.0f || pitch > 1.0f)
      return std::unexpected(BackendError::RangeOutOfBounds);
    const auto val = std::round(
        range_convert_midpoint(pitch, 0.0, 0.5, 1.0, -10.0, 0.0, 10.0));
    if (val < -10.0f || val > 10.0f)
      return std::unexpected(BackendError::RangeOutOfBounds);
    if (!sb_sapi_set_pitch(sapi, static_cast<int>(val)))
      return std::unexpected(BackendError::InternalBackendError);
    return {};
  }

  BackendResult<float> get_pitch() override {
    if (!sapi || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    const auto val = sb_sapi_get_pitch(sapi);
    return std::round(range_convert_midpoint(val, -10, 0, 10, 0.0, 0.5, 1.0));
  }

  BackendResult<> refresh_voices() override {
    if (!sapi || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (!sb_sapi_refresh_voices(sapi))
      return std::unexpected(BackendError::InternalBackendError);
    return {};
  }

  BackendResult<std::size_t> count_voices() override {
    if (!sapi || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    return sb_sapi_count_voices(sapi);
  }

  BackendResult<std::string> get_voice_name(std::size_t id) override {
    if (!sapi || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (id >= std::numeric_limits<int>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    if (const auto ret = sb_sapi_get_voice_name(sapi, static_cast<int>(id));
        !ret)
      return std::unexpected(BackendError::VoiceNotFound);
    else {
      const auto str = std::string(ret);
      return ret;
    }
    return "";
  }

  BackendResult<std::string> get_voice_language(std::size_t id) override {
    if (!sapi || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (id >= std::numeric_limits<int>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    if (const auto ret = sb_sapi_get_voice_language(sapi, static_cast<int>(id));
        !ret)
      return std::unexpected(BackendError::VoiceNotFound);
    else {
      const auto str = std::string(ret);
      return ret;
    }
    return "";
  }

  BackendResult<> set_voice(std::size_t id) override {
    if (!sapi || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (id >= std::numeric_limits<int>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    if (!sb_sapi_set_voice(sapi, static_cast<int>(id)))
      return std::unexpected(BackendError::VoiceNotFound);
    return {};
  }

  BackendResult<std::size_t> get_voice() override {
    if (!sapi || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    return sb_sapi_get_voice(sapi);
  }
};

REGISTER_BACKEND_WITH_ID(SapiBackend, Backends::SAPI, "SAPI", 100);
#endif
