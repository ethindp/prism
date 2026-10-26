// SPDX-License-Identifier: MPL-2.0

#ifdef PRISM_HAVE_GDK_SPEECH
#include "../backend_catalog.h"
#include "../speech_player.h"
#include "../utils.h"
#include <XGameRuntimeInit.h>
#include <XPackage.h>
#include <XSpeechSynthesizer.h>
#ifndef PRISM_XBOX
#include "../optional_library.h"
#endif
#include <algorithm>
#include <array>
#include <atlbase.h>
#include <atomic>
#include <bitset>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <dr_wav/dr_wav.h>
#include <fmt/format.h>
#include <limits>
#include <mutex>
#include <objidl.h>
#include <simdutf.h>
#include <string>
#include <string_view>
#include <tchar.h>
#include <thread>
#include <utility>
#include <vector>
#include <xaudio2.h>
#include <xmllite.h>

namespace {
using tstring = std::basic_string<TCHAR>;
using tstring_view = std::basic_string_view<TCHAR>;
constexpr float DEFAULT_PARAMETER = 0.5F;
constexpr std::size_t NO_VOICE = std::numeric_limits<std::size_t>::max();
#ifndef PRISM_XBOX
constexpr auto GDK_RUNTIME_LIBRARY = TEXT("xgameruntime.thunks.dll");
#endif
#if !defined(PRISM_XBOX) || !defined(PRISM_STATIC)
#define PRISM_GDK_OWNS_RUNTIME 1
#endif

static_assert(sizeof(TCHAR) == sizeof(char16_t),
              "GDK XmlLite integration requires Unicode TCHAR");

std::string format_hresult(HRESULT hr) {
  std::array<TCHAR, 1024> buffer{};
  const auto code = static_cast<DWORD>(hr);
  const DWORD written = FormatMessage(
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code,
      0, buffer.data(), static_cast<DWORD>(buffer.size()), nullptr);
  if (written == 0)
    return fmt::format("HRESULT {:#010x} (message unavailable, Win32 error {})",
                       code, GetLastError());
  tstring_view message(buffer.data(), written);
  while (!message.empty() &&
         (message.back() == _T('\r') || message.back() == _T('\n')))
    message.remove_suffix(1);
  std::string utf8(
      simdutf::utf8_length_from_utf16le(
          reinterpret_cast<const char16_t *>(message.data()), message.size()),
      '\0');
  utf8.resize(simdutf::convert_utf16le_to_utf8(
      reinterpret_cast<const char16_t *>(message.data()), message.size(),
      utf8.data()));
  return fmt::format("HRESULT {:#010x}: {}", code, utf8);
}

bool failed(HRESULT hr, std::string_view operation,
            const LogSource &log) noexcept {
  if (SUCCEEDED(hr))
    return false;
  try {
    log.error("{}: {}", operation, format_hresult(hr));
  } catch (...) {
    log.error("{}: HRESULT {:#010x}", operation, static_cast<unsigned>(hr));
  }
  return true;
}

struct GdkApi {
#ifndef PRISM_XBOX
  SharedLibrary library = open_optional_library(GDK_RUNTIME_LIBRARY, nullptr);
#endif
  decltype(&::XGameRuntimeInitialize) XGameRuntimeInitialize = nullptr;
  decltype(&::XGameRuntimeUninitialize) XGameRuntimeUninitialize = nullptr;
  decltype(&::XPackageGetUserLocale) XPackageGetUserLocale = nullptr;
  decltype(&::XSpeechSynthesizerCloseHandle) XSpeechSynthesizerCloseHandle =
      nullptr;
  decltype(&::XSpeechSynthesizerCloseStreamHandle)
      XSpeechSynthesizerCloseStreamHandle = nullptr;
  decltype(&::XSpeechSynthesizerCreate) XSpeechSynthesizerCreate = nullptr;
  decltype(&::XSpeechSynthesizerCreateStreamFromSsml)
      XSpeechSynthesizerCreateStreamFromSsml = nullptr;
  decltype(&::XSpeechSynthesizerCreateStreamFromText)
      XSpeechSynthesizerCreateStreamFromText = nullptr;
  decltype(&::XSpeechSynthesizerEnumerateInstalledVoices)
      XSpeechSynthesizerEnumerateInstalledVoices = nullptr;
  decltype(&::XSpeechSynthesizerGetStreamData) XSpeechSynthesizerGetStreamData =
      nullptr;
  decltype(&::XSpeechSynthesizerGetStreamDataSize)
      XSpeechSynthesizerGetStreamDataSize = nullptr;
  decltype(&::XSpeechSynthesizerSetCustomVoice)
      XSpeechSynthesizerSetCustomVoice = nullptr;
  decltype(&::XSpeechSynthesizerSetDefaultVoice)
      XSpeechSynthesizerSetDefaultVoice = nullptr;
  bool available = false;
#ifndef PRISM_XBOX
  DWORD load_error = 0;
#endif

