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

  std::string_view get_name() override { return "OneCore"; }

  BackendResult<> initialize() override {
    try {
      synth = SpeechSynthesizer();
      player = MediaPlayer();
      return {};
    } catch (const winrt::hresult_error &e) {
      return BackendError::Unknown;
    }
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!synth || !player)
      return BackendError::NotInitialized;
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
      return BackendError::Unknown;
    }
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<bool> is_speaking() override {
    if (!synth || !player)
      return BackendError::NotInitialized;
    try {
      return player.PlaybackSession().PlaybackState() ==
             MediaPlaybackState::Playing;
    } catch (const winrt::hresult_error &e) {
      return BackendError::Unknown;
    }
  }
