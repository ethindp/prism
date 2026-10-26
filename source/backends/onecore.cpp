// SPDX-License-Identifier: MPL-2.0

#ifdef _WIN32
#include "../backend.h"
#include "../backend_catalog.h"
#include "../logging.h"
#include "../utils.h"
#include <MemoryBuffer.h>
#include <algorithm>
#include <cmath>
#include <concepts>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <dr_wav/dr_wav.h>
#include <exception>
#include <fmt/format.h>
#include <fmt/xchar.h>
#include <limits>
#include <map>
#include <mutex>
#include <objbase.h>
#include <optional>
#include <roapi.h>
#include <simdutf.h>
#include <span>
#include <tchar.h>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
#include <windows.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Audio.h>
#include <winrt/Windows.Media.Devices.h>
#include <winrt/Windows.Media.MediaProperties.h>
#include <winrt/Windows.Media.Render.h>
#include <winrt/Windows.Media.SpeechSynthesis.h>
#include <winrt/Windows.Media.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/base.h>

using namespace winrt;
using namespace winrt::Windows::Foundation::Metadata;
using namespace winrt::Windows::Media;
using namespace winrt::Windows::Media::Audio;
using namespace winrt::Windows::Media::Devices;
using namespace winrt::Windows::Media::MediaProperties;
using namespace winrt::Windows::Media::Render;
using namespace winrt::Windows::Media::SpeechSynthesis;
using namespace winrt::Windows::Storage::Streams;