  GdkApi() noexcept {
#ifdef PRISM_XBOX
    XGameRuntimeInitialize = &::XGameRuntimeInitialize;
    XGameRuntimeUninitialize = &::XGameRuntimeUninitialize;
    XPackageGetUserLocale = &::XPackageGetUserLocale;
    XSpeechSynthesizerCloseHandle = &::XSpeechSynthesizerCloseHandle;
    XSpeechSynthesizerCloseStreamHandle =
        &::XSpeechSynthesizerCloseStreamHandle;
    XSpeechSynthesizerCreate = &::XSpeechSynthesizerCreate;
    XSpeechSynthesizerCreateStreamFromSsml =
        &::XSpeechSynthesizerCreateStreamFromSsml;
    XSpeechSynthesizerCreateStreamFromText =
        &::XSpeechSynthesizerCreateStreamFromText;
    XSpeechSynthesizerEnumerateInstalledVoices =
        &::XSpeechSynthesizerEnumerateInstalledVoices;
    XSpeechSynthesizerGetStreamData = &::XSpeechSynthesizerGetStreamData;
    XSpeechSynthesizerGetStreamDataSize =
        &::XSpeechSynthesizerGetStreamDataSize;
    XSpeechSynthesizerSetCustomVoice = &::XSpeechSynthesizerSetCustomVoice;
    XSpeechSynthesizerSetDefaultVoice = &::XSpeechSynthesizerSetDefaultVoice;
    available = true;
#else
    if (!library) {
      load_error = SharedLibrary::last_error();
      return;
    }
    bool ok = true;
    ok = library.bind(XGameRuntimeInitialize, "XGameRuntimeInitialize") && ok;
    ok = library.bind(XGameRuntimeUninitialize, "XGameRuntimeUninitialize") &&
         ok;
    ok = library.bind(XPackageGetUserLocale, "XPackageGetUserLocale") && ok;
    ok = library.bind(XSpeechSynthesizerCloseHandle,
                      "XSpeechSynthesizerCloseHandle") &&
         ok;
    ok = library.bind(XSpeechSynthesizerCloseStreamHandle,
                      "XSpeechSynthesizerCloseStreamHandle") &&
         ok;
    ok = library.bind(XSpeechSynthesizerCreate, "XSpeechSynthesizerCreate") &&
         ok;
    ok = library.bind(XSpeechSynthesizerCreateStreamFromSsml,
                      "XSpeechSynthesizerCreateStreamFromSsml") &&
         ok;
    ok = library.bind(XSpeechSynthesizerCreateStreamFromText,
                      "XSpeechSynthesizerCreateStreamFromText") &&
         ok;
    ok = library.bind(XSpeechSynthesizerEnumerateInstalledVoices,
                      "XSpeechSynthesizerEnumerateInstalledVoices") &&
         ok;
    ok = library.bind(XSpeechSynthesizerGetStreamData,
                      "XSpeechSynthesizerGetStreamData") &&
         ok;
    ok = library.bind(XSpeechSynthesizerGetStreamDataSize,
                      "XSpeechSynthesizerGetStreamDataSize") &&
         ok;
    ok = library.bind(XSpeechSynthesizerSetCustomVoice,
                      "XSpeechSynthesizerSetCustomVoice") &&
         ok;
    ok = library.bind(XSpeechSynthesizerSetDefaultVoice,
                      "XSpeechSynthesizerSetDefaultVoice") &&
         ok;
    available = ok;
#endif
  }
};
GdkApi &gdk_api() noexcept {
  static GdkApi api;
  return api;
}

#ifdef PRISM_GDK_OWNS_RUNTIME
class GamingRuntimeLease {
  static inline std::mutex mutex;
  static inline std::size_t references = 0;

public:
  static bool acquire(HRESULT &out_hr) noexcept {
    std::scoped_lock lock(mutex);
    if (references != 0) {
      ++references;
      out_hr = 0;
      return true;
    }
    if (!gdk_api().available) {
      out_hr = E_NOTIMPL;
      return false;
    }
    out_hr = gdk_api().XGameRuntimeInitialize();
    if (FAILED(out_hr))
      return false;
    references = 1;
    return true;
  }

