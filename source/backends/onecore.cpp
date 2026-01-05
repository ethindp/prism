// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#include "utils.h"
#include <atomic>
#include <cmath>
#include <limits>
#ifdef _WIN32
#include <windows.h>
#include <winrt/Windows.Foundation.Collections.h>
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

class OneCoreBackend final : public TextToSpeechBackend {
private:
  SpeechSynthesizer synth{nullptr};
  MediaPlayer player{nullptr};
  std::atomic<MediaPlaybackState> current_state{MediaPlaybackState::None};
  winrt::event_token state_changed_token{};

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
    try {
      synth = SpeechSynthesizer();
      player = MediaPlayer();
      state_changed_token = player.PlaybackSession().PlaybackStateChanged(
          [this](MediaPlaybackSession const &session, auto const &) {
            current_state = session.PlaybackState();
          });
      return {};
    } catch (const winrt::hresult_error &e) {
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
    } catch (const winrt::hresult_error &e) {
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
    } catch (const winrt::hresult_error &e) {
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<> stop() override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    const auto state = current_state.load();
    if (state != MediaPlaybackState::Playing &&
        state != MediaPlaybackState::Paused)
      return std::unexpected(BackendError::NotSpeaking);
    try {
      player.Pause();
      player.Source(nullptr);
      current_state = MediaPlaybackState::None;
      return {};
    } catch (const winrt::hresult_error &e) {
      return std::unexpected(BackendError::Unknown);
    }
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
    } catch (const winrt::hresult_error &e) {
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
    } catch (const winrt::hresult_error &e) {
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
    } catch (const winrt::hresult_error &e) {
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<float> get_volume() override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    try {
      return synth.Options().AudioVolume();
    } catch (const winrt::hresult_error &e) {
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
    } catch (const winrt::hresult_error &e) {
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<float> get_rate() override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    try {
      return range_convert_midpoint(synth.Options().SpeakingRate(), 0.5, 3.0,
                                    6.0, 0.0, 0.5, 1.0);
    } catch (const winrt::hresult_error &e) {
      return std::unexpected(BackendError::Unknown);
    }
  }
};

REGISTER_BACKEND_WITH_ID(OneCoreBackend, Backends::OneCore, "OneCore", 99);
#endif