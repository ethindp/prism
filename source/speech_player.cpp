// SPDX-License-Identifier: MPL-2.0

#ifdef _WIN32
#include "speech_player.h"
#include "utils.h"
#include <atlbase.h>
#include <span>
#include <utility>
#ifndef PRISM_XBOX
#include <mmdeviceapi.h>
#endif

namespace {
std::wstring default_device_id() {
#ifdef PRISM_XBOX
  return {};
#else
  CComPtr<IMMDeviceEnumerator> enumerator;
  CComPtr<IMMDevice> endpoint;
  CComHeapPtr<WCHAR> id;
  if (FAILED(enumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                         CLSCTX_INPROC_SERVER)) ||
      FAILED(
          enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &endpoint)) ||
      FAILED(endpoint->GetId(&id)))
    return {};
  return std::wstring{id};
#endif
}
} // namespace

struct SpeechPlayer::Utterance final : IXAudio2VoiceCallback {
  SpeechPlayer *player = nullptr;
  IXAudio2SourceVoice *voice = nullptr;
  std::vector<float> samples;
  bool done = false;

  void STDMETHODCALLTYPE OnVoiceProcessingPassStart(
      [[maybe_unused]] UINT32 bytes_required) noexcept override {}
  void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() noexcept override {}
  void STDMETHODCALLTYPE OnStreamEnd() noexcept override {}
  void STDMETHODCALLTYPE
  OnBufferStart([[maybe_unused]] void *context) noexcept override {}
  void STDMETHODCALLTYPE
  OnBufferEnd([[maybe_unused]] void *context) noexcept override {
    player->on_buffer_end(*this);
  }
  void STDMETHODCALLTYPE
  OnLoopEnd([[maybe_unused]] void *context) noexcept override {}
  void STDMETHODCALLTYPE OnVoiceError([[maybe_unused]] void *context,
                                      HRESULT error) noexcept override {
    player->logger.error("XAudio2 voice error: {:#010x}",
                         static_cast<std::uint32_t>(error));
  }
};

class SpeechPlayer::EngineCallback final : public IXAudio2EngineCallback {
private:
  SpeechPlayer &player;

public:
  explicit EngineCallback(SpeechPlayer &owner) : player(owner) {}
  void STDMETHODCALLTYPE OnProcessingPassStart() noexcept override {}
  void STDMETHODCALLTYPE OnProcessingPassEnd() noexcept override {}
  void STDMETHODCALLTYPE OnCriticalError(HRESULT error) noexcept override {
    player.on_critical_error(error);
  }
};

BackendResult<> SpeechPlayer::create_engine() {
  CComPtr<IXAudio2> next;
  HRESULT hr = XAudio2Create(&next, 0, XAUDIO2_USE_DEFAULT_PROCESSOR);
  if (FAILED(hr) || next == nullptr) {
    logger.error("XAudio2Create failed: {:#010x}",
                 static_cast<std::uint32_t>(hr));
    return std::unexpected(BackendError::BackendNotAvailable);
  }
  IXAudio2MasteringVoice *next_mastering = nullptr;
  hr = next->CreateMasteringVoice(&next_mastering, XAUDIO2_DEFAULT_CHANNELS,
                                  XAUDIO2_DEFAULT_SAMPLERATE, 0, nullptr,
                                  nullptr, AudioCategory_Speech);
  if (FAILED(hr) || next_mastering == nullptr) {
    logger.error("CreateMasteringVoice failed: {:#010x}",
                 static_cast<std::uint32_t>(hr));
    return std::unexpected(BackendError::BackendNotAvailable);
  }
  auto callback = std::make_unique<EngineCallback>(*this);
  (void)next->RegisterForCallbacks(callback.get());
  auto device = default_device_id();
  std::scoped_lock lock(mutex);
  engine = std::move(next);
  mastering = next_mastering;
  engine_callback = std::move(callback);
  engine_device = std::move(device);
  engine_failed = false;
  return {};
}

void SpeechPlayer::release_engine() noexcept {
  CComPtr<IXAudio2> old_engine;
  IXAudio2MasteringVoice *old_mastering = nullptr;
  std::unique_ptr<EngineCallback> old_callback;
  {
    std::scoped_lock lock(mutex);
    old_engine.Attach(engine.Detach());
    old_mastering = std::exchange(mastering, nullptr);
    old_callback = std::move(engine_callback);
  }
  if (old_mastering != nullptr)
    old_mastering->DestroyVoice();
  if (old_engine != nullptr)
    old_engine->UnregisterForCallbacks(old_callback.get());
}

bool SpeechPlayer::default_device_changed() const {
  const auto device = default_device_id();
  if (device.empty())
    return false;
  std::scoped_lock lock(mutex);
  return device != engine_device;
}

SpeechPlayer::SpeechPlayer() = default;

SpeechPlayer::~SpeechPlayer() {
  {
    std::scoped_lock lock(mutex);
    retire_all_locked();
  }
  janitor.request_stop();
  if (janitor.joinable())
    janitor.join();
  for (auto &utterance : graveyard)
    utterance.voice->DestroyVoice();
  release_engine();
}

BackendResult<> SpeechPlayer::open() {
  if (auto result = create_engine(); !result)
    return result;
  janitor =
      std::jthread([this](const std::stop_token &stop) { janitor_proc(stop); });
  return {};
}

std::uint64_t SpeechPlayer::generation() const {
  std::scoped_lock lock(mutex);
  return current_generation;
}

