// SPDX-License-Identifier: MPL-2.0

#ifdef __EMSCRIPTEN__
#include "../backend.h"
#include "../backend_catalog.h"
#include "../utils.h"
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <simdutf.h>
#include <string>
#include <vector>

using emscripten::val;

class WebSpeechSynthesisBackend final : public TextToSpeechBackend {
private:
  val synth{val::null()};
  val current_utterance{val::null()};
  std::vector<val> voices;
  std::size_t current_voice{0};
  float volume{1.0f};
  float rate{0.5f};
  float pitch{0.5f};

  bool paused{false};

  [[nodiscard]] bool synth_busy() const {
    return synth["speaking"].as<bool>() || synth["pending"].as<bool>();
  }

public:
  ~WebSpeechSynthesisBackend() override {
    if (!synth.isNull())
      (void)stop();
  }

  std::string_view get_name() const override { return "Web Speech Synthesis"; }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    val window = val::global("window");
    if (!window.isUndefined()) {
      val speech_synthesis = window["speechSynthesis"];
      if (!speech_synthesis.isUndefined()) {
        features |= IS_SUPPORTED_AT_RUNTIME;
      }
    }
    features |= SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_IS_SPEAKING |
                SUPPORTS_STOP | SUPPORTS_PAUSE | SUPPORTS_RESUME |
                SUPPORTS_SET_VOLUME | SUPPORTS_GET_VOLUME | SUPPORTS_SET_RATE |
                SUPPORTS_GET_RATE | SUPPORTS_SET_PITCH | SUPPORTS_GET_PITCH |
                SUPPORTS_REFRESH_VOICES | SUPPORTS_COUNT_VOICES |
                SUPPORTS_GET_VOICE_NAME | SUPPORTS_GET_VOICE_LANGUAGE |
                SUPPORTS_GET_VOICE | SUPPORTS_SET_VOICE;
    return features |= IS_SPEECH_ENGINE;
  }

  BackendResult<> initialize() override {
    if (!synth.isNull())
      return std::unexpected(BackendError::AlreadyInitialized);
    val window = val::global("window");
    if (window.isUndefined() || window.isNull())
      return std::unexpected(BackendError::BackendNotAvailable);
    if (!window["speechSynthesis"].as<bool>())
      return std::unexpected(BackendError::BackendNotAvailable);
    synth = window["speechSynthesis"];
    if (synth.isNull() || synth.isUndefined())
      return std::unexpected(BackendError::BackendNotAvailable);
    refresh_voices();
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (synth.isNull())
      return std::unexpected(BackendError::NotInitialized);
    if (interrupt) {
      if (const auto r = stop(); !r)
        return std::unexpected(r.error());
    }
    // embind binds std::string, but not std::string_view.
    val utterance =
        val::global("SpeechSynthesisUtterance").new_(std::string{text});
    if (utterance.isNull() || utterance.isUndefined())
      return std::unexpected(BackendError::InternalBackendError);
    utterance.set("volume", volume);
    utterance.set("rate", range_convert_midpoint(rate, 0.0f, 0.5f, 1.0f, 0.1f,
                                                 1.0f, 10.0f));
    utterance.set("pitch", range_convert_midpoint(pitch, 0.0f, 0.5f, 1.0f, 0.0f,
                                                  1.0f, 2.0f));
    if (!voices.empty() && current_voice < voices.size())
      utterance.set("voice", voices[current_voice]);
    // Held so the browser cannot collect the utterance while it plays.
    current_utterance = utterance;
    synth.call<void>("speak", utterance);
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<bool> is_speaking() override {
    if (synth.isNull())
      return std::unexpected(BackendError::NotInitialized);
    return synth_busy();
  }

  BackendResult<> stop() override {
    if (synth.isNull())
      return std::unexpected(BackendError::NotInitialized);
    if (paused)
      synth.call<void>("resume");
    synth.call<void>("cancel");
    paused = false;
    return {};
  }

  BackendResult<> pause() override {
    if (synth.isNull())
      return std::unexpected(BackendError::NotInitialized);
    if (paused)
      return std::unexpected(BackendError::AlreadyPaused);
    if (!synth_busy())
      return std::unexpected(BackendError::NotSpeaking);
    synth.call<void>("pause");
    paused = true;
    return {};
  }

  BackendResult<> resume() override {
    if (synth.isNull())
      return std::unexpected(BackendError::NotInitialized);
    if (!paused)
      return std::unexpected(BackendError::NotPaused);
    synth.call<void>("resume");
    paused = false;
    return {};
  }

  BackendResult<> set_volume(float v) override {
    if (synth.isNull())
      return std::unexpected(BackendError::NotInitialized);
    if (v < 0.0f || v > 1.0f)
      return std::unexpected(BackendError::RangeOutOfBounds);
    volume = v;
    return {};
  }

  BackendResult<float> get_volume() override {
    if (synth.isNull())
      return std::unexpected(BackendError::NotInitialized);
    return volume;
  }

  BackendResult<> set_rate(float r) override {
    if (synth.isNull())
      return std::unexpected(BackendError::NotInitialized);
    if (r < 0.0f || r > 1.0f)
      return std::unexpected(BackendError::RangeOutOfBounds);
    rate = r;
    return {};
  }

  BackendResult<float> get_rate() override {
    if (synth.isNull())
      return std::unexpected(BackendError::NotInitialized);
    return rate;
  }

  BackendResult<> set_pitch(float p) override {
    if (synth.isNull())
      return std::unexpected(BackendError::NotInitialized);
    if (p < 0.0f || p > 1.0f)
      return std::unexpected(BackendError::RangeOutOfBounds);
    pitch = p;
    return {};
  }

  BackendResult<float> get_pitch() override {
    if (synth.isNull())
      return std::unexpected(BackendError::NotInitialized);
    return pitch;
  }

  BackendResult<> refresh_voices() override {
    if (synth.isNull())
      return std::unexpected(BackendError::NotInitialized);
    val voiceList = synth.call<val>("getVoices");
    if (voiceList.isNull() || voiceList.isUndefined())
      return {};
    const auto count = voiceList["length"].as<std::size_t>();
    voices.clear();
    voices.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
      voices.emplace_back(voiceList[i]);
    if (current_voice >= voices.size() && !voices.empty())
      current_voice = 0;
    return {};
  }

  BackendResult<std::size_t> count_voices() override {
    if (synth.isNull())
      return std::unexpected(BackendError::NotInitialized);
    return voices.size();
  }

  BackendResult<std::string> get_voice_name(std::size_t id) override {
    if (synth.isNull())
      return std::unexpected(BackendError::NotInitialized);
    if (id >= voices.size())
      return std::unexpected(BackendError::VoiceNotFound);
    return voices[id]["name"].as<std::string>();
  }

  BackendResult<std::string> get_voice_language(std::size_t id) override {
    if (synth.isNull())
      return std::unexpected(BackendError::NotInitialized);
    if (id >= voices.size())
      return std::unexpected(BackendError::VoiceNotFound);
    return voices[id]["lang"].as<std::string>();
  }

  BackendResult<> set_voice(std::size_t id) override {
    if (synth.isNull())
      return std::unexpected(BackendError::NotInitialized);
    if (voices.empty())
      return std::unexpected(BackendError::NoVoices);
    if (id >= voices.size())
      return std::unexpected(BackendError::VoiceNotFound);
    current_voice = id;
    return {};
  }

  BackendResult<std::size_t> get_voice() override {
    if (synth.isNull())
      return std::unexpected(BackendError::NotInitialized);
    if (voices.empty())
      return std::unexpected(BackendError::NoVoices);
    return current_voice;
  }
};

REGISTER_BACKEND_WITH_ID(WebSpeechSynthesisBackend,
                         Backends::WebSpeechSynthesis, "Web Speech Synthesis",
                         99);
#endif