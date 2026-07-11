// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "frozen_registry.h"
#include "logging.h"
#include "poll_waiter.h"
#include "prism.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#if defined(PRISM_ENABLE_POWER_MANAGEMENT)
#include "power_notifier.h"
#endif

class BackendEnumerator {
private:
  enum class SweepMode {
    Prime,
    Normal,
    Resync,
  };

  FrozenRegistry *registry;
  PrismAvailabilityCallback callback;
  void *userdata;
  std::uint32_t interval_ms;
  std::uint32_t debounce;
  std::uint32_t backoff_max_ms;
  [[maybe_unused]] bool auto_power_manage;
  std::vector<std::shared_ptr<TextToSpeechBackend>> instances;
  std::vector<std::uint8_t> confirmed;
  std::vector<std::uint32_t> streak;
  std::mutex mtx;
  bool paused = false;
  std::unique_ptr<PollWaiter> waiter;
#if defined(PRISM_ENABLE_POWER_MANAGEMENT)
  std::unique_ptr<PowerNotifier> power_notifier;
#endif
  std::jthread thread;
  LogSource logger{"Backend Enumerator"};

  void run(const std::stop_token &stop);
  bool poll_once(const SweepMode mode);

public:
  BackendEnumerator(FrozenRegistry *registry,
                    PrismAvailabilityCallback callback, void *userdata,
                    std::uint32_t poll_interval_ms,
                    std::uint32_t debounce_samples,
                    std::uint32_t backoff_max_ms, bool auto_power_manage);
  ~BackendEnumerator();
  BackendEnumerator(const BackendEnumerator &) = delete;
  BackendEnumerator &operator=(const BackendEnumerator &) = delete;
  BackendEnumerator(BackendEnumerator &&) = delete;
  BackendEnumerator &operator=(BackendEnumerator &&) = delete;
  void pause();
  void resume();
};
