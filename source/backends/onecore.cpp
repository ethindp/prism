// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#include "dr_wav.h"
#include "utils.h"
#include <atomic>
#include <cmath>
#include <limits>
#ifdef _WIN32
#include <tchar.h>
#include <windows.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Core.h>
#include <winrt/Windows.Media.Playback.h>
#include <winrt/Windows.Media.SpeechSynthesis.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/base.h>

using namespace winrt;
using namespace Windows::Media::SpeechSynthesis;
using namespace Windows::Storage::Streams;
using namespace Windows::Media::Core;
using namespace Windows::Media::Playback;
using namespace winrt::Windows::Foundation::Metadata;

class OneCoreBackend final : public TextToSpeechBackend {
private:
  SpeechSynthesizer synth{nullptr};
  MediaPlayer player{nullptr};
  std::atomic<MediaPlaybackState> current_state{MediaPlaybackState::None};
  winrt::event_token state_changed_token{};
  std::size_t cached_channels;
  std::size_t cached_sample_rate;
  std::size_t cached_bit_depth;
  bool format_cached = false;

public:
  ~OneCoreBackend() override {
    if (player && state_changed_token) {
      player.PlaybackSession().PlaybackStateChanged(state_changed_token);
    }
    synth = nullptr;
    player = nullptr;
  }

  std::string_view get_name() const override { return "OneCore"; }

