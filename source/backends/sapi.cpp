// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#include "utils.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <limits>
#include <mutex>
#include <optional>
#include <ranges>
#include <shared_mutex>
#include <stop_token>
#include <thread>
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

struct HandleGuard {
  HANDLE h{};
  ~HandleGuard() noexcept {
    if (h != nullptr && h != INVALID_HANDLE_VALUE)
      CloseHandle(h);
  }
  HandleGuard(HANDLE h) : h(h) {}
  HandleGuard(const HandleGuard &) = delete;
  HandleGuard &operator=(const HandleGuard &) = delete;
};

struct VoiceOutputStreamResetter {
  CComPtr<ISpVoice> v;
  ~VoiceOutputStreamResetter() noexcept {
    if (v != nullptr)
      (void)v->SetOutput(nullptr, TRUE);
  }
};

struct SetVoiceRollbackGuard {
  CComPtr<ISpVoice> voice;
  CComPtr<ISpObjectToken> old_token;
  bool commit = false;
  ~SetVoiceRollbackGuard() noexcept {
    if (!commit && voice != nullptr && old_token != nullptr)
      (void)voice->SetVoice(old_token);
  }
  SetVoiceRollbackGuard(const SetVoiceRollbackGuard &) = delete;
  SetVoiceRollbackGuard &operator=(const SetVoiceRollbackGuard &) = delete;
  SetVoiceRollbackGuard(CComPtr<ISpVoice> v, CComPtr<ISpObjectToken> t)
      : voice(std::move(v)), old_token(std::move(t)) {}
};

struct SapiSpeakParams {
  std::wstring text;
  DWORD flags;
};

struct InitHandshake {
  std::mutex &m;
  std::condition_variable &cv;
  std::optional<bool> &ready;
  std::atomic<IStream *> &marshal_stream;
  bool committed = false;
  ~InitHandshake() noexcept {
    if (!committed) {
      std::lock_guard g(m);
      ready = false;
      cv.notify_all();
    }
  }

  void succeed(IStream *s) {
    std::lock_guard g(m);
    marshal_stream.store(s, std::memory_order_release);
    committed = true;
    ready = true;
    cv.notify_all();
  }
};

class SapiBackend final : public TextToSpeechBackend {
private:
  std::jthread worker_thread;
  CComPtr<ISpVoice> voice;
  std::atomic_flag initialized;
  bool paused = false;
  std::atomic<LONG> pitch{0};
  std::vector<VoiceInfo> voices;
  std::atomic_uint64_t voice_idx{0};
  mutable std::shared_mutex voices_lock;
  std::atomic<std::size_t> audio_channels{0};
  std::atomic<std::size_t> audio_sample_rate{0};
  std::atomic<std::size_t> audio_bit_depth{0};
  std::atomic<IStream *> marshal_stream{nullptr};
  mutable std::mutex init_mtx;
  mutable std::condition_variable init_cv;
  std::optional<bool> ready = std::nullopt;
  std::mutex voice_lock;