void SpeechPlayer::play(std::uint64_t request_generation,
                        std::vector<float> samples, std::size_t channels,
                        std::size_t sample_rate) {
  if (channels == 0 || channels > XAUDIO2_MAX_AUDIO_CHANNELS ||
      sample_rate < XAUDIO2_MIN_SAMPLE_RATE ||
      sample_rate > XAUDIO2_MAX_SAMPLE_RATE) {
    logger.error("Refusing audio with {} channels at {} Hz", channels,
                 sample_rate);
    return;
  }
  bool failed = false;
  bool idle = false;
  {
    std::scoped_lock lock(mutex);
    if (request_generation != current_generation)
      return;
    failed = engine_failed;
    idle = queue.empty();
  }
  if (failed || (idle && default_device_changed())) {
    std::unique_lock lock(mutex);
    retire_all_locked();
    graveyard_cv.wait(lock,
                      [this] { return graveyard.empty() && !janitor_busy; });
    lock.unlock();
    logger.info("Rebuilding the XAudio2 engine for the current output device");
    release_engine();
    if (!create_engine())
      return;
  }
  Utterances staged;
  auto &utterance = staged.emplace_back();
  utterance.player = this;
  utterance.samples = std::move(samples);
  const auto trimmed = trim_silence_rms_gate_inplace(
      std::span<float>(utterance.samples), channels, sample_rate);
  if (trimmed.view.empty() ||
      trimmed.view.size_bytes() > XAUDIO2_MAX_BUFFER_BYTES)
    return;
  WAVEFORMATEX format{};
  format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
  format.nChannels = static_cast<WORD>(channels);
  format.nSamplesPerSec = static_cast<DWORD>(sample_rate);
  format.wBitsPerSample = 32;
  format.nBlockAlign = static_cast<WORD>(channels * sizeof(float));
  format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
  CComPtr<IXAudio2> current_engine;
  {
    std::scoped_lock lock(mutex);
    current_engine = engine;
    if (current_engine == nullptr)
      return;
  }
  HRESULT hr = current_engine->CreateSourceVoice(
      &utterance.voice, &format, 0, XAUDIO2_DEFAULT_FREQ_RATIO, &utterance);
  current_engine.Release();
  if (FAILED(hr) || utterance.voice == nullptr) {
    logger.error("CreateSourceVoice failed: {:#010x}",
                 static_cast<std::uint32_t>(hr));
    return;
  }
  XAUDIO2_BUFFER buffer{};
  buffer.Flags = XAUDIO2_END_OF_STREAM;
  buffer.AudioBytes = static_cast<UINT32>(trimmed.view.size_bytes());
  buffer.pAudioData = reinterpret_cast<const BYTE *>(trimmed.view.data());
  std::scoped_lock lock(mutex);
  hr = utterance.voice->SubmitSourceBuffer(&buffer);
  if (FAILED(hr) || request_generation != current_generation) {
    if (FAILED(hr))
      logger.error("SubmitSourceBuffer failed: {:#010x}",
                   static_cast<std::uint32_t>(hr));
    graveyard.splice(graveyard.end(), staged);
    janitor_cv.notify_one();
    return;
  }
  (void)utterance.voice->SetVolume(volume);
  queue.splice(queue.end(), staged);
  start_head_locked();
}

void SpeechPlayer::on_buffer_end(Utterance &utterance) noexcept {
  std::scoped_lock lock(mutex);
  utterance.done = true;
  bool retired = false;
  while (!queue.empty() && queue.front().done) {
    graveyard.splice(graveyard.end(), queue, queue.begin());
    retired = true;
  }
  if (retired)
    janitor_cv.notify_one();
  start_head_locked();
}

void SpeechPlayer::on_critical_error(HRESULT error) noexcept {
  logger.error("XAudio2 critical error: {:#010x}",
               static_cast<std::uint32_t>(error));
  std::scoped_lock lock(mutex);
  engine_failed = true;
  ++current_generation;
  paused = false;
  retire_all_locked();
}

void SpeechPlayer::start_head_locked() noexcept {
  if (!paused && !queue.empty())
    (void)queue.front().voice->Start(0);
}

void SpeechPlayer::retire_all_locked() noexcept {
  if (queue.empty())
    return;
  for (auto &utterance : queue) {
    (void)utterance.voice->Stop(0);
    (void)utterance.voice->FlushSourceBuffers();
  }
  graveyard.splice(graveyard.end(), queue);
  janitor_cv.notify_one();
}

void SpeechPlayer::stop() {
  std::scoped_lock lock(mutex);
  ++current_generation;
  paused = false;
  retire_all_locked();
}

void SpeechPlayer::pause() {
  std::scoped_lock lock(mutex);
  paused = true;
  if (!queue.empty())
    (void)queue.front().voice->Stop(0);
}

void SpeechPlayer::resume() {
  std::scoped_lock lock(mutex);
  paused = false;
  start_head_locked();
}

bool SpeechPlayer::has_audio() const {
  std::scoped_lock lock(mutex);
  return !queue.empty();
}

void SpeechPlayer::set_volume(float value) {
  std::scoped_lock lock(mutex);
  volume = value;
  for (const auto &utterance : queue)
    (void)utterance.voice->SetVolume(value);
}

void SpeechPlayer::janitor_proc(const std::stop_token &stop) {
  while (true) {
    {
      std::unique_lock lock(mutex);
      if (!janitor_cv.wait(lock, stop, [this] { return !graveyard.empty(); }))
        return;
      dying.splice(dying.end(), graveyard);
      janitor_busy = true;
    }
    for (auto &utterance : dying)
      utterance.voice->DestroyVoice();
    dying.clear();
    {
      std::scoped_lock lock(mutex);
      janitor_busy = false;
    }
    graveyard_cv.notify_all();
  }
}
#endif
