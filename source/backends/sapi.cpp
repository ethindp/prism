// SPDX-License-Identifier: MPL-2.0

#ifdef _WIN32
#include "../backend.h"
#include "../backend_catalog.h"
#include "../logging.h"
#include "../speech_player.h"
#include "../utils.h"
#include <algorithm>
#include <array>
#include <atlbase.h>
#include <atomic>
#include <bitset>
#include <cctype>
#include <charconv>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <dr_wav/dr_wav.h>
#include <future>
#include <limits>
#include <mmreg.h>
#include <mutex>
#include <optional>
#include <ranges>
#include <sapi.h>
#include <shlwapi.h>
#include <simdutf.h>
#include <span>
#include <stop_token>
#include <string>
#include <tchar.h>
#include <thread>
#include <utility>
#include <variant>
#include <vector>
#include <version>
#include <windows.h>
#include <xmllite.h>

namespace {
struct VoiceInfo {
  std::string name;
  std::string language;
};

struct ComApartmentGuard {
  bool initialized;
  ~ComApartmentGuard() noexcept {
    if (initialized)
      CoUninitialize();
  }
  explicit ComApartmentGuard(HRESULT result) : initialized(SUCCEEDED(result)) {}
  ComApartmentGuard(const ComApartmentGuard &) = delete;
  ComApartmentGuard &operator=(const ComApartmentGuard &) = delete;
};

template <class T>
const T *lifetime_as_array(const void *p,
                           [[maybe_unused]] std::size_t n) noexcept {
#ifdef __cpp_lib_start_lifetime_as
  static_assert(__cpp_lib_start_lifetime_as >= 202207L,
                "__cpp_lib_start_lifetime_as must be >= 202207L");
  return std::start_lifetime_as_array<T>(p, n);
#else
  return reinterpret_cast<const T *>(p);
#endif
}

constexpr WAVEFORMATEX FALLBACK_FORMAT{
    .wFormatTag = WAVE_FORMAT_PCM,
    .nChannels = 1,
    .nSamplesPerSec = 22050,
    .nAvgBytesPerSec = 44100,
    .nBlockAlign = 2,
    .wBitsPerSample = 16,
    .cbSize = 0,
};

class CaptureStream final : public IStream {
public:
  CaptureStream(const SpeechPlayer *player, std::uint64_t generation)
      : player(player), generation(generation) {}

  [[nodiscard]] std::span<const std::byte> bytes() const noexcept {
    return captured;
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **object) noexcept override {
    if (object == nullptr)
      return E_POINTER;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(ISequentialStream) ||
        riid == __uuidof(IStream)) {
      *object = static_cast<IStream *>(this);
      AddRef();
      return S_OK;
    }
    *object = nullptr;
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() noexcept override { return ++references; }
  ULONG STDMETHODCALLTYPE Release() noexcept override {
    const ULONG remaining = --references;
    if (remaining == 0)
      delete this;
    return remaining;
  }