  static void release() noexcept {
    std::scoped_lock lock(mutex);
    if (references == 0)
      return;
    if (--references == 0)
      gdk_api().XGameRuntimeUninitialize();
  }
};
#endif

struct GdkVoice {
  std::string id;
  std::string name;
  std::string language;
};

struct SynthesizedAudio {
  std::vector<float> samples;
  std::size_t channels{};
  std::size_t sample_rate{};
  std::size_t bit_depth{};
};

class StreamGuard {
public:
  XSpeechSynthesizerStreamHandle handle{};
  ~StreamGuard() {
    if (handle != nullptr)
      (void)gdk_api().XSpeechSynthesizerCloseStreamHandle(handle);
  }
  StreamGuard() = default;
  StreamGuard(const StreamGuard &) = delete;
  StreamGuard &operator=(const StreamGuard &) = delete;
};

tstring widen(std::string_view text) {
  if (text.empty())
    return {};
  const auto count = simdutf::utf16_length_from_utf8(text.data(), text.size());
  tstring result(count, _T('\0'));
  const auto written = simdutf::convert_utf8_to_utf16le(
      text.data(), text.size(), reinterpret_cast<char16_t *>(result.data()));
  if (written == 0)
    return {};
  result.resize(written);
  return result;
}

std::string make_ssml(std::string_view text, std::string_view language,
                      float rate, float pitch, const LogSource &log) {
  const auto wide_text = widen(text);
  const auto wide_language = widen(language);
  if ((!text.empty() && wide_text.empty()) || wide_language.empty())
    return {};
  const auto wide_rate = widen(fmt::format("{:.4f}", rate));
  const auto wide_pitch = widen(fmt::format("{:+.4f}st", pitch));
  constexpr auto ns = _T("http://www.w3.org/2001/10/synthesis");
  CComPtr<IStream> stream;
  CComPtr<IXmlWriter> writer;
  CComPtr<IXmlWriterOutput> output;
  if (failed(CreateStreamOnHGlobal(nullptr, TRUE, &stream),
             "CreateStreamOnHGlobal", log) ||
      failed(CreateXmlWriter(__uuidof(IXmlWriter),
                             reinterpret_cast<void **>(&writer), nullptr),
             "CreateXmlWriter", log) ||
      failed(CreateXmlWriterOutputWithEncodingName(stream, nullptr, _T("utf-8"),
                                                   &output),
             "CreateXmlWriterOutputWithEncodingName", log) ||
      failed(writer->SetOutput(output), "IXmlWriter::SetOutput", log) ||
      failed(writer->WriteStartElement(nullptr, _T("speak"), ns),
             "Writing the speak element", log) ||
      failed(writer->WriteAttributeString(nullptr, _T("version"), nullptr,
                                          _T("1.0")),
             "Writing the SSML version", log) ||
      failed(writer->WriteAttributeString(
                 _T("xml"), _T("lang"),
                 _T("http://www.w3.org/XML/1998/namespace"),
                 wide_language.c_str()),
             "Writing the SSML language", log) ||
      failed(writer->WriteStartElement(nullptr, _T("prosody"), ns),
             "Writing the prosody element", log) ||
      failed(writer->WriteAttributeString(nullptr, _T("rate"), nullptr,
                                          wide_rate.c_str()),
             "Writing the rate", log) ||
      failed(writer->WriteAttributeString(nullptr, _T("pitch"), nullptr,
                                          wide_pitch.c_str()),
             "Writing the pitch", log) ||
      failed(writer->WriteString(wide_text.c_str()), "Writing the text", log) ||
      failed(writer->WriteEndElement(), "Closing the prosody element", log) ||
      failed(writer->WriteEndElement(), "Closing the speak element", log) ||
      failed(writer->Flush(), "IXmlWriter::Flush", log))
    return {};
  STATSTG stat{};
  LARGE_INTEGER origin{};
  if (failed(stream->Stat(&stat, STATFLAG_NONAME), "IStream::Stat", log) ||
      stat.cbSize.QuadPart > ULONG_MAX ||
      failed(stream->Seek(origin, STREAM_SEEK_SET, nullptr), "IStream::Seek",
             log))
    return {};
  std::string result(static_cast<std::size_t>(stat.cbSize.QuadPart), '\0');
  ULONG read = 0;
  if (failed(
          stream->Read(result.data(), static_cast<ULONG>(result.size()), &read),
          "IStream::Read", log) ||
      read != result.size())
    return {};
  return result;
}
} // namespace

