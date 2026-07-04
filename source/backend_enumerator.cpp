// SPDX-License-Identifier: MPL-2.0

#include "backend_enumerator.h"
#include "backend.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <stop_token>
#include <utility>
#ifdef _WIN32
#include <objbase.h>
#endif

namespace {
constexpr std::uint32_t default_interval = 1000;
constexpr std::uint32_t default_debounce = 2;

inline PrismBackendId to_prism_id(BackendId id) noexcept {
  return static_cast<PrismBackendId>(static_cast<std::uint64_t>(id));
}
} // namespace

BackendEnumerator::BackendEnumerator(FrozenRegistry *registry,
                                     PrismAvailabilityCallback callback,
                                     void *userdata,
                                     std::uint32_t poll_interval_ms,
                                     std::uint32_t debounce_samples,
                                     std::uint32_t backoff_max_ms,
                                     bool auto_power_manage)
    : registry(registry), callback(callback), userdata(userdata),
      interval_ms(poll_interval_ms == 0 ? default_interval : poll_interval_ms),
      debounce(debounce_samples == 0 ? default_debounce : debounce_samples),
      backoff_max_ms(backoff_max_ms), auto_power_manage(auto_power_manage) {
      logger.info("Initializing");
      logger.debug("Retaining registry");
  registry->retain();
  const std::size_t n = registry->count();
  logger.debug("Found {} backends to scan", n);
  logger.trace("Reallocing instance vector");
  instances.resize(n);
  logger.trace("Reallocing confirmed list");
  confirmed.assign(n, 0);
  logger.trace("Reallocing streak list");
  streak.assign(n, 0);
  logger.trace("Creating poll waiter");
  waiter = PollWaiter::create();
  logger.trace("Launching scanner thread");
  thread = std::jthread([this](const std::stop_token &stop) { run(stop); });
#if defined(PRISM_ENABLE_POWER_MANAGEMENT)
  if (auto_power_manage) {
  logger.trace("Creating power notifier");
    power_notifier =
        PowerNotifier::create([this] { pause(); }, [this] { resume(); });
#endif
  }
}

BackendEnumerator::~BackendEnumerator() {
logger.info("Uninitializing");
#if defined(PRISM_ENABLE_POWER_MANAGEMENT)
logger.trace("Destroying power notifier");
  power_notifier.reset();
#endif
logger.trace("Requesting scan thread to stop");
  thread.request_stop();
  if (waiter) {
  logger.trace("Waking waiter");
    waiter->wake();
    }
  if (thread.joinable()) {
  logger.trace("Joining to scan thread");
    thread.join();
    }
    logger.trace("Releasing registry");
  registry->release();
  logger.info("Uninitialization complete");
}

void BackendEnumerator::pause() {
logger.info("Pause requested");
logger.trace("Acquiring mutex");
  {
    std::lock_guard lock(mtx);
    paused = true;
    logger.trace("Releasing mutex");
  }
  if (waiter) {
  logger.trace("Waking waiter");
    waiter->wake();
    }
}

void BackendEnumerator::resume() {
logger.info("Resume requested");
logger.trace("Acquiring mutex");
  {
    std::lock_guard lock(mtx);
    paused = false;
    logger.trace("Releasing mutex");
  }
  if (waiter) {
  logger.trace("Waking waiter");
    waiter->wake();
    }
}

void BackendEnumerator::run(const std::stop_token &stop) {
logger.debug("Beginning scan thread execution");
#ifdef _WIN32
logger.debug("Initializing COM with flags {}", COINIT_APARTMENTTHREADED | COINIT_SPEED_OVER_MEMORY);
  const bool com_ok = SUCCEEDED(CoInitializeEx(
      nullptr, COINIT_APARTMENTTHREADED | COINIT_SPEED_OVER_MEMORY));
#endif
  std::stop_callback on_stop(stop, [this] {
  logger.info("Scan stop requested");
    if (waiter) {
    logger.trace("Waking waiter");
      waiter->wake();
      }
  });
  logger.debug("Beginning priming scan");
  poll_once(SweepMode::Prime);
  const std::uint32_t base = interval_ms;
  const std::uint32_t cap = backoff_max_ms > base ? backoff_max_ms : base;
  std::uint32_t interval = base;
  while (!stop.stop_requested()) {
    bool paused_snapshot;
    {
      std::lock_guard lock(mtx);
      paused_snapshot = paused;
    }
    if (paused_snapshot) {
      waiter->wait(std::nullopt, std::chrono::milliseconds{0});
      if (stop.stop_requested())
        break;
      bool still_paused;
      {
        std::lock_guard lock(mtx);
        still_paused = paused;
      }
      if (still_paused)
        continue;
      interval = base;
      poll_once(SweepMode::Resync);
      continue;
    }
    const auto w = waiter->wait(std::chrono::milliseconds{interval},
                                std::chrono::milliseconds{interval / 8});
    if (stop.stop_requested())
      break;
    {
      std::lock_guard lock(mtx);
      if (paused)
        continue;
    }
    if (w == PollWaiter::Wake::Signal)
      continue;
    const bool quiet = poll_once(SweepMode::Normal);
    interval = quiet ? std::min<std::uint64_t>(
                           static_cast<std::uint64_t>(interval) * 2, cap)
                     : base;
  }
  instances.clear();
#ifdef _WIN32
  if (com_ok)
    CoUninitialize();
#endif
}

bool BackendEnumerator::poll_once(const SweepMode mode) {
  const std::size_t n = confirmed.size();
  bool disagreement = false;
  for (std::size_t slot = 0; slot < n; ++slot) {
    if (!instances[slot]) {
      try {
        instances[slot] = registry->create_at(slot);
      } catch (...) {
        instances[slot] = nullptr;
      }
    }
    if (!instances[slot])
      continue;
    bool raw = false;
    try {
      const auto features = instances[slot]->get_features().to_ullong();
      raw = (features & BackendFeature::IS_SUPPORTED_AT_RUNTIME) != 0;
    } catch (...) {
      continue;
    }
    try {
      if (raw) {
        if (const auto res = instances[slot]->initialize();
            !res && res.error() != BackendError::Ok &&
            res.error() != BackendError::AlreadyInitialized) {
          instances[slot] = nullptr;
          raw = false;
        }
      }
    } catch (...) {
      continue;
    }
    const std::uint8_t sample = raw ? 1 : 0;
    if (sample != confirmed[slot])
      disagreement = true;
    switch (mode) {
    case SweepMode::Prime:
      confirmed[slot] = sample;
      streak[slot] = 0;
      continue;
    case SweepMode::Resync:
      streak[slot] = 0;
      if (sample != confirmed[slot]) {
        confirmed[slot] = sample;
        if (callback != nullptr)
          callback(userdata, to_prism_id(registry->id_at(slot)),
                   registry->name_at(slot), raw);
      }
      continue;
    case SweepMode::Normal:
      if (sample == confirmed[slot]) {
        streak[slot] = 0;
        continue;
      }
      if (++streak[slot] >= debounce) {
        confirmed[slot] = sample;
        streak[slot] = 0;
        if (callback != nullptr)
          callback(userdata, to_prism_id(registry->id_at(slot)),
                   registry->name_at(slot), raw);
      }
      continue;
    }
  }
  return !disagreement;
}