  void thread_proc(const std::stop_token &st) {
    InitHandshake handshake{.m = init_mtx,
                            .cv = init_cv,
                            .ready = ready,
                            .marshal_stream = marshal_stream};
    HandleGuard stop_event(CreateEvent(nullptr, TRUE, FALSE, nullptr));
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED |
                                             COINIT_SPEED_OVER_MEMORY);
    const bool should_uninit = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
      return;
    }
    if (stop_event.h == nullptr) {
      return;
    }
    {
      std::stop_callback cb(st, [h = stop_event.h] { SetEvent(h); });
      CComPtr<ISpVoice> local_voice;
      hr = local_voice.CoCreateInstance(CLSID_SpVoice);
      if (FAILED(hr)) {
        if (should_uninit)
          CoUninitialize();
        return;
      }
      IStream *stream = nullptr;
      hr = CoMarshalInterThreadInterfaceInStream(__uuidof(ISpVoice),
                                                 local_voice, &stream);
      if (FAILED(hr)) {
        if (should_uninit)
          CoUninitialize();
        return;
      }
      handshake.succeed(stream);
      while (true) {
        auto const r = MsgWaitForMultipleObjectsEx(
            1, &stop_event.h, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (r == WAIT_OBJECT_0 || r == WAIT_FAILED)
          break;
        MSG msg{};
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
          DispatchMessage(&msg);
        }
      }
      if (should_uninit)
        CoUninitialize();
    }
  }

  BackendResult<> require_ready_locked() const {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    return {};
  }

  BackendResult<SapiSpeakParams> make_speak_args(std::string_view text,
                                                 DWORD base_flags = 0) const {
    if (!simdutf::validate_utf8(text.data(), text.size()))
      return std::unexpected(BackendError::InvalidUtf8);
    if (text.size() >= std::numeric_limits<int>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    std::wstring wtext(
        simdutf::utf16_length_from_utf8(text.data(), text.size()), _T('\0'));
    if (simdutf::convert_utf8_to_utf16le(
            text.data(), text.size(),
            reinterpret_cast<char16_t *>(wtext.data())) == 0)
      return std::unexpected(BackendError::InvalidUtf8);
    DWORD flags = base_flags;
    const auto p = pitch.load(std::memory_order_acquire);
    if (p != 0) {
      wtext = std::format(_T("<pitch absmiddle=\"{}\"/>{}"), p, wtext);
      flags |= SPF_IS_XML;
    }
    return SapiSpeakParams{.text = std::move(wtext), .flags = flags};
  }

  BackendResult<> refresh_cached_output_params_locked() {
    if (voice == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    CComPtr<ISpStreamFormat> fmt;
    HRESULT hr = voice->GetOutputStream(&fmt);
    if (FAILED(hr) || fmt == nullptr)
      return std::unexpected(BackendError::InternalBackendError);
    GUID format_id{};
    WAVEFORMATEX *wfx = nullptr;
    hr = fmt->GetFormat(&format_id, &wfx);
    if (FAILED(hr) || wfx == nullptr) {
      if (wfx != nullptr)
        CoTaskMemFree(wfx);
      return std::unexpected(BackendError::InternalBackendError);
    }
    audio_channels.store(wfx->nChannels, std::memory_order_release);
    audio_sample_rate.store(wfx->nSamplesPerSec, std::memory_order_release);
    audio_bit_depth.store(wfx->wBitsPerSample, std::memory_order_release);
    CoTaskMemFree(wfx);
    return {};
  }

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
    std::unique_lock vl(voice_lock);
    std::unique_lock lock(init_mtx);
    if (voice != nullptr) {
      return std::unexpected(BackendError::AlreadyInitialized);
    }
    ready = std::nullopt;
    worker_thread = std::jthread(
        [this](const std::stop_token &st) { this->thread_proc(st); });
    auto const success = init_cv.wait_for(lock, std::chrono::seconds(5),
                                          [this] { return ready.has_value(); });
    IStream *stream =
        marshal_stream.exchange(nullptr, std::memory_order_acq_rel);
    if (!success || !*ready || stream == nullptr) {
      worker_thread.request_stop();
      worker_thread.join();
      return std::unexpected(BackendError::InternalBackendError);
    }
    HRESULT hr = CoGetInterfaceAndReleaseStream(
        stream, __uuidof(ISpVoice), reinterpret_cast<void **>(&voice));
    if (FAILED(hr)) {
      worker_thread.request_stop();
      worker_thread.join();
      return std::unexpected(BackendError::InternalBackendError);
    }
    if (auto const res = refresh_voices(); !res) {
      worker_thread.request_stop();
      worker_thread.join();
      return res;
    }
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
      worker_thread.request_stop();
      worker_thread.join();
      return std::unexpected(BackendError::InternalBackendError);
    }
    if (auto const r = refresh_cached_output_params_locked(); !r) {
      worker_thread.request_stop();
      worker_thread.join();
      return std::unexpected(BackendError::InternalBackendError);
    }
    initialized.test_and_set();
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    auto const args = make_speak_args(text, SPF_ASYNC);
    if (!args)
      return std::unexpected(args.error());
    const auto &wtext = args->text;
    DWORD flags = args->flags;
    std::unique_lock vl(voice_lock);
    if (auto r = require_ready_locked(); !r)
      return r;
    paused = false;
    if (interrupt)
      if (FAILED(voice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr)))
        return std::unexpected(BackendError::SpeakFailure);
    if (FAILED(voice->Speak(wtext.c_str(), flags, nullptr)))
      return std::unexpected(BackendError::SpeakFailure);
    return {};
  }

  BackendResult<> speak_to_memory(std::string_view text, AudioCallback callback,
                                  void *userdata) override {
    auto const args = make_speak_args(text, SPF_DEFAULT);
    if (!args)
      return std::unexpected(args.error());
    const auto &wtext = args->text;
    DWORD flags = args->flags;
    CComPtr<ISpStream> stream;
    CComPtr<IStream> base_stream;
    HRESULT hr = CreateStreamOnHGlobal(nullptr, TRUE, &base_stream);
    if (FAILED(hr))
      return std::unexpected(BackendError::InternalBackendError);
    hr = stream.CoCreateInstance(CLSID_SpStream);
    if (FAILED(hr))
      return std::unexpected(BackendError::InternalBackendError);
    std::size_t channels = 0;
    std::size_t sample_rate = 0;
    std::size_t bit_depth = 0;
    {
      std::unique_lock vl(voice_lock);
      if (auto r = require_ready_locked(); !r)
        return r;
      CComPtr<ISpStreamFormat> output_format;
      hr = voice->GetOutputStream(&output_format);
      if (FAILED(hr) || output_format == nullptr)
        return std::unexpected(BackendError::InternalBackendError);
      GUID format_id{};
      WAVEFORMATEX *wfx = nullptr;
      hr = output_format->GetFormat(&format_id, &wfx);
      if (FAILED(hr) || wfx == nullptr) {
        if (wfx != nullptr)
          CoTaskMemFree(wfx);
        return std::unexpected(BackendError::InternalBackendError);
      }
      channels = wfx->nChannels;
      sample_rate = wfx->nSamplesPerSec;
      bit_depth = wfx->wBitsPerSample;
      hr = stream->SetBaseStream(base_stream, format_id, wfx);
      CoTaskMemFree(wfx);
      if (FAILED(hr))
        return std::unexpected(BackendError::InternalBackendError);
      hr = voice->SetOutput(stream, TRUE);
      if (FAILED(hr))
        return std::unexpected(BackendError::InternalBackendError);
      VoiceOutputStreamResetter reset{voice};
      paused = false;
      if (FAILED(voice->Speak(wtext.c_str(), flags, nullptr)))
        return std::unexpected(BackendError::SpeakFailure);
    }
    LARGE_INTEGER zero{};
    ULARGE_INTEGER size{};
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
    if (bit_depth == 0 || (bit_depth != 8 && bit_depth != 16))
      return std::unexpected(BackendError::InternalBackendError);
    std::size_t sample_count =
        static_cast<std::size_t>(size.QuadPart) / (bit_depth / 8);
    std::vector<float> samples(sample_count);
    if (bit_depth == 16) {
      const auto *src = reinterpret_cast<const int16_t *>(buffer.data());
      for (std::size_t i = 0; i < sample_count; ++i)
        samples[i] = static_cast<float>(src[i]) / 32768.0F;
    } else if (bit_depth == 8) {
      const auto *src = reinterpret_cast<const uint8_t *>(buffer.data());
      for (std::size_t i = 0; i < sample_count; ++i)
        samples[i] = static_cast<float>(src[i] - 128) / 128.0F;
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
    std::unique_lock vl(voice_lock);
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    SPVOICESTATUS status;
    if (FAILED(voice->GetStatus(&status, nullptr)))
      return std::unexpected(BackendError::InternalBackendError);
    return status.dwRunningState == SPRS_IS_SPEAKING;
  }

  BackendResult<> stop() override {
    std::unique_lock vl(voice_lock);
    if (auto r = require_ready_locked(); !r)
      return r;
    if (FAILED(voice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr)))
      return std::unexpected(BackendError::SpeakFailure);
    return {};
  }

  BackendResult<> pause() override {
    std::unique_lock vl(voice_lock);
    if (auto r = require_ready_locked(); !r)
      return r;
    if (paused)
      return std::unexpected(BackendError::AlreadyPaused);
    if (FAILED(voice->Pause()))
      return std::unexpected(BackendError::InternalBackendError);
    paused = true;
    return {};
  }

  BackendResult<> resume() override {
    std::unique_lock vl(voice_lock);
    if (auto r = require_ready_locked(); !r)
      return r;
    if (!paused)
      return std::unexpected(BackendError::NotPaused);
    if (FAILED(voice->Resume()))
      return std::unexpected(BackendError::InternalBackendError);
    paused = false;
    return {};
  }

  BackendResult<> set_volume(float volume) override {
    std::unique_lock vl(voice_lock);
    if (!initialized.test())
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
    std::unique_lock vl(voice_lock);
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    USHORT val;
    if (FAILED(voice->GetVolume(&val)))
      return std::unexpected(BackendError::InternalBackendError);
    return range_convert_midpoint(static_cast<float>(val), 0, 50, 100, 0.0, 0.5,
                                  1.0);
  }

  BackendResult<> set_rate(float rate) override {
    std::unique_lock vl(voice_lock);
    if (!initialized.test())
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
    std::unique_lock vl(voice_lock);
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    LONG val;
    if (FAILED(voice->GetRate(&val)))
      return std::unexpected(BackendError::InternalBackendError);
    return range_convert_midpoint(static_cast<float>(val), -10, 0, 10, 0.0, 0.5,
                                  1.0);
  }

  BackendResult<> set_pitch(float pitch) override {
    if (!initialized.test())
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
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    const auto val = pitch.load(std::memory_order_acquire);
    return range_convert_midpoint(static_cast<float>(val), -10, 0, 10, 0.0, 0.5,
                                  1.0);
  }

  BackendResult<> refresh_voices() override {
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
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    std::shared_lock sl(voices_lock);
    return voices.size();
  }

  BackendResult<std::string> get_voice_name(std::size_t id) override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    std::shared_lock sl(voices_lock);
    if (id >= voices.size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    return voices[id].name;
  }

  BackendResult<std::string> get_voice_language(std::size_t id) override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    std::shared_lock sl(voices_lock);
    if (id >= voices.size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    return voices[id].language;
  }

  BackendResult<> set_voice(std::size_t id) override {
    std::scoped_lock lock(voice_lock, voices_lock);
    CComPtr<ISpObjectToken> new_token;
    if (id >= voices.size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    new_token = voices[id].token;
    if (new_token == nullptr)
      return std::unexpected(BackendError::InternalBackendError);
    if (auto r = require_ready_locked(); !r)
      return r;
    CComPtr<ISpObjectToken> old_token;
    HRESULT hr = voice->GetVoice(&old_token);
    if (FAILED(hr) || old_token == nullptr)
      return std::unexpected(BackendError::InternalBackendError);
    SetVoiceRollbackGuard rb{voice, old_token};
    hr = voice->SetVoice(new_token);
    if (FAILED(hr))
      return std::unexpected(BackendError::InternalBackendError);
    if (auto const r = refresh_cached_output_params_locked(); !r) {
      if (SUCCEEDED(voice->SetVoice(old_token)))
        rb.commit = true;
      if (auto const r2 = refresh_cached_output_params_locked(); !r2)
        return r2;
      return r;
    }
    rb.commit = true;
    voice_idx.store(id, std::memory_order_release);
    return {};
  }

  BackendResult<std::size_t> get_voice() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    return voice_idx.load(std::memory_order_acquire);
  }

  BackendResult<std::size_t> get_channels() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    return audio_channels.load(std::memory_order_acquire);
  }

  BackendResult<std::size_t> get_sample_rate() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    return audio_sample_rate.load(std::memory_order_acquire);
  }

  BackendResult<std::size_t> get_bit_depth() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    return audio_bit_depth.load(std::memory_order_acquire);
  }
};

REGISTER_BACKEND_WITH_ID(SapiBackend, Backends::SAPI, "SAPI", 97);
#endif
