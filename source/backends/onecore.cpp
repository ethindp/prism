// SPDX-License-Identifier: MPL-2.0

#ifdef _WIN32
#include "../backend.h"
#include "../backend_catalog.h"
#include "../logging.h"
#include "../utils.h"
#include <atomic>
#include <cmath>
#include <concepts>
#include <dr_wav/dr_wav.h>
#include <exception>
#include <fmt/format.h>
#include <fmt/xchar.h>
#include <limits>
#include <objbase.h>
#include <optional>
#include <simdutf/simdutf.h>
#include <span>
#include <tchar.h>
#include <type_traits>
#include <utility>
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

[[nodiscard]] static std::string hstring_to_utf8(std::wstring_view w) {
  const auto *src = reinterpret_cast<const char16_t *>(w.data());
  std::string out(simdutf::utf8_length_from_utf16le(src, w.size()), '\0');
  (void)simdutf::convert_utf16le_to_utf8(src, w.size(), out.data());
  return out;
}

template <>
struct fmt::formatter<winrt::hstring, char>
    : fmt::formatter<std::string_view, char> {
  template <typename FormatContext>
  auto format(const winrt::hstring &h, FormatContext &ctx) const {
    const std::string utf8 = hstring_to_utf8(std::wstring_view{h});
    return fmt::formatter<std::string_view, char>::format(utf8, ctx);
  }
};

struct MtaEventGuard {
  HANDLE h;
  MtaEventGuard() : h(CreateEvent(nullptr, TRUE, FALSE, nullptr)) {}
  ~MtaEventGuard() noexcept {
    if (h != nullptr) {
      CloseHandle(h);
      h = nullptr;
    }
  }
  MtaEventGuard(const MtaEventGuard &) = delete;
  MtaEventGuard &operator=(const MtaEventGuard &) = delete;
  explicit operator bool() const noexcept { return h != nullptr; }
};

template <typename F>
  requires std::invocable<F>
struct MtaContext {
  using R = std::invoke_result_t<F>;
  std::remove_reference_t<F> *fn;
  [[maybe_unused]] std::conditional_t<std::is_void_v<R>, char, std::optional<R>>
      result{};
  std::exception_ptr ex;
  HANDLE event;
};

inline bool is_current_thread_sta() noexcept {
  APTTYPE type{};
  APTTYPEQUALIFIER qual{};
  HRESULT hr = CoGetApartmentType(&type, &qual);
  if (FAILED(hr))
    return false;
  return type == APTTYPE_STA || type == APTTYPE_MAINSTA;
}

template <std::invocable F> auto run_on_mta(F &&fn) -> std::invoke_result_t<F> {
  using R = std::invoke_result_t<F>;
  if (!is_current_thread_sta()) {
    if constexpr (std::is_void_v<R>) {
      fn();
      return;
    } else
      return fn();
  }
  MtaEventGuard evt;
  if (!evt)
    throw std::runtime_error("run_on_mta: CreateEvent failed");
  MtaContext<F> ctx{.fn = &fn, .result = {}, .ex = nullptr, .event = evt.h};
  auto const ok = TrySubmitThreadpoolCallback(
      [](PTP_CALLBACK_INSTANCE, void *raw) noexcept {
        auto &c = *static_cast<MtaContext<F> *>(raw);
        try {
          if constexpr (std::is_void_v<R>)
            (*c.fn)();
          else
            c.result.emplace((*c.fn)());
        } catch (...) {
          c.ex = std::current_exception();
        }
        SetEvent(c.event);
      },
      &ctx, nullptr);
  if (!ok)
    throw std::runtime_error("run_on_mta: TrySubmitThreadpoolCallback failed");
  WaitForSingleObject(evt.h, INFINITE);
  if (ctx.ex)
    std::rethrow_exception(ctx.ex);
  if constexpr (!std::is_void_v<R>)
    return std::move(*ctx.result);
}

class OneCoreBackend final : public TextToSpeechBackend {
private:
  SpeechSynthesizer synth{nullptr};
  MediaPlayer player{nullptr};
  std::atomic<MediaPlaybackState> current_state{MediaPlaybackState::None};
  winrt::event_token state_changed_token{};
  std::size_t cached_channels = 0;
  std::size_t cached_sample_rate = 0;
  std::size_t cached_bit_depth = 0;
  bool format_cached = false;
  LogSource logger{"OneCore"};

public:
  ~OneCoreBackend() override {
    if (player && state_changed_token) {
      player.PlaybackSession().PlaybackStateChanged(state_changed_token);
      state_changed_token = {};
    }
    synth = nullptr;
    player = nullptr;
  }