#ifdef PRISM_CONSOLE
using GdkBackendBase = TextToSpeechBackend;
#else
using GdkBackendBase = ComTextToSpeechBackend;
#endif

class GdkSpeechBackend final : public GdkBackendBase {
  mutable std::mutex synth_mutex;
  XSpeechSynthesizerHandle synth{};
  std::vector<GdkVoice> voices;
  std::size_t selected_voice = NO_VOICE;
  bool format_cached = false;
  std::size_t cached_channels = 0;
  std::size_t cached_sample_rate = 0;
  std::size_t cached_bit_depth = 0;
  std::atomic<float> volume{DEFAULT_PARAMETER};
  std::atomic<float> rate{DEFAULT_PARAMETER};
  std::atomic<float> pitch{DEFAULT_PARAMETER};
  SpeechPlayer player;
  struct SpeechRequest {
    std::string text;
    float rate;
    float pitch;
    std::uint64_t generation;
  };
  std::mutex request_mutex;
  std::condition_variable_any request_cv;
  std::deque<SpeechRequest> requests;
  std::jthread synthesis_thread;
  std::uint64_t synthesizing_generation = 0;
  bool paused = false;
  bool ready = false;
  LogSource logger{"GdkSpeech"};
#ifdef PRISM_GDK_OWNS_RUNTIME
  bool runtime_initialized = false;
#endif

  static bool CALLBACK voice_callback(
      const XSpeechSynthesizerVoiceInformation *information, void *context) {
    if (information == nullptr || context == nullptr)
      return false;
    auto &out = *static_cast<std::vector<GdkVoice> *>(context);
    try {
      out.push_back(GdkVoice{
          .id = information->VoiceId != nullptr ? information->VoiceId : "",
          .name = information->DisplayName != nullptr ? information->DisplayName
                                                      : "",
          .language =
              information->Language != nullptr ? information->Language : "",
      });
    } catch (...) {
      return false;
    }
    return true;
  }

  static BackendError synth_error(HRESULT hr) noexcept {
    if (hr == E_INVALIDARG)
      return BackendError::InvalidParam;
    if (hr == E_OUTOFMEMORY)
      return BackendError::MemoryFailure;
#ifdef E_BOUNDS
    if (hr == E_BOUNDS)
      return BackendError::RangeOutOfBounds;
#endif
    return BackendError::SpeakFailure;
  }

  std::string synthesis_language_unlocked() const {
    if (selected_voice < voices.size() &&
        !voices[selected_voice].language.empty())
      return voices[selected_voice].language;
    std::array<char, 128> locale{};
    if (SUCCEEDED(
            gdk_api().XPackageGetUserLocale(locale.size(), locale.data())) &&
        locale.front() != '\0')
      return locale.data();
    const auto it = std::ranges::find_if(
        voices, [](const GdkVoice &voice) { return !voice.language.empty(); });
    return it != voices.end() ? it->language : "en-US";
  }

