// SPDX-License-Identifier: MPL-2.0

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

public:
  ~OneCoreBackend() override {
    synth = nullptr;
    player = nullptr;
  }

  std::string_view get_name() const override { return "OneCore"; }

  BackendResult<> initialize() override {
    try {
      synth = SpeechSynthesizer();
      player = MediaPlayer();
      return {};
    } catch (const winrt::hresult_error &e) {
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    try {
      if (interrupt)
        stop();
      const auto wtext = to_hstring(text);
      const auto stream = synth.SynthesizeTextToStreamAsync(wtext).get();
      const auto source =
          MediaSource::CreateFromStream(stream, stream.ContentType());
      player.Source(source);
      player.Play();
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
      return player.PlaybackSession().PlaybackState() ==
             MediaPlaybackState::Playing;
    } catch (const winrt::hresult_error &e) {
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<> stop() override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    try {
      if (player.PlaybackSession().PlaybackState() ==
          MediaPlaybackState::Playing) {
        player.Pause();
        player.Source(nullptr);
        return {};
      } else {
        return std::unexpected(BackendError::NotSpeaking);
      }
    } catch (const winrt::hresult_error &e) {
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<> pause() override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    try {
      if (player.PlaybackSession().PlaybackState() ==
          MediaPlaybackState::Playing) {
        player.Pause();
        return {};
      } else if (player.PlaybackSession().PlaybackState() ==
                 MediaPlaybackState::Paused) {
        return std::unexpected(BackendError::AlreadyPaused);
      } else {
        return std::unexpected(BackendError::NotSpeaking);
      }
    } catch (const winrt::hresult_error &e) {
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<> resume() override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    try {
      if (player.PlaybackSession().PlaybackState() ==
          MediaPlaybackState::Paused) {
        player.Play();
        return {};
      } else if (player.PlaybackSession().PlaybackState() ==
                 MediaPlaybackState::Playing) {
        return std::unexpected(BackendError::InvalidOperation);
      } else {
        return std::unexpected(BackendError::NotSpeaking);
      }
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
      const auto val = synth.Options().SpeakingRate();
      synth.Options().SpeakingRate(
          range_convert_midpoint(val, 0.5, 3.0, 6.0, 0.0, 0.5, 1.0));
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