  HRESULT STDMETHODCALLTYPE
  Read([[maybe_unused]] void *data, [[maybe_unused]] ULONG size,
       [[maybe_unused]] ULONG *read) noexcept override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE Write(const void *data, ULONG size,
                                  ULONG *written) noexcept override {
    if (written != nullptr)
      *written = 0;
    if (data == nullptr && size != 0)
      return STG_E_INVALIDPOINTER;
    if (player != nullptr && player->generation() != generation)
      return E_ABORT;
    try {
      const auto *first = static_cast<const std::byte *>(data);
      captured.insert(captured.end(), first, first + size);
    } catch (...) {
      return E_OUTOFMEMORY;
    }
    position += size;
    if (written != nullptr)
      *written = size;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE
  Seek(LARGE_INTEGER move, DWORD origin,
       ULARGE_INTEGER *new_position) noexcept override {
    if ((origin != STREAM_SEEK_CUR && origin != STREAM_SEEK_END) ||
        move.QuadPart != 0)
      return STG_E_INVALIDFUNCTION;
    if (new_position != nullptr)
      new_position->QuadPart = position;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE
  SetSize([[maybe_unused]] ULARGE_INTEGER size) noexcept override {
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE
  CopyTo([[maybe_unused]] IStream *destination,
         [[maybe_unused]] ULARGE_INTEGER size,
         [[maybe_unused]] ULARGE_INTEGER *read,
         [[maybe_unused]] ULARGE_INTEGER *written) noexcept override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  Commit([[maybe_unused]] DWORD flags) noexcept override {
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE Revert() noexcept override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE
  LockRegion([[maybe_unused]] ULARGE_INTEGER offset,
             [[maybe_unused]] ULARGE_INTEGER size,
             [[maybe_unused]] DWORD type) noexcept override {
    return STG_E_INVALIDFUNCTION;
  }
  HRESULT STDMETHODCALLTYPE
  UnlockRegion([[maybe_unused]] ULARGE_INTEGER offset,
               [[maybe_unused]] ULARGE_INTEGER size,
               [[maybe_unused]] DWORD type) noexcept override {
    return STG_E_INVALIDFUNCTION;
  }
  HRESULT STDMETHODCALLTYPE
  Stat(STATSTG *stat, [[maybe_unused]] DWORD flags) noexcept override {
    if (stat == nullptr)
      return STG_E_INVALIDPOINTER;
    *stat = STATSTG{};
    stat->type = STGTY_STREAM;
    stat->cbSize.QuadPart = position;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE
  Clone([[maybe_unused]] IStream **clone) noexcept override {
    return E_NOTIMPL;
  }

private:
  ~CaptureStream() = default;
  std::atomic<ULONG> references{1};
  const SpeechPlayer *player;
  std::uint64_t generation;
  std::vector<std::byte> captured;
  std::uint64_t position = 0;
};

struct Prosody {
  LONG rate = 0;
  LONG pitch = 0;
  std::size_t voice = 0;
};

struct SpeakJob {
  std::wstring text;
  Prosody prosody;
  std::uint64_t generation = 0;
};

struct MemoryJob {
  std::wstring text;
  Prosody prosody;
  float volume = 1.0F;
  TextToSpeechBackend::AudioCallback *callback = nullptr;
  void *userdata = nullptr;
  std::promise<BackendResult<>> *done = nullptr;
};

struct SetVoiceJob {
  std::size_t voice = 0;
  std::promise<BackendResult<>> *done = nullptr;
};

struct RefreshVoicesJob {
  std::promise<BackendResult<>> *done = nullptr;
};

using Job = std::variant<SpeakJob, MemoryJob, SetVoiceJob, RefreshVoicesJob>;

template <class... Ts> struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

std::string narrow(std::wstring_view text) {
  const auto *source = reinterpret_cast<const char16_t *>(text.data());
  std::string result(simdutf::utf8_length_from_utf16le(source, text.size()),
                     '\0');
  result.resize(
      simdutf::convert_utf16le_to_utf8(source, text.size(), result.data()));
  return result;
}

std::string token_language(ISpObjectToken &token) {
  CComHeapPtr<WCHAR> value;
  if (FAILED(token.GetStringValue(_T("Language"), &value)) ||
      value.m_pData == nullptr)
    return "en-us";
  const std::wstring_view view{value};
  const auto first = view.substr(0, view.find(_T(';')));
  std::string ascii(first.size(), '\0');
  std::ranges::transform(first, ascii.begin(),
                         [](wchar_t c) { return static_cast<char>(c); });
  LANGID langid{};
  if (std::from_chars(ascii.data(), ascii.data() + ascii.size(), langid, 16)
          .ec != std::errc{})
    return "en-us";
  std::array<wchar_t, LOCALE_NAME_MAX_LENGTH> locale{};
  if (LCIDToLocaleName(MAKELCID(langid, SORT_DEFAULT), locale.data(),
                       LOCALE_NAME_MAX_LENGTH, 0) == 0)
    return "en-us";
  auto language = narrow(locale.data());
  std::ranges::transform(language, language.begin(),
                         [](unsigned char c) { return std::tolower(c); });
  return language;
}
} // namespace

class SapiBackend final : public ComTextToSpeechBackend {
private:
  static constexpr auto VOICE_CATEGORIES = std::to_array<LPCWSTR>({
      SPCAT_VOICES,
      _T("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech ")
      _T("Server\\v11.0\\Voices"),
  });

  SpeechPlayer player;
  bool ready = false;
  bool paused = false;
  float volume = 1.0F;
  float rate = 0.5F;
  float pitch = 0.5F;
  Prosody prosody;

  std::mutex state_mutex;
  std::condition_variable_any jobs_cv;
  std::deque<Job> jobs;
  std::uint64_t rendering_generation = 0;
  std::vector<VoiceInfo> voice_names;
  std::size_t published_voice = 0;
  std::size_t audio_channels = 0;
  std::size_t audio_sample_rate = 0;
  std::size_t audio_bit_depth = 0;

  CComPtr<ISpVoice> voice;
  std::vector<CComPtr<ISpObjectToken>> tokens;
  std::size_t active_voice = std::numeric_limits<std::size_t>::max();
  WAVEFORMATEX format = FALLBACK_FORMAT;

  std::jthread worker;
  LogSource logger{"SAPI"};

  static BackendResult<std::wstring> make_text(std::string_view text,
                                               LONG pitch_value) {
    if (text.size() >= std::numeric_limits<int>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    std::wstring wtext(
        simdutf::utf16_length_from_utf8(text.data(), text.size()), _T('\0'));
    if (simdutf::convert_valid_utf8_to_utf16(
            text.data(), text.size(),
            reinterpret_cast<char16_t *>(wtext.data())) == 0 &&
        !text.empty())
      return std::unexpected(BackendError::InvalidUtf8);
    if (pitch_value == 0)
      return wtext;
    CComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(nullptr, 0));
    if (!stream)
      return std::unexpected(BackendError::MemoryFailure);
    CComPtr<IXmlWriter> writer;
    CComPtr<IXmlWriterOutput> output;
    const auto pitch_text = std::to_wstring(pitch_value);
    LARGE_INTEGER zero{};
    ULARGE_INTEGER end{};
    if (FAILED(CreateXmlWriter(__uuidof(IXmlWriter),
                               reinterpret_cast<void **>(&writer), nullptr)) ||
        FAILED(
            writer->SetProperty(XmlWriterProperty_OmitXmlDeclaration, TRUE)) ||
        FAILED(CreateXmlWriterOutputWithEncodingName(stream, nullptr,
                                                     _T("utf-16"), &output)) ||
        FAILED(writer->SetOutput(output)) ||
        FAILED(writer->WriteStartElement(nullptr, _T("pitch"), nullptr)) ||
        FAILED(writer->WriteAttributeString(nullptr, _T("absmiddle"), nullptr,
                                            pitch_text.c_str())) ||
        FAILED(writer->WriteString(wtext.c_str())) ||
        FAILED(writer->WriteEndElement()) || FAILED(writer->Flush()) ||
        FAILED(stream->Seek(zero, STREAM_SEEK_END, &end)))
      return std::unexpected(BackendError::InternalBackendError);
    const auto bytes = static_cast<std::size_t>(end.QuadPart);
    if (bytes == 0 || bytes % sizeof(wchar_t) != 0 ||
        FAILED(stream->Seek(zero, STREAM_SEEK_SET, nullptr)))
      return std::unexpected(BackendError::InternalBackendError);
    std::wstring markup(bytes / sizeof(wchar_t), _T('\0'));
    ULONG read = 0;
    if (FAILED(stream->Read(markup.data(), static_cast<ULONG>(bytes), &read)) ||
        read != bytes)
      return std::unexpected(BackendError::InternalBackendError);
    return markup;
  }

  static DWORD speak_flags(LONG pitch_value) {
    return pitch_value != 0 ? SPF_IS_XML : SPF_IS_NOT_XML;
  }

  BackendResult<> enumerate_voices() {
    std::vector<CComPtr<ISpObjectToken>> found;
    std::vector<VoiceInfo> names;
    for (const auto &category_id : VOICE_CATEGORIES) {
      CComPtr<ISpObjectTokenCategory> category;
      if (FAILED(category.CoCreateInstance(CLSID_SpObjectTokenCategory)))
        continue;
      if (FAILED(category->SetId(category_id, FALSE))) {
        logger.debug(_T("voice category absent, skipping: {}"), category_id);
        continue;
      }
      CComPtr<IEnumSpObjectTokens> tokens_enum;
      if (FAILED(category->EnumTokens(nullptr, nullptr, &tokens_enum)))
        continue;
      CComPtr<ISpObjectToken> token;
      while (tokens_enum->Next(1, &token, nullptr) == S_OK) {
        CComHeapPtr<WCHAR> name;
        if (SUCCEEDED(token->GetStringValue(nullptr, &name)) &&
            name.m_pData != nullptr && name[0] != _T('\0')) {
          names.push_back(VoiceInfo{.name = narrow(name.m_pData),
                                    .language = token_language(*token)});
          found.push_back(token);
        }
        token.Release();
      }
    }
    if (found.empty())
      return std::unexpected(BackendError::NoVoices);
    std::size_t current = 0;
    CComPtr<ISpObjectToken> in_use;
    if (SUCCEEDED(voice->GetVoice(&in_use)) && in_use != nullptr) {
      for (std::size_t i = 0; i < found.size(); ++i) {
        if (found[i].IsEqualObject(in_use)) {
          current = i;
          break;
        }
      }
    }
    tokens = std::move(found);
    active_voice = current;
    {
      std::scoped_lock lock(state_mutex);
      voice_names = std::move(names);
    }
    return select_voice(current);
  }

  BackendResult<> select_voice(std::size_t index) {
    if (index >= tokens.size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    if (FAILED(voice->SetVoice(tokens[index])))
      return std::unexpected(BackendError::VoiceNotFound);
    active_voice = index;
    format = FALLBACK_FORMAT;
    CComPtr<ISpStreamFormat> output;
    GUID format_id{};
    WAVEFORMATEX *reported = nullptr;
    if (SUCCEEDED(voice->SetOutput(nullptr, TRUE)) &&
        SUCCEEDED(voice->GetOutputStream(&output)) && output != nullptr &&
        SUCCEEDED(output->GetFormat(&format_id, &reported)) &&
        reported != nullptr && reported->wFormatTag == WAVE_FORMAT_PCM)
      format = *reported;
    if (reported != nullptr)
      CoTaskMemFree(reported);
    format.cbSize = 0;
    std::scoped_lock lock(state_mutex);
    published_voice = index;
    audio_channels = format.nChannels;
    audio_sample_rate = format.nSamplesPerSec;
    audio_bit_depth = format.wBitsPerSample;
    return {};
  }

  void apply(const Prosody &settings) {
    if (settings.voice != active_voice)
      if (const auto selected = select_voice(settings.voice); !selected)
        logger.warn("Could not select voice {} ({})", settings.voice,
                    std::to_underlying(selected.error()));
    (void)voice->SetRate(settings.rate);
  }

  BackendResult<std::vector<float>>
  render_samples(const std::wstring &text, const Prosody &settings,
                 const SpeechPlayer *cancelled_by, std::uint64_t generation) {
    apply(settings);
    auto *capture = new CaptureStream(cancelled_by, generation);
    CComPtr<IStream> capture_stream;
    capture_stream.Attach(capture);
    CComPtr<ISpStream> stream;
    if (FAILED(stream.CoCreateInstance(CLSID_SpStream)) ||
        FAILED(stream->SetBaseStream(capture_stream, SPDFID_WaveFormatEx,
                                     &format)) ||
        FAILED(voice->SetOutput(stream, FALSE)))
      return std::unexpected(BackendError::InternalBackendError);
    const HRESULT hr =
        voice->Speak(text.c_str(), speak_flags(settings.pitch), nullptr);
    if (FAILED(hr))
      return std::unexpected(hr == E_ABORT ? BackendError::NotSpeaking
                                           : BackendError::SpeakFailure);
    const std::size_t channels = format.nChannels;
    const std::size_t block_align = format.nBlockAlign;
    const std::size_t bit_depth = format.wBitsPerSample;
    if (channels == 0 || block_align == 0 || bit_depth % 8 != 0)
      return std::unexpected(BackendError::InvalidAudioFormat);
    const auto bytes = capture->bytes();
    const std::size_t total_samples = bytes.size() / block_align * channels;
    const auto *data = reinterpret_cast<const std::uint8_t *>(bytes.data());
    std::vector<float> samples(total_samples);
    switch (bit_depth) {
    case 8:
      drwav_u8_to_f32(samples.data(), data, total_samples);
      break;
    case 16:
      drwav_s16_to_f32(samples.data(),
                       lifetime_as_array<drwav_int16>(data, total_samples),
                       total_samples);
      break;
    case 24:
      drwav_s24_to_f32(samples.data(), data, total_samples);
      break;
    case 32:
      drwav_s32_to_f32(samples.data(),
                       lifetime_as_array<drwav_int32>(data, total_samples),
                       total_samples);
      break;
    default:
      return std::unexpected(BackendError::InvalidAudioFormat);
    }
    return samples;
  }

  void render(const SpeakJob &job) {
    auto samples =
        render_samples(job.text, job.prosody, &player, job.generation);
    if (!samples) {
      if (samples.error() != BackendError::NotSpeaking)
        logger.error("Could not render an utterance ({})",
                     std::to_underlying(samples.error()));
      return;
    }
    player.play(job.generation, std::move(*samples), format.nChannels,
                format.nSamplesPerSec);
  }

  BackendResult<> render_to_memory(const MemoryJob &job) {
    auto samples = render_samples(job.text, job.prosody, nullptr, 0);
    if (!samples)
      return std::unexpected(samples.error());
    auto trimmed = trim_silence_rms_gate_inplace(
        std::span<float>(*samples), format.nChannels, format.nSamplesPerSec);
    for (float &sample : trimmed.view)
      sample = std::clamp(sample * job.volume, -1.0F, 1.0F);
    (*job.callback)(job.userdata, trimmed.view.data(), trimmed.view.size(),
                    format.nChannels, format.nSamplesPerSec);
    return {};
  }

  void run(Job &job) {
    std::visit(Overloaded{[this](SpeakJob &speak) { render(speak); },
                          [this](MemoryJob &memory) {
                            BackendResult<> result = std::unexpected(
                                BackendError::InternalBackendError);
                            try {
                              result = render_to_memory(memory);
                            } catch (const std::bad_alloc &) {
                              result =
                                  std::unexpected(BackendError::MemoryFailure);
                            }
                            memory.done->set_value(result);
                          },
                          [this](SetVoiceJob &set) {
                            set.done->set_value(select_voice(set.voice));
                          },
                          [this](RefreshVoicesJob &refresh) {
                            refresh.done->set_value(enumerate_voices());
                          }},
               job);
  }

  void worker_proc(const std::stop_token &stop,
                   std::promise<BackendResult<>> &started) {
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const ComApartmentGuard apartment{hr};
    if (FAILED(hr) || FAILED(voice.CoCreateInstance(CLSID_SpVoice))) {
      started.set_value(std::unexpected(BackendError::BackendNotAvailable));
      return;
    }
    started.set_value(enumerate_voices());
    while (true) {
      Job job;
      {
        std::unique_lock lock(state_mutex);
        if (!jobs_cv.wait(lock, stop, [this] { return !jobs.empty(); }))
          break;
        job = std::move(jobs.front());
        jobs.pop_front();
        if (const auto *speak = std::get_if<SpeakJob>(&job))
          rendering_generation = speak->generation;
      }
      run(job);
      std::scoped_lock lock(state_mutex);
      rendering_generation = 0;
    }
    std::scoped_lock lock(state_mutex);
    for (auto &pending : jobs) {
      std::visit(Overloaded{[](SpeakJob &) {},
                            [](auto &request) {
                              request.done->set_value(std::unexpected(
                                  BackendError::NotInitialized));
                            }},
                 pending);
    }
    jobs.clear();
    tokens.clear();
    voice.Release();
  }

  BackendResult<> call(Job job, std::promise<BackendResult<>> &done) {
    auto result = done.get_future();
    {
      std::scoped_lock lock(state_mutex);
      jobs.push_back(std::move(job));
    }
    jobs_cv.notify_one();
    return result.get();
  }

  [[nodiscard]] bool has_speech_locked() const {
    const auto current = player.generation();
    return rendering_generation == current || player.has_audio() ||
           std::ranges::any_of(jobs, [current](const Job &job) {
             const auto *speak = std::get_if<SpeakJob>(&job);
             return speak != nullptr && speak->generation == current;
           });
  }

  void discard() {
    std::scoped_lock lock(state_mutex);
    player.stop();
    std::erase_if(jobs, [](const Job &job) {
      return std::holds_alternative<SpeakJob>(job);
    });
    paused = false;
  }

public:
  ~SapiBackend() override {
    if (worker.joinable()) {
      {
        std::scoped_lock lock(state_mutex);
        player.stop();
      }
      worker.request_stop();
      worker.join();
    }
  }

  [[nodiscard]] std::string_view get_name() const override { return "SAPI"; }

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
    IClassFactory *factory = nullptr;
    const HRESULT hr = CoGetClassObject(
        CLSID_SpVoice, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, nullptr,
        IID_IClassFactory, reinterpret_cast<void **>(&factory));
    if (SUCCEEDED(hr) && factory != nullptr) {
      factory->Release();
      features |= IS_SUPPORTED_AT_RUNTIME;
    }
    return features;
  }

  BackendResult<> initialize() override {
    if (ready)
      return std::unexpected(BackendError::AlreadyInitialized);
    if (auto opened = player.open(); !opened)
      return opened;
    std::promise<BackendResult<>> started;
    auto result = started.get_future();
    worker = std::jthread([this, &started](const std::stop_token &stop) {
      worker_proc(stop, started);
    });
    if (auto outcome = result.get(); !outcome) {
      worker.request_stop();
      worker.join();
      return outcome;
    }
    {
      std::scoped_lock lock(state_mutex);
      prosody.voice = published_voice;
    }
    ready = true;
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    auto wtext = make_text(text, prosody.pitch);
    if (!wtext)
      return std::unexpected(wtext.error());
    if (interrupt)
      discard();
    {
      std::scoped_lock lock(state_mutex);
      jobs.emplace_back(SpeakJob{.text = std::move(*wtext),
                                 .prosody = prosody,
                                 .generation = player.generation()});
    }
    jobs_cv.notify_one();
    return {};
  }

  BackendResult<> speak_to_memory(std::string_view text, AudioCallback callback,
                                  void *userdata) override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    auto wtext = make_text(text, prosody.pitch);
    if (!wtext)
      return std::unexpected(wtext.error());
    std::promise<BackendResult<>> done;
    return call(MemoryJob{.text = std::move(*wtext),
                          .prosody = prosody,
                          .volume = volume,
                          .callback = &callback,
                          .userdata = userdata,
                          .done = &done},
                done);
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<bool> is_speaking() override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    std::scoped_lock lock(state_mutex);
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
    std::scoped_lock lock(state_mutex);
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
    std::scoped_lock lock(state_mutex);
    if (!paused)
      return std::unexpected(BackendError::NotPaused);
    player.resume();
    paused = false;
    return {};
  }

  BackendResult<> set_volume(float value) override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    volume = value;
    player.set_volume(value);
    return {};
  }

  BackendResult<float> get_volume() override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    return volume;
  }

  BackendResult<> set_rate(float value) override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    rate = value;
    prosody.rate = static_cast<LONG>(std::lround(
        range_convert_midpoint(value, 0.0, 0.5, 1.0, -10.0, 0.0, 10.0)));
    return {};
  }

