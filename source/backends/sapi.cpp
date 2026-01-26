// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#include "utils.h"
#include <algorithm>
#include <atomic>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <vector>
#ifdef _WIN32
#include <atlbase.h>
#include <sapi.h>
#include <tchar.h>

struct VoiceInfo {
  CComPtr<ISpObjectToken> token;
  std::string name;
  std::string language;
};

class SapiBackend final : public TextToSpeechBackend {
private:
  CComPtr<ISpVoice> voice;
  std::atomic_flag initialized;
  std::atomic_flag paused;
  std::atomic<LONG> pitch;
  std::vector<VoiceInfo> voices;
  std::atomic_uint64_t voice_idx;
  std::shared_mutex voices_lock;
  std::atomic<std::size_t> audio_channels;
  std::atomic<std::size_t> audio_sample_rate;
  std::atomic<std::size_t> audio_bit_depth;

public:
  ~SapiBackend() override = default;

  [[nodiscard]] std::string_view get_name() const override { return "SAPI"; }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    IClassFactory *factory = nullptr;
    HRESULT hr = CoGetClassObject(
        CLSID_SpVoice, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, nullptr,
        IID_IClassFactory, reinterpret_cast<void **>(&factory));
    if (SUCCEEDED(hr) && factory != nullptr) {
      factory->Release();
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
                SUPPORTS_GET_SAMPLE_RATE | SUPPORTS_GET_BIT_DEPTH;
    return features;
  }