  [[nodiscard]] std::string_view get_name() const override { return "OneCore"; }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    if (ApiInformation::IsTypePresent(
            _T("Windows.Media.SpeechSynthesis.SpeechSynthesizer")) &&
        ApiInformation::IsTypePresent(
            _T("Windows.Media.Playback.MediaPlayer"))) {
      features |= IS_SUPPORTED_AT_RUNTIME;
    }
    features |= SUPPORTS_SPEAK | SUPPORTS_SPEAK_TO_MEMORY | SUPPORTS_OUTPUT |
                SUPPORTS_IS_SPEAKING | SUPPORTS_STOP | SUPPORTS_PAUSE |
                SUPPORTS_RESUME | SUPPORTS_SET_VOLUME | SUPPORTS_GET_VOLUME |
                SUPPORTS_SET_RATE | SUPPORTS_GET_RATE | SUPPORTS_SET_PITCH |
                SUPPORTS_GET_PITCH | SUPPORTS_REFRESH_VOICES |
                SUPPORTS_COUNT_VOICES | SUPPORTS_GET_VOICE_NAME |
                SUPPORTS_GET_VOICE_LANGUAGE | SUPPORTS_GET_VOICE |
                SUPPORTS_SET_VOICE | SUPPORTS_GET_CHANNELS |
                SUPPORTS_GET_SAMPLE_RATE | SUPPORTS_GET_BIT_DEPTH |
                PERFORMS_SILENCE_TRIMMING_ON_SPEAK_TO_MEMORY;
    return features;
  }

  BackendResult<> initialize() override {
    try {
      if (!ApiInformation::IsTypePresent(
              _T("Windows.Media.SpeechSynthesis.SpeechSynthesizer")) ||
          !ApiInformation::IsTypePresent(
              _T("Windows.Media.Playback.MediaPlayer")))
        return std::unexpected(BackendError::BackendNotAvailable);
      if (synth || player)
        return std::unexpected(BackendError::AlreadyInitialized);
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
    } catch (const winrt::hresult_error &e) {
      logger.error("Could not initialize OneCore backend: {}", e.message());
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    try {
      if (interrupt)
        if (const auto res = stop(); !res)
          return res;
      const auto wtext = to_hstring(text);
      const auto stream = run_on_mta(
          [&] { return synth.SynthesizeTextToStreamAsync(wtext).get(); });
      const auto source =
          MediaSource::CreateFromStream(stream, stream.ContentType());
      player.Source(source);
      player.Play();
      current_state = MediaPlaybackState::Playing;
      return {};
    } catch (const winrt::hresult_error &e) {
      logger.error("Could not speak text of size {}: {}", text.size(),
                   e.message());
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<> speak_to_memory(std::string_view text, AudioCallback callback,
                                  void *userdata) override {
    if (!synth)
      return std::unexpected(BackendError::NotInitialized);
    try {
      const auto wtext = to_hstring(text);
      const auto stream = run_on_mta(
          [&] { return synth.SynthesizeTextToStreamAsync(wtext).get(); });
      if (stream.ContentType() != _T("audio/wav"))
        return std::unexpected(BackendError::NotImplemented);
      const auto size64 = stream.Size();
      if (size64 > std::numeric_limits<uint32_t>::max())
        return std::unexpected(BackendError::RangeOutOfBounds);
      const auto cap = static_cast<uint32_t>(size64);
      Buffer buffer(cap);
      stream.Seek(0);
      std::uint32_t total = 0;
      while (total < cap) {
        Buffer chunk(cap - total);
        run_on_mta([&] {
          stream.ReadAsync(chunk, cap - total, InputStreamOptions::None).get();
        });
        const uint32_t got = chunk.Length();
        if (got == 0)
          break;
        std::memcpy(buffer.data() + total, chunk.data(), got);
        total += got;
      }
      if (total == 0)
        return std::unexpected(BackendError::InternalBackendError);
      drwav wav{};
      if (drwav_init_memory(&wav, buffer.data(), total, nullptr) == 0)
        return std::unexpected(BackendError::InvalidAudioFormat);
      auto frame_count = wav.totalPCMFrameCount;
      std::vector<float> samples(frame_count * wav.channels);
      drwav_read_pcm_frames_f32(&wav, frame_count, samples.data());
      auto const trimmed_samples =
          trim_silence_rms_gate(samples, wav.channels, wav.sampleRate);
      callback(userdata, trimmed_samples.data(), trimmed_samples.size(),
               wav.channels, wav.sampleRate);
      drwav_uninit(&wav);
      return {};
    } catch (const std::exception &e) {
      logger.error(
          "speak_to_memory failed  with text of size {} and userdata {}: {}",
          text.size(), static_cast<const void *>(userdata), e.what());
      return std::unexpected(BackendError::Unknown);
    } catch (...) {
      logger.error(
          "speak_to_memory failed  with text of size {} and userdata {}: "
          "unknown non-std exception",
          text.size(), static_cast<const void *>(userdata));
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
      logger.error("is_speaking failed: {}", e.message());
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
      } catch (const winrt::hresult_error &e) {
        logger.error("stop failed: {}", e.message());
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
    } catch (const winrt::hresult_error &e) {
      logger.error("pause failed: {}", e.message());
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
      logger.error("resume failed: {}", e.message());
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<> set_volume(float volume) override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    try {
      synth.Options().AudioVolume(volume);
      return {};
    } catch (const winrt::hresult_error &e) {
      logger.error("set_volume failed: {}", e.message());
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<float> get_volume() override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    try {
      return static_cast<float>(synth.Options().AudioVolume());
    } catch (const winrt::hresult_error &e) {
      logger.error("get_volume failed: {}", e.message());
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<> set_rate(float rate) override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    try {
      const auto val = exp_range_convert(rate, 0.5, 1.0, 6.0);
      synth.Options().SpeakingRate(val);
      return {};
    } catch (const winrt::hresult_error &e) {
      logger.error("set_rate failed: {}", e.message());
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<float> get_rate() override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    try {
      return exp_range_convert_inv(synth.Options().SpeakingRate(), 0.5, 1.0,
                                   6.0);
    } catch (const winrt::hresult_error &e) {
      logger.error("get_rate failed: {}", e.message());
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<> set_pitch(float pitch) override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    try {
      const auto val = exp_range_convert(pitch, 0.0, 1.0, 2.0);
      synth.Options().AudioPitch(val);
      return {};
    } catch (const winrt::hresult_error &e) {
      logger.error("set_pitch failed: {}", e.message());
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<float> get_pitch() override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    try {
      return exp_range_convert_inv(synth.Options().AudioPitch(), 0.0, 1.0, 2.0);
    } catch (const winrt::hresult_error &e) {
      logger.error("get_pitch failed: {}", e.message());
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
    return SpeechSynthesizer::AllVoices().Size();
  }

  BackendResult<std::string> get_voice_name(std::size_t id) override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    if (id >= std::numeric_limits<std::uint32_t>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    const auto voices = SpeechSynthesizer::AllVoices();
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
    const auto voices = SpeechSynthesizer::AllVoices();
    if (id >= voices.Size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    return to_string(voices.GetAt(static_cast<std::uint32_t>(id)).Language());
  }

  BackendResult<> set_voice(std::size_t id) override {
    if (!synth || !player)
      return std::unexpected(BackendError::NotInitialized);
    if (id >= std::numeric_limits<std::uint32_t>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    const auto voices = SpeechSynthesizer::AllVoices();
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
    const auto voices = SpeechSynthesizer::AllVoices();
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
    if (format_cached) {
      logger.info("cache_audio_format: audio format already cached; aborting");
      return;
    }
    try {
      const auto stream = run_on_mta(
          [&] { return synth.SynthesizeTextToStreamAsync(_T(" ")).get(); });
      if (stream.ContentType() != _T("audio/wav")) {
        logger.error(
            "cache_audio_format: unknown stream content type {}; aborting",
            stream.ContentType());
        return;
      }
      const auto size64 = stream.Size();
      if (size64 > std::numeric_limits<uint32_t>::max()) {
        logger.error("cache_audio_format: stream size of {} exceeds max size "
                     "of {}; aborting",
                     size64, std::numeric_limits<uint32_t>::max());
        return;
      }
      const auto cap = static_cast<uint32_t>(size64);
      Buffer buffer(cap);
      stream.Seek(0);
      std::uint32_t total = 0;
      while (total < cap) {
        Buffer chunk(cap - total);
        run_on_mta([&] {
          stream.ReadAsync(chunk, cap - total, InputStreamOptions::None).get();
        });
        const uint32_t got = chunk.Length();
        if (got == 0)
          break;
        std::memcpy(buffer.data() + total, chunk.data(), got);
        total += got;
      }
      if (total == 0) {
        logger.error("cache_audio_format: synthesis audio stream has no "
                     "samples; aborting");
        return;
      }
      drwav wav{};
      if (const auto res =
              drwav_init_memory(&wav, buffer.data(), total, nullptr);
          res != 0) {
        cached_channels = wav.channels;
        cached_sample_rate = wav.sampleRate;
        cached_bit_depth = wav.bitsPerSample;
        format_cached = true;
        drwav_uninit(&wav);
      } else {
        logger.error("cache_audio_format: WAV parse error: code {}", res);
        return;
      }
    } catch (const std::exception &e) {
      logger.error("cache_audio_format failed:  {}", e.what());
      return;
    } catch (...) {
      logger.error("cache_audio_format failed: unknown non-std exception");
      return;
    }
  }
};

REGISTER_BACKEND_WITH_ID(OneCoreBackend, Backends::OneCore, "OneCore", 98);
#endif