  BackendResult<std::vector<std::byte>>
  synthesize_wave_unlocked(std::string_view text, float current_rate,
                           float current_pitch) {
    if (synth == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    const bool neutral_rate =
        std::abs(current_rate - DEFAULT_PARAMETER) < 1e-6F;
    const bool neutral_pitch =
        std::abs(current_pitch - DEFAULT_PARAMETER) < 1e-6F;
    StreamGuard stream;
    HRESULT hr;
    if (neutral_rate && neutral_pitch) {
      const std::string owned{text};
      hr = gdk_api().XSpeechSynthesizerCreateStreamFromText(
          synth, owned.c_str(), &stream.handle);
    } else {
      const auto rate_multiplier = range_convert_midpoint(
          current_rate, 0.0F, 0.5F, 1.0F, 0.5F, 1.0F, 6.0F);
      const auto pitch_semitones = range_convert_midpoint(
          current_pitch, 0.0F, 0.5F, 1.0F, -12.0F, 0.0F, 12.0F);
      const auto ssml = make_ssml(text, synthesis_language_unlocked(),
                                  rate_multiplier, pitch_semitones, logger);
      if (ssml.empty())
        return std::unexpected(BackendError::SpeakFailure);
      hr = gdk_api().XSpeechSynthesizerCreateStreamFromSsml(synth, ssml.c_str(),
                                                            &stream.handle);
    }
    if (failed(hr, "XSpeechSynthesizerCreateStream", logger))
      return std::unexpected(synth_error(hr));
    std::size_t size = 0;
    hr = gdk_api().XSpeechSynthesizerGetStreamDataSize(stream.handle, &size);
    if (failed(hr, "XSpeechSynthesizerGetStreamDataSize", logger))
      return std::unexpected(synth_error(hr));
    if (size == 0) {
      logger.warn("GDK synthesized stream reported zero bytes");
      return std::unexpected(BackendError::InvalidAudioFormat);
    }
    std::vector<std::byte> bytes(size);
    std::size_t used = 0;
    hr = gdk_api().XSpeechSynthesizerGetStreamData(stream.handle, bytes.size(),
                                                   bytes.data(), &used);
    if (failed(hr, "XSpeechSynthesizerGetStreamData", logger))
      return std::unexpected(synth_error(hr));
    if (used == 0 || used > bytes.size()) {
      logger.error("GDK returned invalid stream byte count: {} of {}", used,
                   bytes.size());
      return std::unexpected(BackendError::InvalidAudioFormat);
    }
    bytes.resize(used);
    return bytes;
  }

  BackendResult<SynthesizedAudio> synthesize_pcm_unlocked(std::string_view text,
                                                          float current_rate,
                                                          float current_pitch) {
    auto bytes = synthesize_wave_unlocked(text, current_rate, current_pitch);
    if (!bytes)
      return std::unexpected(bytes.error());
    drwav wav{};
    if (drwav_init_memory(&wav, bytes->data(), bytes->size(), nullptr) == 0)
      return std::unexpected(BackendError::InvalidAudioFormat);
    if (wav.channels == 0 || wav.sampleRate == 0 ||
        wav.totalPCMFrameCount >
            std::numeric_limits<std::size_t>::max() / wav.channels) {
      drwav_uninit(&wav);
      return std::unexpected(BackendError::InvalidAudioFormat);
    }
    const auto frames_requested = wav.totalPCMFrameCount;
    const std::size_t sample_count =
        static_cast<std::size_t>(frames_requested) * wav.channels;
    SynthesizedAudio result{
        .samples = std::vector<float>(sample_count),
        .channels = wav.channels,
        .sample_rate = wav.sampleRate,
        .bit_depth = wav.bitsPerSample,
    };
    const auto frames_read = drwav_read_pcm_frames_f32(&wav, frames_requested,
                                                       result.samples.data());
    result.samples.resize(static_cast<std::size_t>(frames_read) * wav.channels);
    drwav_uninit(&wav);
    if (frames_read == 0 && frames_requested != 0)
      return std::unexpected(BackendError::InvalidAudioFormat);
    cached_channels = result.channels;
    cached_sample_rate = result.sample_rate;
    cached_bit_depth = result.bit_depth;
    format_cached = true;
    return result;
  }

  BackendResult<> refresh_voices_unlocked() {
    std::string selected_id;
    if (selected_voice < voices.size())
      selected_id = voices[selected_voice].id;
    std::vector<GdkVoice> next;
    const HRESULT hr = gdk_api().XSpeechSynthesizerEnumerateInstalledVoices(
        &next, &voice_callback);
    if (failed(hr, "XSpeechSynthesizerEnumerateInstalledVoices", logger))
      return std::unexpected(BackendError::BackendNotAvailable);
    if (next.empty()) {
      logger.warn("GDK reports no installed speech voices");
      return std::unexpected(BackendError::NoVoices);
    }
    logger.debug("Enumerated {} GDK speech voices", next.size());
    voices = std::move(next);
    if (selected_id.empty()) {
      selected_voice = NO_VOICE;
      return {};
    }
    const auto it = std::ranges::find(voices, selected_id, &GdkVoice::id);
    if (it != voices.end()) {
      selected_voice = static_cast<std::size_t>(it - voices.begin());
      return {};
    }
    selected_voice = NO_VOICE;
    format_cached = false;
    if (synth != nullptr &&
        failed(gdk_api().XSpeechSynthesizerSetDefaultVoice(synth),
               "XSpeechSynthesizerSetDefaultVoice", logger))
      return std::unexpected(BackendError::VoiceNotFound);
    return {};
  }

  BackendResult<> cache_format_unlocked() {
    if (format_cached)
      return {};
    auto audio =
        synthesize_pcm_unlocked(" ", rate.load(std::memory_order_acquire),
                                pitch.load(std::memory_order_acquire));
    if (!audio)
      return std::unexpected(audio.error());
    return {};
  }

  void render(const SpeechRequest &request) {
    BackendResult<SynthesizedAudio> result =
        std::unexpected(BackendError::NotInitialized);
    {
      std::scoped_lock lock(synth_mutex);
      if (synth != nullptr)
        result =
            synthesize_pcm_unlocked(request.text, request.rate, request.pitch);
    }
    if (!result) {
      logger.error("Asynchronous speech synthesis failed ({})",
                   static_cast<int>(result.error()));
      return;
    }
    player.play(request.generation, std::move(result->samples),
                result->channels, result->sample_rate);
  }

  void synthesis_loop(const std::stop_token &stop) {
    while (true) {
      SpeechRequest request;
      {
        std::unique_lock lock(request_mutex);
        if (!request_cv.wait(lock, stop, [this] { return !requests.empty(); }))
          return;
        request = std::move(requests.front());
        requests.pop_front();
        synthesizing_generation = request.generation;
      }
      try {
        render(request);
      } catch (const std::bad_alloc &) {
        logger.error("Out of memory while synthesizing speech");
      }
      std::scoped_lock lock(request_mutex);
      synthesizing_generation = 0;
    }
  }

  [[nodiscard]] bool has_speech_locked() const {
    const auto current = player.generation();
    return synthesizing_generation == current || player.has_audio() ||
           std::ranges::any_of(requests, [current](const SpeechRequest &r) {
             return r.generation == current;
           });
  }

  void discard() {
    std::scoped_lock lock(request_mutex);
    player.stop();
    requests.clear();
    paused = false;
  }

public:
  GdkSpeechBackend() {
#ifdef PRISM_GDK_OWNS_RUNTIME
    HRESULT hr = 0;
    runtime_initialized = GamingRuntimeLease::acquire(hr);
    if (!runtime_initialized && gdk_api().available)
      (void)failed(hr, "XGameRuntimeInitialize", logger);
#endif
#ifndef PRISM_XBOX
    if (!gdk_api().available) {
      if (gdk_api().load_error != 0)
        logger.debug(TEXT("{} could not be loaded (LastError={})"),
                     GDK_RUNTIME_LIBRARY, gdk_api().load_error);
      else
        logger.warn(TEXT("{} lacks a function Prism needs"),
                    GDK_RUNTIME_LIBRARY);
    }
#endif
  }

  ~GdkSpeechBackend() override {
    if (synthesis_thread.joinable()) {
      discard();
      synthesis_thread.request_stop();
      synthesis_thread.join();
    }
    std::scoped_lock lock(synth_mutex);
    if (synth != nullptr) {
      (void)gdk_api().XSpeechSynthesizerCloseHandle(synth);
      synth = nullptr;
    }
#ifdef PRISM_GDK_OWNS_RUNTIME
    if (runtime_initialized) {
      GamingRuntimeLease::release();
      runtime_initialized = false;
    }
#endif
  }

  [[nodiscard]] std::string_view get_name() const override {
    return "GdkSpeech";
  }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features =
        SUPPORTS_SPEAK | SUPPORTS_SPEAK_TO_MEMORY | SUPPORTS_OUTPUT |
        SUPPORTS_IS_SPEAKING | SUPPORTS_STOP | SUPPORTS_PAUSE |
        SUPPORTS_RESUME | SUPPORTS_SET_VOLUME | SUPPORTS_GET_VOLUME |
        SUPPORTS_SET_RATE | SUPPORTS_GET_RATE | SUPPORTS_SET_PITCH |
        SUPPORTS_GET_PITCH | SUPPORTS_REFRESH_VOICES | SUPPORTS_COUNT_VOICES |
        SUPPORTS_GET_VOICE_NAME | SUPPORTS_GET_VOICE_LANGUAGE |
        SUPPORTS_GET_VOICE | SUPPORTS_SET_VOICE | SUPPORTS_GET_CHANNELS |
        SUPPORTS_GET_SAMPLE_RATE | SUPPORTS_GET_BIT_DEPTH |
        PERFORMS_SILENCE_TRIMMING_ON_SPEAK |
        PERFORMS_SILENCE_TRIMMING_ON_SPEAK_TO_MEMORY;
#ifdef PRISM_GDK_OWNS_RUNTIME
    if (!runtime_initialized)
      return features;
#endif
    if (!gdk_api().available)
      return features;
    XSpeechSynthesizerHandle probe_synth{};
    CComPtr<IXAudio2> probe_audio;
    const bool speech_ok =
        SUCCEEDED(gdk_api().XSpeechSynthesizerCreate(&probe_synth)) &&
        probe_synth != nullptr;
    const bool audio_ok =
        SUCCEEDED(XAudio2Create(&probe_audio, 0, XAUDIO2_DEFAULT_PROCESSOR)) &&
        probe_audio != nullptr;
    if (probe_synth != nullptr)
      (void)gdk_api().XSpeechSynthesizerCloseHandle(probe_synth);
    if (speech_ok && audio_ok)
      features |= IS_SUPPORTED_AT_RUNTIME;
    return features;
  }