  BackendResult<float> get_rate() override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    return rate;
  }

  BackendResult<> set_pitch(float value) override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    pitch = value;
    prosody.pitch = static_cast<LONG>(std::lround(
        range_convert_midpoint(value, 0.0, 0.5, 1.0, -10.0, 0.0, 10.0)));
    return {};
  }

  BackendResult<float> get_pitch() override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    return pitch;
  }

  BackendResult<> refresh_voices() override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    std::promise<BackendResult<>> done;
    auto result = call(RefreshVoicesJob{.done = &done}, done);
    std::scoped_lock lock(state_mutex);
    prosody.voice = published_voice;
    return result;
  }

  BackendResult<std::size_t> count_voices() override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    std::scoped_lock lock(state_mutex);
    return voice_names.size();
  }

  BackendResult<std::string> get_voice_name(std::size_t id) override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    std::scoped_lock lock(state_mutex);
    if (id >= voice_names.size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    return voice_names[id].name;
  }

  BackendResult<std::string> get_voice_language(std::size_t id) override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    std::scoped_lock lock(state_mutex);
    if (id >= voice_names.size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    return voice_names[id].language;
  }

  BackendResult<> set_voice(std::size_t id) override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    {
      std::scoped_lock lock(state_mutex);
      if (id >= voice_names.size())
        return std::unexpected(BackendError::RangeOutOfBounds);
    }
    std::promise<BackendResult<>> done;
    if (auto result = call(SetVoiceJob{.voice = id, .done = &done}, done);
        !result)
      return result;
    prosody.voice = id;
    return {};
  }

  BackendResult<std::size_t> get_voice() override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    return prosody.voice;
  }

  BackendResult<std::size_t> get_channels() override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    std::scoped_lock lock(state_mutex);
    return audio_channels;
  }

  BackendResult<std::size_t> get_sample_rate() override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    std::scoped_lock lock(state_mutex);
    return audio_sample_rate;
  }

  BackendResult<std::size_t> get_bit_depth() override {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    std::scoped_lock lock(state_mutex);
    return audio_bit_depth;
  }
};

REGISTER_BACKEND_WITH_ID(SapiBackend, Backends::SAPI, "SAPI", 97);
#endif