  BackendResult<> initialize() override {
    if (voice != nullptr) {
      return std::unexpected(BackendError::AlreadyInitialized);
    }
    HRESULT hr = voice.CoCreateInstance(CLSID_SpVoice);
    if (FAILED(hr)) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    if (auto const res = refresh_voices(); !res)
      return res;
    CComPtr<ISpObjectToken> current_token;
    hr = voice->GetVoice(&current_token);
    if (SUCCEEDED(hr) && current_token != nullptr) {
      std::shared_lock sl(voices_lock);
      for (std::size_t i = 0; i < voices.size(); ++i) {
        if (voices[i].token.IsEqualObject(current_token)) {
          voice_idx.store(i, std::memory_order_release);
          break;
        }
      }
    } else {
      return std::unexpected(BackendError::InternalBackendError);
    }
    CComPtr<ISpStreamFormat> output_format;
    hr = voice->GetOutputStream(&output_format);
    if (FAILED(hr) || output_format == nullptr)
      return std::unexpected(BackendError::InternalBackendError);
    GUID format_id;
    WAVEFORMATEX *wfx = nullptr;
    hr = output_format->GetFormat(&format_id, &wfx);
    if (FAILED(hr) || wfx == nullptr)
      return std::unexpected(BackendError::InternalBackendError);
    audio_channels.exchange(wfx->nChannels, std::memory_order_acq_rel);
    audio_sample_rate.exchange(wfx->nSamplesPerSec, std::memory_order_acq_rel);
    audio_bit_depth.exchange(wfx->wBitsPerSample, std::memory_order_acq_rel);
    CoTaskMemFree(wfx);
    initialized.test_and_set();
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (voice == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (!simdutf::validate_utf8(text.data(), text.size())) {
      return std::unexpected(BackendError::InvalidUtf8);
    }
    if (text.size() >= std::numeric_limits<int>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    paused.clear();
    if (interrupt)
      if (FAILED(voice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr)))
        return std::unexpected(BackendError::SpeakFailure);
    std::wstring wtext(
        simdutf::utf16_length_from_utf8(text.data(), text.size()), _T('\0'));
    if (const auto res = simdutf::convert_utf8_to_utf16le(
            text.data(), text.size(),
            reinterpret_cast<char16_t *>(wtext.data()));
        res == 0)
      return std::unexpected(BackendError::InvalidUtf8);
    DWORD flags = SPF_ASYNC;
    if (const auto current_pitch = pitch.load(std::memory_order_acquire);
        current_pitch != 0) {
      wtext =
          std::format(_T("<pitch absmiddle=\"{}\"/>{}"), current_pitch, wtext);
      flags |= SPF_IS_XML;
    }
    if (FAILED(voice->Speak(wtext.c_str(), flags, nullptr)))
      return std::unexpected(BackendError::SpeakFailure);
    return {};
  }

  BackendResult<> speak_to_memory(std::string_view text, AudioCallback callback,
                                  void *userdata) override {
    if (voice == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (!simdutf::validate_utf8(text.data(), text.size())) {
      return std::unexpected(BackendError::InvalidUtf8);
    }
    if (text.size() >= std::numeric_limits<int>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    paused.clear();
    std::wstring wtext(
        simdutf::utf16_length_from_utf8(text.data(), text.size()), _T('\0'));
    (void)simdutf::convert_utf8_to_utf16le(
        text.data(), text.size(),
        reinterpret_cast<char16_t *>(
            wtext.data())); // Deliberately ignored return value
    CComPtr<ISpStream> stream;
    CComPtr<IStream> base_stream;
    HRESULT hr = CreateStreamOnHGlobal(nullptr, TRUE, &base_stream);
    if (FAILED(hr))
      return std::unexpected(BackendError::InternalBackendError);
    CComPtr<ISpStreamFormat> output_format;
    hr = voice->GetOutputStream(&output_format);
    if (FAILED(hr))
      return std::unexpected(BackendError::InternalBackendError);
    GUID format_id;
    WAVEFORMATEX *wfx = nullptr;
    hr = output_format->GetFormat(&format_id, &wfx);
    if (FAILED(hr))
      return std::unexpected(BackendError::InternalBackendError);
    hr = stream.CoCreateInstance(CLSID_SpStream);
    if (FAILED(hr)) {
      CoTaskMemFree(wfx);
      return std::unexpected(BackendError::InternalBackendError);
    }
    hr = stream->SetBaseStream(base_stream, format_id, wfx);
    if (FAILED(hr)) {
      CoTaskMemFree(wfx);
      return std::unexpected(BackendError::InternalBackendError);
    }
    auto const channels = wfx->nChannels;
    auto const sample_rate = wfx->nSamplesPerSec;
    auto const bit_depth = wfx->wBitsPerSample;
    CoTaskMemFree(wfx);
    hr = voice->SetOutput(stream, TRUE);
    if (FAILED(hr))
      return std::unexpected(BackendError::InternalBackendError);
    DWORD flags = SPF_DEFAULT;
    if (const auto current_pitch = pitch.load(std::memory_order_acquire);
        current_pitch != 0) {
      wtext =
          std::format(_T("<pitch absmiddle=\"{}\"/>{}"), current_pitch, wtext);
      flags |= SPF_IS_XML;
    }
    if (FAILED(voice->Speak(wtext.c_str(), flags, nullptr)))
      return std::unexpected(BackendError::SpeakFailure);
    hr = voice->SetOutput(nullptr, TRUE);
    if (FAILED(hr))
      return std::unexpected(BackendError::InternalBackendError);
    LARGE_INTEGER zero = {};
    ULARGE_INTEGER size;
    stream->Seek(zero, STREAM_SEEK_END, &size);
    stream->Seek(zero, STREAM_SEEK_SET, nullptr);
    if (size.QuadPart == 0)
      return std::unexpected(BackendError::InternalBackendError);
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(size.QuadPart));
    ULONG bytes_read = 0;
    hr = stream->Read(buffer.data(), static_cast<ULONG>(buffer.size()),
                      &bytes_read);
    if (FAILED(hr) || bytes_read != buffer.size())
      return std::unexpected(BackendError::InternalBackendError);
    std::size_t sample_count = size.QuadPart / (bit_depth / 8);
    std::vector<float> samples(sample_count);
    if (bit_depth == 16) {
      const auto *src = reinterpret_cast<const int16_t *>(buffer.data());
      for (std::size_t i = 0; i < sample_count; ++i) {
        samples[i] = static_cast<float>(src[i]) / 32768.0F;
      }
    } else if (bit_depth == 8) {
      const auto *src = static_cast<const uint8_t *>(buffer.data());
      for (std::size_t i = 0; i < sample_count; ++i) {
        samples[i] = static_cast<float>(src[i] - 128) / 128.0F;
      }
    } else {
      return std::unexpected(BackendError::InternalBackendError);
    }
    callback(userdata, samples.data(), sample_count, channels, sample_rate);
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<bool> is_speaking() override {
    if (voice == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    SPVOICESTATUS status;
    if (FAILED(voice->GetStatus(&status, nullptr)))
      return std::unexpected(BackendError::InternalBackendError);
    return status.dwRunningState == SPRS_IS_SPEAKING;
  }

  BackendResult<> stop() override {
    if (voice == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (FAILED(voice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr)))
      return std::unexpected(BackendError::SpeakFailure);
    return {};
  }

  BackendResult<> pause() override {
    if (voice == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (paused.test())
      return std::unexpected(BackendError::AlreadyPaused);
    if (FAILED(voice->Pause()))
      return std::unexpected(BackendError::InternalBackendError);
    paused.test_and_set();
    return {};
  }

  BackendResult<> resume() override {
    if (voice == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (!paused.test())
      return std::unexpected(BackendError::NotPaused);
    if (FAILED(voice->Resume()))
      return std::unexpected(BackendError::InternalBackendError);
    paused.clear();
    return {};
  }

  BackendResult<> set_volume(float volume) override {
    if (voice == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (volume < 0.0F || volume > 1.0F)
      return std::unexpected(BackendError::RangeOutOfBounds);
    const auto val = std::round(
        range_convert_midpoint(volume, 0.0, 0.5, 1.0, 0.0, 50.0, 100.0));
    if (val < 0.0F || val > 100.0F)
      return std::unexpected(BackendError::RangeOutOfBounds);
    if (FAILED(voice->SetVolume(static_cast<USHORT>(val))))
      return std::unexpected(BackendError::InternalBackendError);
    return {};
  }

  BackendResult<float> get_volume() override {
    if (voice == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    USHORT val;
    if (FAILED(voice->GetVolume(&val)))
      return std::unexpected(BackendError::InternalBackendError);
    return range_convert_midpoint(static_cast<float>(val), 0, 50, 100, 0.0, 0.5,
                                  1.0);
  }

  BackendResult<> set_rate(float rate) override {
    if (voice == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (rate < 0.0F || rate > 1.0F)
      return std::unexpected(BackendError::RangeOutOfBounds);
    const auto val =
        range_convert_midpoint(rate, 0.0, 0.5, 1.0, -10.0, 0.0, 10.0);
    if (val < -10.0F || val > 10.0F)
      return std::unexpected(BackendError::RangeOutOfBounds);
    if (FAILED(voice->SetRate(static_cast<LONG>(val))))
      return std::unexpected(BackendError::InternalBackendError);
    return {};
  }

  BackendResult<float> get_rate() override {
    if (voice == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    LONG val;
    if (FAILED(voice->GetRate(&val)))
      return std::unexpected(BackendError::InternalBackendError);
    return range_convert_midpoint(static_cast<float>(val), -10, 0, 10, 0.0, 0.5,
                                  1.0);
  }

  BackendResult<> set_pitch(float pitch) override {
    if (voice == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (pitch < 0.0F || pitch > 1.0F)
      return std::unexpected(BackendError::RangeOutOfBounds);
    const auto val =
        range_convert_midpoint(pitch, 0.0, 0.5, 1.0, -10.0, 0.0, 10.0);
    if (val < -10.0F || val > 10.0F)
      return std::unexpected(BackendError::RangeOutOfBounds);
    this->pitch.exchange(static_cast<LONG>(val), std::memory_order_acq_rel);
    return {};
  }

  BackendResult<float> get_pitch() override {
    if (voice == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    const auto val = pitch.load(std::memory_order_acquire);
    return range_convert_midpoint(static_cast<float>(val), -10, 0, 10, 0.0, 0.5,
                                  1.0);
  }

  BackendResult<> refresh_voices() override {
    if (voice == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    std::vector<VoiceInfo> new_voices;
    CComPtr<ISpObjectTokenCategory> category;
    HRESULT hr = category.CoCreateInstance(CLSID_SpObjectTokenCategory);
    if (FAILED(hr))
      return std::unexpected(BackendError::InternalBackendError);
    hr = category->SetId(SPCAT_VOICES, FALSE);
    if (FAILED(hr))
      return std::unexpected(BackendError::InternalBackendError);
    CComPtr<IEnumSpObjectTokens> enum_tokens;
    hr = category->EnumTokens(nullptr, nullptr, &enum_tokens);
    if (FAILED(hr))
      return std::unexpected(BackendError::InternalBackendError);
    ULONG count = 0;
    hr = enum_tokens->GetCount(&count);
    if (FAILED(hr))
      return std::unexpected(BackendError::InternalBackendError);
    new_voices.reserve(count);
    for (ULONG i = 0; i < count; ++i) {
      CComPtr<ISpObjectToken> token;
      hr = enum_tokens->Next(1, &token, nullptr);
      if (FAILED(hr))
        break;
      LPWSTR name_ptr = nullptr;
      hr = token->GetStringValue(nullptr, &name_ptr);
      if (FAILED(hr) || name_ptr == nullptr) {
        if (name_ptr != nullptr)
          CoTaskMemFree(name_ptr);
        continue;
      }
      std::wstring_view name_view{name_ptr};
      std::string name(simdutf::utf8_length_from_utf16le(
                           reinterpret_cast<const char16_t *>(name_view.data()),
                           name_view.size()),
                       '\0');
      (void)simdutf::convert_utf16le_to_utf8(
          reinterpret_cast<const char16_t *>(name_view.data()),
          name_view.size(), name.data()); // Deliberately ignored return value
      if (name.empty()) {
        if (name_ptr != nullptr)
          CoTaskMemFree(name_ptr);
        continue;
      }
      CoTaskMemFree(name_ptr);
      std::string language = "en-us";
      LPWSTR lang_ptr = nullptr;
      if (SUCCEEDED(token->GetStringValue(_T("Language"), &lang_ptr)) &&
          lang_ptr != nullptr) {
        std::wstring_view lang_view{lang_ptr};
        LANGID langid{};
        std::string lang_narrow(lang_view.size(), '\0');
        std::ranges::transform(lang_view, lang_narrow.begin(),
                               [](wchar_t c) { return static_cast<char>(c); });
        auto [ptr, ec] = std::from_chars(
            lang_narrow.data(), lang_narrow.data() + lang_narrow.size(), langid,
            16);
        CoTaskMemFree(lang_ptr);
        if (ec == std::errc{}) {
          std::array<wchar_t, LOCALE_NAME_MAX_LENGTH> locale_name{};
          if (LCIDToLocaleName(MAKELCID(langid, SORT_DEFAULT),
                               locale_name.data(), LOCALE_NAME_MAX_LENGTH,
                               0) != 0) {
            std::wstring_view locale_view{locale_name.data()};
            language.resize(simdutf::utf8_length_from_utf16le(
                reinterpret_cast<const char16_t *>(locale_view.data()),
                locale_view.size()));
            (void)simdutf::convert_utf16le_to_utf8(
                reinterpret_cast<const char16_t *>(locale_view.data()),
                locale_view.size(), language.data());
            std::ranges::transform(
                language, language.begin(),
                [](unsigned char c) { return std::tolower(c); });
          }
        }
      }
      new_voices.emplace_back(VoiceInfo{.token = std::move(token),
                                        .name = std::move(name),
                                        .language = std::move(language)});
    }
    {
      std::unique_lock ul(voices_lock);
      std::swap(voices, new_voices);
    }
    return {};
  }

  BackendResult<std::size_t> count_voices() override {
    if (voice == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    std::shared_lock sl(voices_lock);
    return voices.size();
  }

  BackendResult<std::string> get_voice_name(std::size_t id) override {
    if (voice == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    std::shared_lock sl(voices_lock);
    if (id >= voices.size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    return voices[id].name;
  }

  BackendResult<std::string> get_voice_language(std::size_t id) override {
    if (voice == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    std::shared_lock sl(voices_lock);
    if (id >= voices.size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    return voices[id].language;
  }

  BackendResult<> set_voice(std::size_t id) override {
    if (voice == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    std::shared_lock sl(voices_lock);
    auto const old_val = voice_idx.load(std::memory_order_acquire);
    if (id >= voices.size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    if (FAILED(voice->SetVoice(voices[id].token)))
      return std::unexpected(BackendError::InternalBackendError);
    voice_idx.store(id, std::memory_order_release);
    CComPtr<ISpStreamFormat> output_format;
    if (SUCCEEDED(voice->GetOutputStream(&output_format)) &&
        output_format != nullptr) {
      GUID format_id;
      WAVEFORMATEX *wfx = nullptr;
      if (SUCCEEDED(output_format->GetFormat(&format_id, &wfx)) &&
          wfx != nullptr) {
        audio_channels.store(wfx->nChannels, std::memory_order_release);
        audio_sample_rate.store(wfx->nSamplesPerSec, std::memory_order_release);
        audio_bit_depth.store(wfx->wBitsPerSample, std::memory_order_release);
        CoTaskMemFree(wfx);
      } else {
        voice_idx.store(old_val, std::memory_order_release);
        if (FAILED(voice->SetVoice(voices[old_val].token)))
          return std::unexpected(BackendError::InternalBackendError);
        return std::unexpected(BackendError::InternalBackendError);
      }
    } else {
      voice_idx.store(old_val, std::memory_order_release);
      if (FAILED(voice->SetVoice(voices[old_val].token)))
        return std::unexpected(BackendError::InternalBackendError);
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }

  BackendResult<std::size_t> get_voice() override {
    if (voice == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    return voice_idx.load(std::memory_order_acquire);
  }

  BackendResult<std::size_t> get_channels() override {
    if (voice == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    return audio_channels.load(std::memory_order_relaxed);
  }

  BackendResult<std::size_t> get_sample_rate() override {
    if (voice == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    return audio_sample_rate.load(std::memory_order_relaxed);
  }

  BackendResult<std::size_t> get_bit_depth() override {
    if (voice == nullptr || !initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    return audio_bit_depth.load(std::memory_order_relaxed);
  }
};

REGISTER_BACKEND_WITH_ID(SapiBackend, Backends::SAPI, "SAPI", 97);
#endif