  BackendResult<> initialize() override {
    if (ready)
      return std::unexpected(BackendError::AlreadyInitialized);
    if (!gdk_api().available)
      return std::unexpected(BackendError::BackendNotAvailable);
#ifdef PRISM_GDK_OWNS_RUNTIME
    if (!runtime_initialized)
      return std::unexpected(BackendError::BackendNotAvailable);
#endif
    {
      std::scoped_lock lock(synth_mutex);
      const HRESULT hr = gdk_api().XSpeechSynthesizerCreate(&synth);
      if (failed(hr, "XSpeechSynthesizerCreate", logger) || synth == nullptr) {
        synth = nullptr;
        return std::unexpected(BackendError::BackendNotAvailable);
      }
      if (auto result = refresh_voices_unlocked(); !result) {
        (void)gdk_api().XSpeechSynthesizerCloseHandle(synth);
        synth = nullptr;
        return result;
      }
    }
    if (auto result = player.open(); !result) {
      std::scoped_lock lock(synth_mutex);
      (void)gdk_api().XSpeechSynthesizerCloseHandle(synth);
      synth = nullptr;
      return result;
    }
    synthesis_thread = std::jthread(
        [this](const std::stop_token &stop) { synthesis_loop(stop); });
    ready = true;
    logger.info("GDK speech backend initialized");
    std::scoped_lock lock(synth_mutex);
    if (auto result = cache_format_unlocked(); !result)
      logger.warn(
          "Could not cache GDK speech output format during initialization");
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    if (interrupt)
      discard();
    {
      std::scoped_lock lock(request_mutex);
      requests.push_back(SpeechRequest{
          .text = std::string{text},
          .rate = rate.load(std::memory_order_acquire),
          .pitch = pitch.load(std::memory_order_acquire),
          .generation = player.generation(),
      });
    }
    request_cv.notify_one();
    return {};
  }