  BackendResult<> initialize() override {
    if (!ApiInformation::IsTypePresent(
            _T("Windows.Media.SpeechSynthesis.SpeechSynthesizer")) ||
        !ApiInformation::IsTypePresent(
            _T("Windows.Media.Playback.MediaPlayer")))
      return std::unexpected(BackendError::BackendNotAvailable);
    try {
      synth = SpeechSynthesizer();
      synth.Options().AppendedSilence(SpeechAppendedSilence::Min);
      synth.Options().PunctuationSilence(SpeechPunctuationSilence::Min);
      player = MediaPlayer();
      state_changed_token = player.PlaybackSession().PlaybackStateChanged(
          [this](MediaPlaybackSession const &session, auto const &) {
            current_state = session.PlaybackState();
          });
      cache_audio_format();
      return {};
    } catch (const winrt::hresult_error &) {
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    if (!simdutf::validate_utf8(text.data(), text.size())) {
      return std::unexpected(BackendError::InvalidUtf8);
    }
    try {
      if (interrupt)
        if (const auto res = stop();
            !res && res.error() != BackendError::NotSpeaking)
          return res;
      const auto wtext = to_hstring(text);
      const auto stream = synth.SynthesizeTextToStreamAsync(wtext).get();
      const auto source =
          MediaSource::CreateFromStream(stream, stream.ContentType());
      player.Source(source);
      player.Play();
      current_state = MediaPlaybackState::Playing;
      return {};
    } catch (const winrt::hresult_error &) {
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<> speak_to_memory(std::string_view text, AudioCallback callback,
                                  void *userdata) override {
    if (!synth)
      return std::unexpected(BackendError::NotInitialized);
    if (!simdutf::validate_utf8(text.data(), text.size()))
      return std::unexpected(BackendError::InvalidUtf8);
    try {
      const auto wtext = to_hstring(text);
      const auto stream = synth.SynthesizeTextToStreamAsync(wtext).get();
      if (stream.ContentType() != L"audio/wav")
        return std::unexpected(BackendError::NotImplemented);
      const auto size = static_cast<uint32_t>(stream.Size());
      Buffer buffer(size);
      stream.ReadAsync(buffer, size, InputStreamOptions::None).get();
      drwav wav;
      if (!drwav_init_memory(&wav, buffer.data(), size, nullptr))
        return std::unexpected(BackendError::InternalBackendError);
      auto frame_count = wav.totalPCMFrameCount;
      std::vector<float> samples(frame_count * wav.channels);
      drwav_read_pcm_frames_f32(&wav, frame_count, samples.data());
      callback(userdata, samples.data(), samples.size(), wav.channels,
               wav.sampleRate);
      drwav_uninit(&wav);
      return {};
    } catch (...) {
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<bool> is_speaking() override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    try {
      return current_state == MediaPlaybackState::Playing;
    } catch (const winrt::hresult_error &) {
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<> stop() override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    const auto state = current_state.load();
    if (state == MediaPlaybackState::Playing ||
        state == MediaPlaybackState::Paused) {
      try {
        player.Pause();
        player.Source(nullptr);
        current_state = MediaPlaybackState::None;
      } catch (const winrt::hresult_error &) {
        return std::unexpected(BackendError::Unknown);
      }
    }
    return {};
  }

  BackendResult<> pause() override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    const auto state = current_state.load();
    if (state == MediaPlaybackState::Paused)
      return std::unexpected(BackendError::AlreadyPaused);
    if (state != MediaPlaybackState::Playing)
      return std::unexpected(BackendError::NotSpeaking);
    try {
      player.Pause();
      current_state = MediaPlaybackState::Paused;
      return {};
    } catch (const winrt::hresult_error &) {
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<> resume() override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    const auto state = current_state.load();
    if (state != MediaPlaybackState::Paused)
      return std::unexpected(BackendError::NotPaused);
    try {
      player.Play();
      current_state = MediaPlaybackState::Playing;
      return {};
    } catch (const winrt::hresult_error &) {
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<> set_volume(float volume) override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    try {
      if (volume < 0.0f || volume > 1.0f)
        return std::unexpected(BackendError::RangeOutOfBounds);
      synth.Options().AudioVolume(volume);
      return {};
    } catch (const winrt::hresult_error &) {
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<float> get_volume() override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    try {
      return static_cast<float>(synth.Options().AudioVolume());
    } catch (const winrt::hresult_error &) {
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<> set_rate(float rate) override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    try {
      // This is really weird because Microsofts implementation of this is
      // weird. SpeakingRate is a multiplier and not the (actual) absolute rate.
      // So, we do this for now. Fix this later.
      if (rate < 0.0 || rate > 1.0)
        return std::unexpected(BackendError::RangeOutOfBounds);
      const auto val =
          range_convert_midpoint(rate, 0.0, 0.5, 1.0, 0.5, 3.0, 6.0);
      synth.Options().SpeakingRate(val);
      return {};
    } catch (const winrt::hresult_error &) {
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<float> get_rate() override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    try {
      return range_convert_midpoint(
          static_cast<float>(synth.Options().SpeakingRate()), 0.5, 3.0, 6.0,
          0.0, 0.5, 1.0);
    } catch (const winrt::hresult_error &) {
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<> refresh_voices() override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);

    return {};
  }

  BackendResult<std::size_t> count_voices() override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    return synth.AllVoices().Size();
  }

  BackendResult<std::string> get_voice_name(std::size_t id) override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    if (id >= std::numeric_limits<std::uint32_t>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    const auto voices = synth.AllVoices();
    if (id >= voices.Size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    return to_string(
        voices.GetAt(static_cast<std::uint32_t>(id)).DisplayName());
  }

  BackendResult<std::string> get_voice_language(std::size_t id) override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    if (id >= std::numeric_limits<std::uint32_t>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    const auto voices = synth.AllVoices();
    if (id >= voices.Size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    return to_string(voices.GetAt(static_cast<std::uint32_t>(id)).Language());
  }

  BackendResult<> set_voice(std::size_t id) override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    if (id >= std::numeric_limits<std::uint32_t>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    const auto voices = synth.AllVoices();
    if (id >= voices.Size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    synth.Voice(voices.GetAt(static_cast<std::uint32_t>(id)));
    format_cached = false;
    cache_audio_format();
    return {};
  }

  BackendResult<std::size_t> get_voice() override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    const auto voices = synth.AllVoices();
    for (std::uint32_t i = 0; i < voices.Size(); ++i) {
      if (synth.Voice().Id() == voices.GetAt(i).Id())
        return i;
    }
    return std::unexpected(BackendError::InternalBackendError);
  }

  BackendResult<std::size_t> get_channels() override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    if (!format_cached)
      cache_audio_format();
    return cached_channels;
  }

  BackendResult<std::size_t> get_sample_rate() override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    if (!format_cached)
      cache_audio_format();
    return cached_sample_rate;
  }

  BackendResult<std::size_t> get_bit_depth() override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    if (!format_cached)
      cache_audio_format();
    return cached_bit_depth;
  }

  void cache_audio_format() {
    if (format_cached)
      return;
    try {
      const auto stream = synth.SynthesizeTextToStreamAsync(L" ").get();
      if (stream.ContentType() != L"audio/wav")
        return;
      const auto size = static_cast<uint32_t>(stream.Size());
      Buffer buffer(size);
      stream.ReadAsync(buffer, size, InputStreamOptions::None).get();
      drwav wav;
      if (drwav_init_memory(&wav, buffer.data(), size, nullptr)) {
        cached_channels = wav.channels;
        cached_sample_rate = wav.sampleRate;
        cached_bit_depth = wav.bitsPerSample;
        format_cached = true;
        drwav_uninit(&wav);
      }
    } catch (...) {
    return;
    }
  }
};

REGISTER_BACKEND_WITH_ID(OneCoreBackend, Backends::OneCore, "OneCore", 99);
#endif