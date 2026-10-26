// SPDX-License-Identifier: MPL-2.0

#pragma once
#ifdef _WIN32
#include "backend.h"
#include "logging.h"
#include <atlbase.h>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>
#include <xaudio2.h>

class SpeechPlayer {
private:
  struct Utterance;
  class EngineCallback;
  using Utterances = std::list<Utterance>;

  void on_buffer_end(Utterance &utterance) noexcept;
  void on_critical_error(HRESULT error) noexcept;
  void start_head_locked() noexcept;
  void retire_all_locked() noexcept;
  BackendResult<> create_engine();
  void release_engine() noexcept;
  [[nodiscard]] bool default_device_changed() const;
  void janitor_proc(const std::stop_token &stop);

  const LogSource logger{"SpeechPlayer"};
  mutable std::mutex mutex;
  std::condition_variable_any janitor_cv;
  std::condition_variable_any graveyard_cv;
  CComPtr<IXAudio2> engine;
  IXAudio2MasteringVoice *mastering = nullptr;
  std::unique_ptr<EngineCallback> engine_callback;
  std::wstring engine_device;
  bool engine_failed = false;
  std::uint64_t current_generation = 1;
  bool paused = false;
  float volume = 1.0F;
  Utterances queue;
  Utterances graveyard;
  Utterances dying;
  bool janitor_busy = false;
  std::jthread janitor;

public:
  SpeechPlayer();
  ~SpeechPlayer();
  SpeechPlayer(const SpeechPlayer &) = delete;
  SpeechPlayer &operator=(const SpeechPlayer &) = delete;

  BackendResult<> open();

  [[nodiscard]] std::uint64_t generation() const;

  void play(std::uint64_t request_generation, std::vector<float> samples,
            std::size_t channels, std::size_t sample_rate);

  void stop();
  void pause();
  void resume();
  [[nodiscard]] bool has_audio() const;
  void set_volume(float value);
};
#endif