  BackendResult<> speak_to_memory(std::string_view text, AudioCallback callback,
                                  void *userdata) override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    const float current_rate = rate.load(std::memory_order_acquire);
    const float current_pitch = pitch.load(std::memory_order_acquire);
    const float current_volume = volume.load(std::memory_order_acquire);
    BackendResult<SynthesizedAudio> audio =
        std::unexpected(BackendError::NotInitialized);
    {
      std::scoped_lock lock(synth_mutex);
      audio = synthesize_pcm_unlocked(text, current_rate, current_pitch);
    }
    if (!audio)
      return std::unexpected(audio.error());
    auto trimmed = trim_silence_rms_gate_inplace(
        std::span<float>(audio->samples), audio->channels, audio->sample_rate);
    for (float &sample : trimmed.view)
      sample = std::clamp(sample * current_volume, -1.0F, 1.0F);
    callback(userdata, trimmed.view.data(), trimmed.view.size(),
             audio->channels, audio->sample_rate);
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<bool> is_speaking() override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    std::scoped_lock lock(request_mutex);
    return !paused && has_speech_locked();
  }

  BackendResult<> stop() override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    discard();
    return {};
  }

  BackendResult<> pause() override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    std::scoped_lock lock(request_mutex);
    if (paused)
      return std::unexpected(BackendError::AlreadyPaused);
    if (!has_speech_locked())
      return std::unexpected(BackendError::NotSpeaking);
    player.pause();
    paused = true;
    return {};
  }

  BackendResult<> resume() override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    std::scoped_lock lock(request_mutex);
    if (!paused)
      return std::unexpected(BackendError::NotPaused);
    player.resume();
    paused = false;
    return {};
  }

  BackendResult<> set_volume(float value) override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    volume.store(value, std::memory_order_release);
    player.set_volume(value);
    return {};
  }

  BackendResult<float> get_volume() override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    return volume.load(std::memory_order_acquire);
  }

  BackendResult<> set_rate(float value) override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    rate.store(value, std::memory_order_release);
    return {};
  }

  BackendResult<float> get_rate() override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    return rate.load(std::memory_order_acquire);
  }

  BackendResult<> set_pitch(float value) override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    pitch.store(value, std::memory_order_release);
    return {};
  }

  BackendResult<float> get_pitch() override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    return pitch.load(std::memory_order_acquire);
  }

  BackendResult<> refresh_voices() override {
    std::scoped_lock lock(synth_mutex);
    if (synth == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    return refresh_voices_unlocked();
  }

  BackendResult<std::size_t> count_voices() override {
    std::scoped_lock lock(synth_mutex);
    if (synth == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    return voices.size();
  }

  BackendResult<std::string> get_voice_name(std::size_t id) override {
    std::scoped_lock lock(synth_mutex);
    if (synth == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (id >= voices.size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    return voices[id].name;
  }

  BackendResult<std::string> get_voice_language(std::size_t id) override {
    std::scoped_lock lock(synth_mutex);
    if (synth == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (id >= voices.size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    return voices[id].language;
  }

  BackendResult<> set_voice(std::size_t id) override {
    std::scoped_lock lock(synth_mutex);
    if (synth == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (id >= voices.size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    const HRESULT hr = gdk_api().XSpeechSynthesizerSetCustomVoice(
        synth, voices[id].id.c_str());
    if (failed(hr, "XSpeechSynthesizerSetCustomVoice", logger))
      return std::unexpected(BackendError::VoiceNotFound);
    selected_voice = id;
    format_cached = false;
    return {};
  }

  BackendResult<std::size_t> get_voice() override {
    std::scoped_lock lock(synth_mutex);
    if (synth == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (selected_voice == NO_VOICE)
      return std::unexpected(BackendError::VoiceNotFound);
    return selected_voice;
  }

  BackendResult<std::size_t> get_channels() override {
    std::scoped_lock lock(synth_mutex);
    if (synth == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (auto result = cache_format_unlocked(); !result)
      return std::unexpected(result.error());
    return cached_channels;
  }

  BackendResult<std::size_t> get_sample_rate() override {
    std::scoped_lock lock(synth_mutex);
    if (synth == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (auto result = cache_format_unlocked(); !result)
      return std::unexpected(result.error());
    return cached_sample_rate;
  }

  BackendResult<std::size_t> get_bit_depth() override {
    std::scoped_lock lock(synth_mutex);
    if (synth == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (auto result = cache_format_unlocked(); !result)
      return std::unexpected(result.error());
    return cached_bit_depth;
  }
};

REGISTER_BACKEND_WITH_ID(GdkSpeechBackend, Backends::GdkSpeech, "GdkSpeech",
                         100)
#endif