[[nodiscard]] static std::string hstring_to_utf8(std::wstring_view w) {
  const auto *src = reinterpret_cast<const char16_t *>(w.data());
  std::string out(simdutf::utf8_length_from_utf16le(src, w.size()), '\0');
  out.resize(simdutf::convert_utf16le_to_utf8(src, w.size(), out.data()));
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

inline bool is_current_thread_sta() noexcept {
  APTTYPE type{};
  APTTYPEQUALIFIER qual{};
  HRESULT hr = CoGetApartmentType(&type, &qual);
  if (FAILED(hr))
    return false;
  return type == APTTYPE_STA || type == APTTYPE_MAINSTA;
}

struct WinrtApartmentGuard {
  HRESULT hr;
  explicit WinrtApartmentGuard(
      RO_INIT_TYPE model = is_current_thread_sta()
                               ? RO_INIT_SINGLETHREADED
                               : RO_INIT_MULTITHREADED) noexcept
      : hr(RoInitialize(model)) {}
  ~WinrtApartmentGuard() noexcept {
    if (SUCCEEDED(hr))
      RoUninitialize();
  }
  WinrtApartmentGuard(const WinrtApartmentGuard &) = delete;
  WinrtApartmentGuard &operator=(const WinrtApartmentGuard &) = delete;
  explicit operator bool() const noexcept { return SUCCEEDED(hr); }
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

template <std::invocable F> auto run_on_mta(F &&fn) -> std::invoke_result_t<F> {
  using R = std::invoke_result_t<F>;
  if (!is_current_thread_sta()) {
    WinrtApartmentGuard apartment{RO_INIT_MULTITHREADED};
    winrt::check_hresult(apartment.hr);
    if constexpr (std::is_void_v<R>) {
      fn();
      return;
    } else {
      return fn();
    }
  }
  MtaEventGuard evt;
  if (!evt)
    throw std::runtime_error("run_on_mta: CreateEvent failed");
  MtaContext<F> ctx{.fn = &fn, .result = {}, .ex = nullptr, .event = evt.h};
  auto const ok = TrySubmitThreadpoolCallback(
      [](PTP_CALLBACK_INSTANCE, void *raw) noexcept {
        auto &c = *static_cast<MtaContext<F> *>(raw);
        try {
          WinrtApartmentGuard apartment{RO_INIT_MULTITHREADED};
          winrt::check_hresult(apartment.hr);
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

namespace {
constexpr float DEFAULT_PARAMETER = 0.5F;
constexpr float DEFAULT_VOLUME = 1.0F;

struct DecodedAudio {
  std::vector<float> samples;
  std::uint32_t channels{};
  std::uint32_t sample_rate{};
  std::uint32_t bit_depth{};
};

std::vector<std::uint8_t> read_stream(const SpeechSynthesisStream &stream) {
  if (stream.ContentType() != _T("audio/wav"))
    throw std::runtime_error("synthesized stream is not audio/wav");
  const auto size = stream.Size();
  if (size > std::numeric_limits<std::uint32_t>::max())
    throw std::length_error("synthesized stream is too large");
  const auto capacity = static_cast<std::uint32_t>(size);
  std::vector<std::uint8_t> bytes;
  bytes.reserve(capacity);
  stream.Seek(0);
  while (bytes.size() < capacity) {
    const auto wanted = capacity - static_cast<std::uint32_t>(bytes.size());
    const auto chunk =
        stream.ReadAsync(Buffer(wanted), wanted, InputStreamOptions::None)
            .get();
    if (chunk.Length() == 0)
      break;
    bytes.insert(bytes.end(), chunk.data(), chunk.data() + chunk.Length());
  }
  return bytes;
}

std::optional<DecodedAudio> decode_wave(std::span<const std::uint8_t> bytes) {
  drwav wav{};
  if (drwav_init_memory(&wav, bytes.data(), bytes.size(), nullptr) == 0)
    return std::nullopt;
  DecodedAudio audio{
      .samples = {},
      .channels = wav.channels,
      .sample_rate = wav.sampleRate,
      .bit_depth = wav.bitsPerSample,
  };
  if (wav.channels == 0 || wav.sampleRate == 0) {
    drwav_uninit(&wav);
    return std::nullopt;
  }
  audio.samples.resize(static_cast<std::size_t>(wav.totalPCMFrameCount) *
                       wav.channels);
  const auto frames = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount,
                                                audio.samples.data());
  audio.samples.resize(static_cast<std::size_t>(frames) * wav.channels);
  drwav_uninit(&wav);
  return audio;
}

AudioFrame make_frame(std::span<const float> samples) {
  const auto bytes = static_cast<std::uint32_t>(samples.size_bytes());
  AudioFrame frame(bytes);
  {
    const AudioBuffer buffer = frame.LockBuffer(AudioBufferAccessMode::Write);
    const auto reference = buffer.CreateReference();
    const auto access =
        reference.as<::Windows::Foundation::IMemoryBufferByteAccess>();
    BYTE *data = nullptr;
    UINT32 capacity = 0;
    check_hresult(access->GetBuffer(&data, &capacity));
    if (capacity < bytes)
      throw std::length_error("audio frame is smaller than requested");
    std::memcpy(data, samples.data(), bytes);
    buffer.Length(bytes);
    reference.Close();
    buffer.Close();
  }
  return frame;
}

struct SpeechRequest {
  hstring text;
  double rate;
  double pitch;
  VoiceInformation voice{nullptr};
  std::uint64_t generation;
};

struct QueuedFrame {
  AudioFrameInputNode node{nullptr};
  AudioFrame frame{nullptr};
};
} // namespace

class OneCoreBackend final : public ComTextToSpeechBackend {
private:
  SpeechSynthesizer synth{nullptr};
  SpeechSynthesizer worker_synth{nullptr};

  float volume = DEFAULT_VOLUME;
  float rate = DEFAULT_PARAMETER;
  float pitch = DEFAULT_PARAMETER;
  bool ready = false;

  std::size_t cached_channels = 0;
  std::size_t cached_sample_rate = 0;
  std::size_t cached_bit_depth = 0;
  bool format_cached = false;

  std::jthread worker;

  std::mutex state_mutex;
  std::condition_variable_any request_cv;
  std::deque<SpeechRequest> requests;
  std::uint64_t synthesizing_generation = 0;
  std::uint64_t generation = 1;
  bool paused = false;
  AudioGraph graph{nullptr};
  AudioDeviceOutputNode output_node{nullptr};
  std::map<std::pair<std::uint32_t, std::uint32_t>, AudioFrameInputNode> nodes;
  AudioFrameInputNode active_node{nullptr};
  std::deque<AudioFrame> in_flight;
  std::deque<QueuedFrame> waiting;
  bool graph_stale = false;
  event_token device_changed_token{};

  LogSource logger{"OneCore"};

  [[nodiscard]] bool has_speech_locked() const noexcept {
    return !in_flight.empty() || !waiting.empty() || !requests.empty() ||
           synthesizing_generation == generation;
  }

  void pump_locked() {
    while (!waiting.empty()) {
      auto &next = waiting.front();
      if (next.node != active_node) {
        if (!in_flight.empty())
          return;
        active_node = next.node;
        active_node.OutgoingGain(volume);
        if (paused)
          active_node.Stop();
        else
          active_node.Start();
      }
      active_node.AddFrame(next.frame);
      in_flight.push_back(std::move(next.frame));
      waiting.pop_front();
    }
  }

  void on_frame_completed(const AudioFrameInputNode &node,
                          const AudioFrameCompletedEventArgs &args) {
    std::scoped_lock lock(state_mutex);
    if (node != active_node || in_flight.empty() ||
        in_flight.front() != args.Frame())
      return;
    in_flight.pop_front();
    pump_locked();
  }

  [[nodiscard]] AudioFrameInputNode discard_locked() {
    ++generation;
    requests.clear();
    waiting.clear();
    in_flight.clear();
    paused = false;
    if (!active_node)
      return nullptr;
    active_node.Stop();
    std::erase_if(nodes, [this](const auto &entry) {
      return entry.second == active_node;
    });
    return std::exchange(active_node, nullptr);
  }

  void close_node(const AudioFrameInputNode &node) const noexcept {
    if (!node)
      return;
    try {
      node.Close();
    } catch (...) {
      logger.debug("A silenced frame input node could not be closed");
    }
  }

  [[nodiscard]] AudioGraph build_graph_locked() {
    AudioGraphSettings settings(AudioRenderCategory::Speech);
    const auto created = AudioGraph::CreateAsync(settings).get();
    if (created.Status() != AudioGraphCreationStatus::Success)
      throw hresult_error(created.ExtendedError(),
                          _T("AudioGraph could not be created"));
    AudioGraph next = created.Graph();
    const auto device = next.CreateDeviceOutputNodeAsync().get();
    if (device.Status() != AudioDeviceNodeCreationStatus::Success)
      throw hresult_error(device.ExtendedError(),
                          _T("AudioGraph output node could not be created"));
    next.UnrecoverableErrorOccurred(
        [this](const AudioGraph &failed,
               const AudioGraphUnrecoverableErrorOccurredEventArgs &args) {
          std::scoped_lock lock(state_mutex);
          if (failed != graph)
            return;
          logger.error("AudioGraph failed ({}); speech in progress is lost",
                       static_cast<int>(args.Error()));
          ++generation;
          waiting.clear();
          in_flight.clear();
          paused = false;
          graph_stale = true;
        });
    AudioGraph old = std::exchange(graph, std::move(next));
    output_node = device.DeviceOutputNode();
    nodes.clear();
    active_node = nullptr;
    graph_stale = false;
    graph.Start();
    return old;
  }

  AudioFrameInputNode node_for_locked(std::uint32_t sample_rate,
                                      std::uint32_t channels) {
    const auto key = std::pair{sample_rate, channels};
    if (const auto it = nodes.find(key); it != nodes.end())
      return it->second;
    auto properties =
        AudioEncodingProperties::CreatePcm(sample_rate, channels, 32);
    properties.Subtype(_T("Float"));
    auto node = graph.CreateFrameInputNode(properties);
    node.AddOutgoingConnection(output_node);
    node.Stop();
    node.AudioFrameCompleted([this](const AudioFrameInputNode &sender,
                                    const AudioFrameCompletedEventArgs &args) {
      on_frame_completed(sender, args);
    });
    nodes.emplace(key, node);
    return node;
  }

  static void apply_options(const SpeechSynthesizer &target, double rate_value,
                            double pitch_value) {
    target.Options().SpeakingRate(rate_value);
    target.Options().AudioPitch(pitch_value);
  }

  void play(const SpeechRequest &request) {
    if (request.voice && worker_synth.Voice().Id() != request.voice.Id())
      worker_synth.Voice(request.voice);
    apply_options(worker_synth, request.rate, request.pitch);
    const auto stream =
        worker_synth.SynthesizeTextToStreamAsync(request.text).get();
    const auto bytes = read_stream(stream);
    auto audio = decode_wave(bytes);
    if (!audio) {
      logger.error("Could not decode synthesized speech");
      return;
    }
    const auto trimmed = trim_silence_rms_gate_inplace(
        std::span<float>(audio->samples), audio->channels, audio->sample_rate);
    if (trimmed.view.empty())
      return;
    const AudioFrame frame = make_frame(trimmed.view);
    AudioGraph replaced{nullptr};
    {
      std::scoped_lock lock(state_mutex);
      if (request.generation != generation)
        return;
      if (graph_stale && in_flight.empty() && waiting.empty())
        replaced = build_graph_locked();
      waiting.push_back(QueuedFrame{
          .node = node_for_locked(audio->sample_rate, audio->channels),
          .frame = frame,
      });
      pump_locked();
    }
    if (replaced)
      replaced.Close();
  }

  void worker_proc(const std::stop_token &stop) {
    WinrtApartmentGuard apartment{RO_INIT_MULTITHREADED};
    if (!apartment)
      return;
    while (true) {
      SpeechRequest request;
      {
        std::unique_lock lock(state_mutex);
        if (!request_cv.wait(lock, stop, [this] { return !requests.empty(); }))
          return;
        request = std::move(requests.front());
        requests.pop_front();
        synthesizing_generation = request.generation;
      }
      try {
        play(request);
      } catch (const hresult_error &e) {
        logger.error("Could not speak an utterance: {}", e.message());
      } catch (const std::exception &e) {
        logger.error("Could not speak an utterance: {}", e.what());
      }
      std::scoped_lock lock(state_mutex);
      synthesizing_generation = 0;
    }
  }

  void reset_state() noexcept {
    if (worker.joinable()) {
      worker.request_stop();
      worker.join();
    }
    if (device_changed_token) {
      try {
        MediaDevice::DefaultAudioRenderDeviceChanged(device_changed_token);
      } catch (...) {
        logger.debug("Could not stop watching for output device changes");
      }
      device_changed_token = {};
    }
    AudioGraph closing{nullptr};
    {
      std::scoped_lock lock(state_mutex);
      requests.clear();
      waiting.clear();
      in_flight.clear();
      nodes.clear();
      active_node = nullptr;
      output_node = nullptr;
      closing = std::exchange(graph, nullptr);
    }
    if (closing) {
      try {
        closing.Close();
      } catch (...) {
        logger.debug("The audio graph could not be closed");
      }
    }
    synth = nullptr;
    worker_synth = nullptr;
    ready = false;
  }

  BackendResult<> cache_audio_format() try {
    if (format_cached)
      return {};
    const auto bytes = run_on_mta([&] {
      return read_stream(synth.SynthesizeTextToStreamAsync(_T(" ")).get());
    });
    drwav wav{};
    if (drwav_init_memory(&wav, bytes.data(), bytes.size(), nullptr) == 0) {
      logger.error("cache_audio_format: WAV parse error");
      return std::unexpected(BackendError::InvalidAudioFormat);
    }
    cached_channels = wav.channels;
    cached_sample_rate = wav.sampleRate;
    cached_bit_depth = wav.bitsPerSample;
    format_cached = true;
    drwav_uninit(&wav);
    return {};
  } catch (const hresult_error &e) {
    logger.error("cache_audio_format failed: {}", e.message());
    return std::unexpected(BackendError::InternalBackendError);
  } catch (const std::exception &e) {
    logger.error("cache_audio_format failed: {}", e.what());
    return std::unexpected(BackendError::InternalBackendError);
  }

  template <class F>
  auto guarded(std::string_view operation, F &&body) -> decltype(body()) try {
    if (!ready)
      return std::unexpected(BackendError::NotInitialized);
    return body();
  } catch (const hresult_error &e) {
    logger.error("{} failed: {}", operation, e.message());
    return std::unexpected(BackendError::InternalBackendError);
  } catch (const std::bad_alloc &) {
    return std::unexpected(BackendError::MemoryFailure);
  } catch (const std::exception &e) {
    logger.error("{} failed: {}", operation, e.what());
    return std::unexpected(BackendError::InternalBackendError);
  }

  static BackendResult<VoiceInformation> voice_at(std::size_t id) {
    const auto voices = SpeechSynthesizer::AllVoices();
    if (id >= voices.Size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    return voices.GetAt(static_cast<std::uint32_t>(id));
  }

public:
  ~OneCoreBackend() override { reset_state(); }

  [[nodiscard]] std::string_view get_name() const override { return "OneCore"; }

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
    try {
      if (ApiInformation::IsTypePresent(
              _T("Windows.Media.SpeechSynthesis.SpeechSynthesizer")) &&
          ApiInformation::IsTypePresent(_T("Windows.Media.Audio.AudioGraph")))
        features |= IS_SUPPORTED_AT_RUNTIME;
    } catch (...) {
      logger.debug("Could not ask whether the speech and audio APIs exist");
    }
    return features;
  }

  BackendResult<> initialize() override try {
    if (ready)
      return std::unexpected(BackendError::AlreadyInitialized);
    if (!ApiInformation::IsTypePresent(
            _T("Windows.Media.SpeechSynthesis.SpeechSynthesizer")) ||
        !ApiInformation::IsTypePresent(_T("Windows.Media.Audio.AudioGraph")))
      return std::unexpected(BackendError::BackendNotAvailable);
    run_on_mta([this] {
      synth = SpeechSynthesizer();
      worker_synth = SpeechSynthesizer();
      for (const auto &target : {synth, worker_synth}) {
        target.Options().AppendedSilence(SpeechAppendedSilence::Min);
        target.Options().PunctuationSilence(SpeechPunctuationSilence::Min);
      }
      std::scoped_lock lock(state_mutex);
      (void)build_graph_locked();
    });
    device_changed_token = MediaDevice::DefaultAudioRenderDeviceChanged(
        [this](const auto &,
               const DefaultAudioRenderDeviceChangedEventArgs &args) {
          if (args.Role() != AudioDeviceRole::Default)
            return;
          std::scoped_lock lock(state_mutex);
          graph_stale = true;
        });
    worker = std::jthread(
        [this](const std::stop_token &stop) { worker_proc(stop); });
    ready = true;
    if (const auto format_result = cache_audio_format(); !format_result)
      logger.warn(
          "Could not cache OneCore audio format during initialization: {}",
          std::to_underlying(format_result.error()));
    return {};
  } catch (const hresult_error &e) {
    logger.error("Could not initialize OneCore backend: {}", e.message());
    reset_state();
    return std::unexpected(BackendError::BackendNotAvailable);
  } catch (const std::exception &e) {
    logger.error("Could not initialize OneCore backend: {}", e.what());
    reset_state();
    return std::unexpected(BackendError::InternalBackendError);
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    return guarded("speak", [&]() -> BackendResult<> {
      SpeechRequest request{
          .text = to_hstring(text),
          .rate = exp_range_convert(rate, 0.5, 1.0, 6.0),
          .pitch =
              range_convert(static_cast<double>(pitch), 0.0, 1.0, 0.0, 2.0),
          .voice = synth.Voice(),
          .generation = 0,
      };
      AudioFrameInputNode silenced{nullptr};
      {
        std::scoped_lock lock(state_mutex);
        if (interrupt)
          silenced = discard_locked();
        request.generation = generation;
        requests.push_back(std::move(request));
      }
      request_cv.notify_one();
      close_node(silenced);
      return {};
    });
  }

  BackendResult<> speak_to_memory(std::string_view text, AudioCallback callback,
                                  void *userdata) override {
    return guarded("speak_to_memory", [&]() -> BackendResult<> {
      const auto wide = to_hstring(text);
      apply_options(
          synth, exp_range_convert(rate, 0.5, 1.0, 6.0),
          range_convert(static_cast<double>(pitch), 0.0, 1.0, 0.0, 2.0));
      const auto bytes = run_on_mta([&] {
        return read_stream(synth.SynthesizeTextToStreamAsync(wide).get());
      });
      auto audio = decode_wave(bytes);
      if (!audio)
        return std::unexpected(BackendError::InvalidAudioFormat);
      auto trimmed =
          trim_silence_rms_gate_inplace(std::span<float>(audio->samples),
                                        audio->channels, audio->sample_rate);
      for (float &sample : trimmed.view)
        sample = std::clamp(sample * volume, -1.0F, 1.0F);
      callback(userdata, trimmed.view.data(), trimmed.view.size(),
               audio->channels, audio->sample_rate);
      return {};
    });
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
    return guarded("stop", [&]() -> BackendResult<> {
      AudioFrameInputNode silenced{nullptr};
      {
        std::scoped_lock lock(state_mutex);
        silenced = discard_locked();
      }
      close_node(silenced);
      return {};
    });
  }

  BackendResult<> pause() override {
    return guarded("pause", [&]() -> BackendResult<> {
      std::scoped_lock lock(state_mutex);
      if (paused)
        return std::unexpected(BackendError::AlreadyPaused);
      if (!has_speech_locked())
        return std::unexpected(BackendError::NotSpeaking);
      if (active_node)
        active_node.Stop();
      paused = true;
      return {};
    });
  }

  BackendResult<> resume() override {
    return guarded("resume", [&]() -> BackendResult<> {
      std::scoped_lock lock(state_mutex);
      if (!paused)
        return std::unexpected(BackendError::NotPaused);
      paused = false;
      if (active_node)
        active_node.Start();
      pump_locked();
      return {};
    });
  }

  BackendResult<> set_volume(float value) override {
    return guarded("set_volume", [&]() -> BackendResult<> {
      std::scoped_lock lock(state_mutex);
      volume = value;
      for (const auto &[format, node] : nodes)
        node.OutgoingGain(value);
      return {};
    });
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
    return {};
  }

  BackendResult<std::size_t> count_voices() override {
    return guarded("count_voices", [&]() -> BackendResult<std::size_t> {
      return SpeechSynthesizer::AllVoices().Size();
    });
  }

  BackendResult<std::string> get_voice_name(std::size_t id) override {
    return guarded("get_voice_name", [&]() -> BackendResult<std::string> {
      return voice_at(id).transform([](const VoiceInformation &voice) {
        return to_string(voice.DisplayName());
      });
    });
  }

  BackendResult<std::string> get_voice_language(std::size_t id) override {
    return guarded("get_voice_language", [&]() -> BackendResult<std::string> {
      return voice_at(id).transform([](const VoiceInformation &voice) {
        return to_string(voice.Language());
      });
    });
  }

  BackendResult<> set_voice(std::size_t id) override {
    return guarded("set_voice", [&]() -> BackendResult<> {
      const auto voice = voice_at(id);
      if (!voice)
        return std::unexpected(voice.error());
      const auto old = synth.Voice();
      synth.Voice(*voice);
      format_cached = false;
      if (const auto res = cache_audio_format(); !res) {
        logger.warn("Could not retrieve voice audio format when setting voice "
                    "to ID {}; reverting update",
                    id);
        synth.Voice(old);
        format_cached = false;
        return std::unexpected(BackendError::InternalBackendError);
      }
      return {};
    });
  }

  BackendResult<std::size_t> get_voice() override {
    return guarded("get_voice", [&]() -> BackendResult<std::size_t> {
      const auto voices = SpeechSynthesizer::AllVoices();
      const auto current = synth.Voice().Id();
      for (std::uint32_t i = 0; i < voices.Size(); ++i) {
        if (voices.GetAt(i).Id() == current)
          return i;
      }
      return std::unexpected(BackendError::VoiceNotFound);
    });
  }

  BackendResult<std::size_t> get_channels() override {
    return guarded("get_channels", [&]() -> BackendResult<std::size_t> {
      return cache_audio_format().transform([this] { return cached_channels; });
    });
  }

  BackendResult<std::size_t> get_sample_rate() override {
    return guarded("get_sample_rate", [&]() -> BackendResult<std::size_t> {
      return cache_audio_format().transform(
          [this] { return cached_sample_rate; });
    });
  }

  BackendResult<std::size_t> get_bit_depth() override {
    return guarded("get_bit_depth", [&]() -> BackendResult<std::size_t> {
      return cache_audio_format().transform(
          [this] { return cached_bit_depth; });
    });
  }
};

REGISTER_BACKEND_WITH_ID(OneCoreBackend, Backends::OneCore, "OneCore", 98);
#endif
