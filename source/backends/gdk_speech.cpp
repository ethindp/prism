// SPDX-License-Identifier: MPL-2.0

#ifdef PRISM_HAVE_GDK_SPEECH
#include "../backend_catalog.h"
#include "../utils.h"
#include <XGameRuntimeInit.h>
#include <XPackage.h>
#include <XSpeechSynthesizer.h>
#ifndef PRISM_XBOX
#include "../shared_library.h"
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <chrono>
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
#include <wrl/client.h>
#include <xaudio2.h>
#include <xmllite.h>

namespace {
using tstring = std::basic_string<TCHAR>;
using tstring_view = std::basic_string_view<TCHAR>;
constexpr float DEFAULT_PARAMETER = 0.5F;
constexpr std::size_t NO_VOICE = std::numeric_limits<std::size_t>::max();
constexpr auto PLAYBACK_POLL = std::chrono::milliseconds{5};

static_assert(sizeof(TCHAR) == sizeof(char16_t),
              "GDK XmlLite integration requires Unicode TCHAR");

std::string format_hresult(HRESULT hr) {
  std::array<TCHAR, 1024> buffer{};
  const DWORD code = static_cast<DWORD>(hr);
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

// This is such a bad hack
// Todo: fix this so there's no duplication
bool failed(HRESULT hr, std::string_view operation) noexcept {
  if (SUCCEEDED(hr))
    return false;
  try {
    LogSource{"GdkSpeech"}.error("{}: {}", operation, format_hresult(hr));
  } catch (...) {
    LogSource{"GdkSpeech"}.error("{}: HRESULT {:#010x}", operation,
                                 static_cast<unsigned>(hr));
  }
  return true;
}

struct GdkApi {
#ifndef PRISM_XBOX
  SharedLibrary library{TEXT("xgameruntime.dll")};
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
      LogSource{"GdkSpeech"}.debug("GDK runtime library not found");
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
    if (!ok)
      LogSource{"GdkSpeech"}.warn(
          "GDK runtime lacks one or more required exports");
    else
      LogSource{"GdkSpeech"}.debug("GDK runtime exports resolved");
#endif
  }
};
GdkApi &gdk_api() noexcept {
  static GdkApi api;
  return api;
}

#ifndef PRISM_STATIC
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
    if (failed(out_hr, "out_hr"))
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

struct PlaybackItem {
  std::vector<float> samples;
  std::uint32_t channels{};
  std::uint32_t sample_rate{};
  float volume{};
  std::uint64_t generation{};
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
                      float rate, float pitch) {
  using Microsoft::WRL::ComPtr;
  ComPtr<IStream> stream;
  if (failed(CreateStreamOnHGlobal(nullptr, TRUE, &stream),
             "CreateStreamOnHGlobal(nullptr, TRUE, &stream)"))
    return {};
  ComPtr<IXmlWriter> writer;
  if (failed(CreateXmlWriter(__uuidof(IXmlWriter),
                             reinterpret_cast<void **>(writer.GetAddressOf()),
                             nullptr),
             "CreateXmlWriter(__uuidof(IXmlWriter), reinterpret_cast<void "
             "**>(writer.GetAddressOf()), nullptr)"))
    return {};
  ComPtr<IXmlWriterOutput> output;
  if (failed(CreateXmlWriterOutputWithEncodingName(stream.Get(), nullptr,
                                                   _T("utf-8"), &output),
             "CreateXmlWriterOutputWithEncodingName( stream.Get(), nullptr, "
             "_T(utf-8), &output)") ||
      failed(writer->SetOutput(output.Get()),
             "writer->SetOutput(output.Get())"))
    return {};
  const auto wide_text = widen(text);
  const auto wide_language = widen(language);
  if ((!text.empty() && wide_text.empty()) || wide_language.empty())
    return {};
  const auto wide_rate = widen(fmt::format("{:.4f}", rate));
  const auto wide_pitch = widen(fmt::format("{:+.4f}st", pitch));
  constexpr auto ns = _T("http://www.w3.org/2001/10/synthesis");
  if (failed(writer->WriteStartElement(nullptr, _T("speak"), ns),
             "writer->WriteStartElement(nullptr, _T(speak), ns)") ||
      failed(writer->WriteAttributeString(nullptr, _T("version"), nullptr,
                                          _T("1.0")),
             "writer->WriteAttributeString(nullptr, _T(version), nullptr, "
             "_T(1.0))") ||
      failed(writer->WriteAttributeString(
                 _T("xml"), _T("lang"),
                 _T("http://www.w3.org/XML/1998/namespace"),
                 wide_language.c_str()),
             "writer->WriteAttributeString(_T(xml), _T(lang), "
             "_T(http://www.w3.org/XML/1998/namespace), wide") ||
      failed(writer->WriteStartElement(nullptr, _T("prosody"), ns),
             "writer->WriteStartElement(nullptr, _T(prosody), ns)") ||
      failed(writer->WriteAttributeString(nullptr, _T("rate"), nullptr,
                                          wide_rate.c_str()),
             "writer->WriteAttributeString(nullptr, _T(rate), nullptr, "
             "wide_rate.c_str())") ||
      failed(writer->WriteAttributeString(nullptr, _T("pitch"), nullptr,
                                          wide_pitch.c_str()),
             "writer->WriteAttributeString(nullptr, _T(pitch), nullptr, "
             "wide_pitch.c_str())") ||
      failed(writer->WriteString(wide_text.c_str()),
             "writer->WriteString(wide_text.c_str())") ||
      failed(writer->WriteEndElement(), "writer->WriteEndElement()") ||
      failed(writer->WriteEndElement(), "writer->WriteEndElement()") ||
      failed(writer->Flush(), "writer->Flush()"))
    return {};
  STATSTG stat{};
  LARGE_INTEGER origin{};
  if (failed(stream->Stat(&stat, STATFLAG_NONAME),
             "stream->Stat(&stat, STATFLAG_NONAME)") ||
      stat.cbSize.QuadPart > static_cast<ULONGLONG>(SIZE_MAX) ||
      failed(stream->Seek(origin, STREAM_SEEK_SET, nullptr),
             "stream->Seek(origin, STREAM_SEEK_SET, nullptr)"))
    return {};
  std::string result(static_cast<std::size_t>(stat.cbSize.QuadPart), '\0');
  if (result.size() > static_cast<std::size_t>(ULONG_MAX))
    return {};
  ULONG read = 0;
  if (failed(
          stream->Read(result.data(), static_cast<ULONG>(result.size()), &read),
          "stream->Read(result.data(), static_cast<ULONG>(result.size()), "
          "&read)") ||
      read != result.size())
    return {};
  return result;
}
} // namespace

