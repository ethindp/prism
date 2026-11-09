// SPDX-License-Identifier: MPL-2.0

#include "../backend.h"
#include "../backend_catalog.h"
#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <simdutf.h>
#include <string>
#include <string_view>

static constexpr std::size_t no_speech_sample_rate = 22050;
static constexpr std::size_t no_speech_frames_per_code_point = 1323;
static constexpr std::size_t no_speech_chunk_frames = 4096;
static const std::array<float, no_speech_chunk_frames> no_speech_silence{};

class NoSpeechBackend final : public TextToSpeechBackend {
private:
  using Clock = std::chrono::steady_clock;

  bool initialized = false;
  bool paused = false;
  Clock::time_point end;
  Clock::duration remaining{};
  float volume = 0.5F;
  float rate = 0.5F;
  float pitch = 0.5F;

  [[nodiscard]] std::size_t frames_for(std::string_view text) const {
    const double speed = rate <= 0.5F
                             ? 0.5 + rate
                             : 1.0 + ((static_cast<double>(rate) - 0.5) * 4.0);
    const auto code_points = simdutf::count_utf8(text.data(), text.size());
    return static_cast<std::size_t>(std::llround(
        static_cast<double>(code_points * no_speech_frames_per_code_point) /
        speed));
  }

  void enqueue(std::string_view text, bool interrupt) {
    const auto now = Clock::now();
    const auto length = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(static_cast<double>(frames_for(text)) /
                                      no_speech_sample_rate));
    if (interrupt) {
      paused = false;
      remaining = {};
      end = now + length;
    } else if (paused) {
      remaining += length;
    } else {
      end = std::max(end, now) + length;
    }
  }

public:
  static constexpr bool requires_explicit_selection = true;
  NoSpeechBackend() = default;
  NoSpeechBackend(const NoSpeechBackend &) = delete;
  NoSpeechBackend &operator=(const NoSpeechBackend &) = delete;
  NoSpeechBackend(NoSpeechBackend &&) = delete;
  NoSpeechBackend &operator=(NoSpeechBackend &&) = delete;
  ~NoSpeechBackend() override = default;

  [[nodiscard]] std::string_view get_name() const override {
    return "No Speech";
  }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    return IS_SUPPORTED_AT_RUNTIME | SUPPORTS_SPEAK | SUPPORTS_SPEAK_TO_MEMORY |
           SUPPORTS_BRAILLE | SUPPORTS_OUTPUT | SUPPORTS_IS_SPEAKING |
           SUPPORTS_STOP | SUPPORTS_PAUSE | SUPPORTS_RESUME |
           SUPPORTS_SET_VOLUME | SUPPORTS_GET_VOLUME | SUPPORTS_SET_RATE |
           SUPPORTS_GET_RATE | SUPPORTS_SET_PITCH | SUPPORTS_GET_PITCH |
           SUPPORTS_REFRESH_VOICES | SUPPORTS_COUNT_VOICES |
           SUPPORTS_GET_VOICE_NAME | SUPPORTS_GET_VOICE_LANGUAGE |
           SUPPORTS_GET_VOICE | SUPPORTS_SET_VOICE | SUPPORTS_GET_CHANNELS |
           SUPPORTS_GET_SAMPLE_RATE | SUPPORTS_GET_BIT_DEPTH |
           REQUIRES_EXPLICIT_SELECTION;
  }

  BackendResult<> initialize() override {
    if (initialized)
      return std::unexpected(BackendError::AlreadyInitialized);
    initialized = true;
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    enqueue(text, interrupt);
    return {};
  }

  BackendResult<> speak_to_memory(std::string_view text, AudioCallback callback,
                                  void *userdata) override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    for (auto frames = frames_for(text); frames > 0;) {
      const auto count = std::min(frames, no_speech_chunk_frames);
      callback(userdata, no_speech_silence.data(), count, 1,
               no_speech_sample_rate);
      frames -= count;
    }
    return {};
  }

  BackendResult<> braille([[maybe_unused]] std::string_view text) override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<bool> is_speaking() override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    return !paused && Clock::now() < end;
  }

  BackendResult<> stop() override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    paused = false;
    remaining = {};
    end = {};
    return {};
  }

  BackendResult<> pause() override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    if (paused)
      return std::unexpected(BackendError::AlreadyPaused);
    const auto now = Clock::now();
    if (now >= end)
      return std::unexpected(BackendError::NotSpeaking);
    remaining = end - now;
    paused = true;
    return {};
  }

  BackendResult<> resume() override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    if (!paused)
      return std::unexpected(BackendError::NotPaused);
    end = Clock::now() + remaining;
    remaining = {};
    paused = false;
    return {};
  }

  BackendResult<> set_volume(float value) override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    volume = value;
    return {};
  }

  BackendResult<float> get_volume() override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    return volume;
  }

  BackendResult<> set_rate(float value) override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    rate = value;
    return {};
  }

  BackendResult<float> get_rate() override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    return rate;
  }

  BackendResult<> set_pitch(float value) override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    pitch = value;
    return {};
  }

  BackendResult<float> get_pitch() override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    return pitch;
  }

  BackendResult<> refresh_voices() override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    return {};
  }

  BackendResult<std::size_t> count_voices() override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    return 1;
  }

  BackendResult<std::string> get_voice_name(std::size_t id) override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    if (id != 0)
      return std::unexpected(BackendError::RangeOutOfBounds);
    return "No Speech";
  }

  BackendResult<std::string> get_voice_language(std::size_t id) override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    if (id != 0)
      return std::unexpected(BackendError::RangeOutOfBounds);
    return "und";
  }

  BackendResult<> set_voice(std::size_t id) override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    if (id != 0)
      return std::unexpected(BackendError::RangeOutOfBounds);
    return {};
  }

  BackendResult<std::size_t> get_voice() override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    return 0;
  }

  BackendResult<std::size_t> get_channels() override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    return 1;
  }

  BackendResult<std::size_t> get_sample_rate() override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    return no_speech_sample_rate;
  }

  BackendResult<std::size_t> get_bit_depth() override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    return 16;
  }
};

REGISTER_BACKEND_WITH_ID(NoSpeechBackend, Backends::NoSpeech, "No Speech", 0);