class GdkSpeechBackend final : public TextToSpeechBackend {
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
  IXAudio2 *audio_engine = nullptr;
  IXAudio2MasteringVoice *mastering_voice = nullptr;
  mutable std::mutex playback_mutex;
  std::condition_variable playback_cv;
  std::deque<PlaybackItem> playback_queue;
  IXAudio2SourceVoice *current_voice = nullptr;
  std::jthread playback_thread;
  struct SpeechRequest {
    std::string text;
    float volume;
    float rate;
    float pitch;
    std::uint64_t generation;
  };
  std::mutex request_mutex;
  std::condition_variable request_cv;
  std::deque<SpeechRequest> requests;
  std::jthread synthesis_thread;
  bool synthesis_shutdown = false;
  std::uint64_t playback_generation = 1;
  std::uint64_t current_generation = 0;
  bool playback_paused = false;
  bool playback_shutdown = false;
  bool starting_voice = false;
  LogSource logger{"GdkSpeech"};
#ifndef PRISM_STATIC
  bool runtime_initialized = false;
#endif

  static bool CALLBACK voice_callback(
      const XSpeechSynthesizerVoiceInformation *information, void *context) {
    if (information == nullptr || context == nullptr)
      return false;
    auto &out = *static_cast<std::vector<GdkVoice> *>(context);
    out.push_back(GdkVoice{
        .id = information->VoiceId != nullptr ? information->VoiceId : "",
        .name =
            information->DisplayName != nullptr ? information->DisplayName : "",
        .language =
            information->Language != nullptr ? information->Language : "",
    });
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

  static BackendError audio_error(HRESULT hr) noexcept {
    if (hr == E_INVALIDARG)
      return BackendError::InvalidParam;
    if (hr == E_OUTOFMEMORY)
      return BackendError::MemoryFailure;
    return BackendError::InternalBackendError;
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
                                  rate_multiplier, pitch_semitones);
      if (ssml.empty())
        return std::unexpected(BackendError::SpeakFailure);
      hr = gdk_api().XSpeechSynthesizerCreateStreamFromSsml(synth, ssml.c_str(),
                                                            &stream.handle);
    }
    if (failed(hr, "GDK or XAudio2 operation")) {
      logger.error("GDK speech API failed: HRESULT {:#010x}",
                   static_cast<unsigned>(hr));
      return std::unexpected(synth_error(hr));
    }
    std::size_t size = 0;
    hr = gdk_api().XSpeechSynthesizerGetStreamDataSize(stream.handle, &size);
    if (failed(hr, "GDK or XAudio2 operation")) {
      logger.error("GDK speech API failed: HRESULT {:#010x}",
                   static_cast<unsigned>(hr));
      return std::unexpected(synth_error(hr));
    }
    if (size == 0) {
      logger.warn("GDK synthesized stream reported zero bytes");
      return std::unexpected(BackendError::InvalidAudioFormat);
    }
    std::vector<std::byte> bytes(size);
    std::size_t used = 0;
    hr = gdk_api().XSpeechSynthesizerGetStreamData(stream.handle, bytes.size(),
                                                   bytes.data(), &used);
    if (failed(hr, "GDK or XAudio2 operation")) {
      logger.error("GDK speech API failed: HRESULT {:#010x}",
                   static_cast<unsigned>(hr));
      return std::unexpected(synth_error(hr));
    }
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
    if (failed(hr, "GDK or XAudio2 operation"))
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
               "gdk_api().XSpeechSynthesizerSetDefaultVoice(synth)"))
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

  void playback_loop() noexcept {
    while (true) {
      PlaybackItem item;
      {
        std::unique_lock lock(playback_mutex);
        playback_cv.wait(lock, [this] {
          return playback_shutdown ||
                 (!playback_paused && !playback_queue.empty());
        });
        if (playback_shutdown)
          return;
        item = std::move(playback_queue.front());
        playback_queue.pop_front();
        starting_voice = true;
      }
      if (item.channels == 0 ||
          item.channels > std::numeric_limits<WORD>::max() ||
          item.sample_rate == 0 ||
          item.samples.size() >
              std::numeric_limits<UINT32>::max() / sizeof(float)) {
        logger.error("Refusing invalid synthesized audio format");
        std::scoped_lock lock(playback_mutex);
        starting_voice = false;
        playback_cv.notify_all();
        continue;
      }
      WAVEFORMATEX format{};
      format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
      format.nChannels = static_cast<WORD>(item.channels);
      format.nSamplesPerSec = item.sample_rate;
      format.wBitsPerSample = 32;
      format.nBlockAlign = static_cast<WORD>(format.nChannels * sizeof(float));
      format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
      format.cbSize = 0;
      IXAudio2SourceVoice *voice = nullptr;
      HRESULT hr = audio_engine->CreateSourceVoice(&voice, &format,
                                                   XAUDIO2_VOICE_NOPITCH, 1.0F,
                                                   nullptr, nullptr, nullptr);
      if (failed(hr, "GDK or XAudio2 operation") || voice == nullptr) {
        logger.error("CreateSourceVoice failed with HRESULT 0x{:08X}",
                     static_cast<unsigned>(hr));
        std::scoped_lock lock(playback_mutex);
        starting_voice = false;
        playback_cv.notify_all();
        continue;
      }
      XAUDIO2_BUFFER buffer{};
      buffer.Flags = XAUDIO2_END_OF_STREAM;
      buffer.AudioBytes =
          static_cast<UINT32>(item.samples.size() * sizeof(float));
      buffer.pAudioData = reinterpret_cast<const BYTE *>(item.samples.data());
      hr = voice->SubmitSourceBuffer(&buffer);
      if (SUCCEEDED(hr))
        hr = voice->SetVolume(item.volume);
      if (failed(hr, "GDK or XAudio2 operation")) {
        logger.error(
            "Preparing XAudio2 source voice failed with HRESULT 0x{:08X}",
            static_cast<unsigned>(hr));
        voice->DestroyVoice();
        std::scoped_lock lock(playback_mutex);
        starting_voice = false;
        playback_cv.notify_all();
        continue;
      }
      bool valid = false;
      {
        std::scoped_lock lock(playback_mutex);
        starting_voice = false;
        valid = !playback_shutdown && item.generation == playback_generation;
        if (valid) {
          current_voice = voice;
          current_generation = item.generation;
        }
        playback_cv.notify_all();
      }
      if (!valid) {
        voice->DestroyVoice();
        continue;
      }
      {
        std::scoped_lock lock(playback_mutex);
        if (!playback_paused && current_voice == voice &&
            item.generation == playback_generation) {
          hr = voice->Start(0);
          if (failed(hr, "GDK or XAudio2 operation")) {
            logger.error("XAudio2 source Start failed with HRESULT 0x{:08X}",
                         static_cast<unsigned>(hr));
            current_voice = nullptr;
            current_generation = 0;
          }
        }
      }
      if (failed(hr, "GDK or XAudio2 operation")) {
        voice->DestroyVoice();
        continue;
      }
      while (true) {
        {
          std::unique_lock lock(playback_mutex);
          if (playback_shutdown || item.generation != playback_generation ||
              current_voice != voice) {
            if (current_voice == voice) {
              current_voice = nullptr;
              current_generation = 0;
            }
            break;
          }
          if (playback_paused) {
            playback_cv.wait_for(lock, PLAYBACK_POLL, [this, &item, voice] {
              return playback_shutdown || !playback_paused ||
                     item.generation != playback_generation ||
                     current_voice != voice;
            });
            continue;
          }
        }
        XAUDIO2_VOICE_STATE state{};
        voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (state.BuffersQueued == 0) {
          std::scoped_lock lock(playback_mutex);
          if (current_voice == voice) {
            current_voice = nullptr;
            current_generation = 0;
          }
          playback_cv.notify_all();
          break;
        }
        std::unique_lock lock(playback_mutex);
        playback_cv.wait_for(lock, PLAYBACK_POLL, [this, &item, voice] {
          return playback_shutdown || playback_paused ||
                 item.generation != playback_generation ||
                 current_voice != voice;
        });
      }
      (void)voice->Stop(0);
      voice->DestroyVoice();
    }
  }

  void synthesis_loop() noexcept {
    for (;;) {
      SpeechRequest request;
      {
        std::unique_lock lock(request_mutex);
        request_cv.wait(
            lock, [this] { return synthesis_shutdown || !requests.empty(); });
        if (synthesis_shutdown)
          return;
        request = std::move(requests.front());
        requests.pop_front();
      }
      BackendResult<SynthesizedAudio> result =
          std::unexpected(BackendError::NotInitialized);
      {
        std::scoped_lock lock(synth_mutex);
        if (synth != nullptr)
          result = synthesize_pcm_unlocked(request.text, request.rate,
                                           request.pitch);
      }
      if (!result) {
        logger.error("Asynchronous speech synthesis failed ({})",
                     static_cast<int>(result.error()));
        continue;
      }
      {
        std::scoped_lock lock(playback_mutex);
        if (request.generation != playback_generation)
          continue;
      }
      (void)enqueue_playback(std::move(*result), request.volume);
    }
  }

  BackendResult<> initialize_audio_unlocked() {
    HRESULT hr = XAudio2Create(&audio_engine, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (failed(hr, "GDK or XAudio2 operation") || audio_engine == nullptr) {
      audio_engine = nullptr;
      return std::unexpected(audio_error(hr));
    }
    hr = audio_engine->CreateMasteringVoice(
        &mastering_voice, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE,
        0, nullptr, nullptr, AudioCategory_Speech);
    if (failed(hr, "GDK or XAudio2 operation") || mastering_voice == nullptr) {
      audio_engine->Release();
      audio_engine = nullptr;
      mastering_voice = nullptr;
      return std::unexpected(audio_error(hr));
    }
    playback_shutdown = false;
    playback_paused = false;
    playback_thread = std::jthread([this] { playback_loop(); });
    logger.debug("XAudio2 playback worker started");
    return {};
  }

  void shutdown_audio() noexcept {
    {
      std::scoped_lock lock(playback_mutex);
      playback_shutdown = true;
      ++playback_generation;
      playback_queue.clear();
      playback_paused = false;
      starting_voice = false;
      if (current_voice != nullptr)
        (void)current_voice->Stop(0);
    }
    playback_cv.notify_all();
    if (playback_thread.joinable())
      playback_thread.join();
    if (mastering_voice != nullptr) {
      mastering_voice->DestroyVoice();
      mastering_voice = nullptr;
    }
    if (audio_engine != nullptr) {
      audio_engine->Release();
      audio_engine = nullptr;
    }
  }

  BackendResult<> enqueue_playback(SynthesizedAudio &&audio,
                                   float playback_volume) {
    if (audio.channels == 0 ||
        audio.channels > std::numeric_limits<std::uint32_t>::max() ||
        audio.sample_rate == 0 ||
        audio.sample_rate > std::numeric_limits<std::uint32_t>::max())
      return std::unexpected(BackendError::InvalidAudioFormat);
    if (audio.samples.empty())
      return {};
    std::scoped_lock lock(playback_mutex);
    if (playback_shutdown || audio_engine == nullptr ||
        mastering_voice == nullptr)
      return std::unexpected(BackendError::BackendNotAvailable);
    playback_queue.push_back(PlaybackItem{
        .samples = std::move(audio.samples),
        .channels = static_cast<std::uint32_t>(audio.channels),
        .sample_rate = static_cast<std::uint32_t>(audio.sample_rate),
        .volume = playback_volume,
        .generation = playback_generation,
    });
    playback_cv.notify_all();
    return {};
  }

public:
  GdkSpeechBackend() {
#ifndef PRISM_STATIC
    HRESULT hr = 0;
    runtime_initialized = GamingRuntimeLease::acquire(hr);
    if (!runtime_initialized)
      logger.error("XGameRuntimeInitialize failed with HRESULT 0x{:08X}",
                   static_cast<unsigned>(hr));
#endif
  }

  ~GdkSpeechBackend() override {
    {
      std::scoped_lock lock(request_mutex);
      synthesis_shutdown = true;
      requests.clear();
    }
    request_cv.notify_all();
    if (synthesis_thread.joinable())
      synthesis_thread.join();
    logger.debug("GDK synthesis worker stopped");
    shutdown_audio();
    std::scoped_lock lock(synth_mutex);
    if (synth != nullptr) {
      (void)gdk_api().XSpeechSynthesizerCloseHandle(synth);
      synth = nullptr;
    }
#ifndef PRISM_STATIC
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
        SUPPORTS_SET_VOICE | SUPPORTS_GET_CHANNELS | SUPPORTS_GET_SAMPLE_RATE |
        SUPPORTS_GET_BIT_DEPTH | PERFORMS_SILENCE_TRIMMING_ON_SPEAK_TO_MEMORY;
#ifndef PRISM_STATIC
    if (!runtime_initialized)
      return features;
#endif
    XSpeechSynthesizerHandle probe_synth{};
    IXAudio2 *probe_audio = nullptr;
    if (!gdk_api().available)
      return features;
    const bool speech_ok =
        SUCCEEDED(gdk_api().XSpeechSynthesizerCreate(&probe_synth)) &&
        probe_synth != nullptr;
    const bool audio_ok =
        SUCCEEDED(XAudio2Create(&probe_audio, 0, XAUDIO2_DEFAULT_PROCESSOR)) &&
        probe_audio != nullptr;
    if (probe_synth != nullptr)
      (void)gdk_api().XSpeechSynthesizerCloseHandle(probe_synth);
    if (probe_audio != nullptr)
      probe_audio->Release();
    if (speech_ok && audio_ok)
      features |= IS_SUPPORTED_AT_RUNTIME;
    return features;
  }

  BackendResult<> initialize() override {
    if (!gdk_api().available)
      return std::unexpected(BackendError::BackendNotAvailable);
#ifndef PRISM_STATIC
    if (!runtime_initialized)
      return std::unexpected(BackendError::BackendNotAvailable);
#endif
    std::scoped_lock lock(synth_mutex);
    if (synth != nullptr)
      return std::unexpected(BackendError::AlreadyInitialized);
    HRESULT hr = gdk_api().XSpeechSynthesizerCreate(&synth);
    if (failed(hr, "GDK or XAudio2 operation") || synth == nullptr) {
      logger.error("XSpeechSynthesizerCreate failed: HRESULT {:#010x}",
                   static_cast<unsigned>(hr));
      synth = nullptr;
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    if (auto result = refresh_voices_unlocked(); !result) {
      (void)gdk_api().XSpeechSynthesizerCloseHandle(synth);
      synth = nullptr;
      return result;
    }
    if (auto result = initialize_audio_unlocked(); !result) {
      (void)gdk_api().XSpeechSynthesizerCloseHandle(synth);
      synth = nullptr;
      return result;
    }
    synthesis_thread = std::jthread([this] { synthesis_loop(); });
    logger.info("GDK speech backend initialized");
    if (auto result = cache_format_unlocked(); !result)
      logger.warn(
          "Could not cache Xbox speech output format during initialization");
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (interrupt) {
      if (auto result = stop(); !result)
        return result;
    }
    {
      std::scoped_lock lock(synth_mutex);
      if (synth == nullptr)
        return std::unexpected(BackendError::NotInitialized);
    }
    std::uint64_t generation;
    {
      std::scoped_lock lock(playback_mutex);
      generation = playback_generation;
    }
    {
      std::scoped_lock lock(request_mutex);
      if (synthesis_shutdown)
        return std::unexpected(BackendError::BackendNotAvailable);
      requests.push_back(SpeechRequest{
          .text = std::string{text},
          .volume = volume.load(std::memory_order_acquire),
          .rate = rate.load(std::memory_order_acquire),
          .pitch = pitch.load(std::memory_order_acquire),
          .generation = generation,
      });
    }
    logger.debug("Queued {} bytes of speech text", text.size());
    request_cv.notify_one();
    return {};
  }

  BackendResult<> speak_to_memory(std::string_view text, AudioCallback callback,
                                  void *userdata) override {
    const float current_rate = rate.load(std::memory_order_acquire);
    const float current_pitch = pitch.load(std::memory_order_acquire);
    const float current_volume = volume.load(std::memory_order_acquire);
    BackendResult<SynthesizedAudio> audio =
        std::unexpected(BackendError::NotInitialized);
    {
      std::scoped_lock lock(synth_mutex);
      if (synth == nullptr)
        return std::unexpected(BackendError::NotInitialized);
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
    std::scoped_lock synth_lock(synth_mutex);
    if (synth == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    std::scoped_lock playback_lock(playback_mutex);
    const bool current_is_active =
        current_voice != nullptr && current_generation == playback_generation;
    const bool playback_active =
        !playback_paused &&
        (current_is_active || starting_voice || !playback_queue.empty());
    std::scoped_lock queue_lock(request_mutex);
    return playback_active || !requests.empty();
  }

  BackendResult<> stop() override {
    {
      std::scoped_lock synth_lock(synth_mutex);
      if (synth == nullptr)
        return std::unexpected(BackendError::NotInitialized);
    }
    {
      std::scoped_lock lock(playback_mutex);
      ++playback_generation;
      playback_queue.clear();
      playback_paused = false;
      starting_voice = false;
      if (current_voice != nullptr)
        (void)current_voice->Stop(0);
    }
    {
      std::scoped_lock lock(request_mutex);
      requests.clear();
    }
    logger.debug("Stopped speech and discarded pending requests");
    playback_cv.notify_all();
    return {};
  }

  BackendResult<> pause() override {
    {
      std::scoped_lock synth_lock(synth_mutex);
      if (synth == nullptr)
        return std::unexpected(BackendError::NotInitialized);
    }
    std::scoped_lock lock(playback_mutex);
    if (playback_paused)
      return std::unexpected(BackendError::AlreadyPaused);
    const bool current_is_active =
        current_voice != nullptr && current_generation == playback_generation;
    if (!current_is_active && !starting_voice && playback_queue.empty())
      return std::unexpected(BackendError::NotSpeaking);
    if (current_is_active) {
      const HRESULT hr = current_voice->Stop(0);
      if (failed(hr, "GDK or XAudio2 operation"))
        return std::unexpected(audio_error(hr));
    }
    playback_paused = true;
    playback_cv.notify_all();
    return {};
  }

  BackendResult<> resume() override {
    {
      std::scoped_lock synth_lock(synth_mutex);
      if (synth == nullptr)
        return std::unexpected(BackendError::NotInitialized);
    }
    std::scoped_lock lock(playback_mutex);
    if (!playback_paused)
      return std::unexpected(BackendError::NotPaused);
    if (current_voice != nullptr) {
      const HRESULT hr = current_voice->Start(0);
      if (failed(hr, "GDK or XAudio2 operation"))
        return std::unexpected(audio_error(hr));
    }
    playback_paused = false;
    playback_cv.notify_all();
    return {};
  }

  BackendResult<> set_volume(float value) override {
    if (!std::isfinite(value) || value < 0.0F || value > 1.0F)
      return std::unexpected(BackendError::RangeOutOfBounds);
    {
      std::scoped_lock lock(synth_mutex);
      if (synth == nullptr)
        return std::unexpected(BackendError::NotInitialized);
    }
    volume.store(value, std::memory_order_release);
    std::scoped_lock lock(playback_mutex);
    if (current_voice != nullptr && current_generation == playback_generation) {
      const HRESULT hr = current_voice->SetVolume(value);
      if (failed(hr, "GDK or XAudio2 operation"))
        return std::unexpected(audio_error(hr));
    }
    return {};
  }

  BackendResult<float> get_volume() override {
    std::scoped_lock lock(synth_mutex);
    if (synth == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    return volume.load(std::memory_order_acquire);
  }

  BackendResult<> set_rate(float value) override {
    if (!std::isfinite(value) || value < 0.0F || value > 1.0F)
      return std::unexpected(BackendError::RangeOutOfBounds);
    std::scoped_lock lock(synth_mutex);
    if (synth == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    rate.store(value, std::memory_order_release);
    format_cached = false;
    return {};
  }

  BackendResult<float> get_rate() override {
    std::scoped_lock lock(synth_mutex);
    if (synth == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    return rate.load(std::memory_order_acquire);
  }

  BackendResult<> set_pitch(float value) override {
    if (!std::isfinite(value) || value < 0.0F || value > 1.0F)
      return std::unexpected(BackendError::RangeOutOfBounds);
    std::scoped_lock lock(synth_mutex);
    if (synth == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    pitch.store(value, std::memory_order_release);
    format_cached = false;
    return {};
  }

  BackendResult<float> get_pitch() override {
    std::scoped_lock lock(synth_mutex);
    if (synth == nullptr)
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
    if (failed(hr, "GDK or XAudio2 operation"))
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
