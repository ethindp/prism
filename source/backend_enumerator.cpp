// SPDX-License-Identifier: MPL-2.0

#include "backend_enumerator.h"
#include "backend.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
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
  logger.trace("Retaining registry");
  registry->retain();
  const std::size_t n = registry->count();
  logger.trace("Found {} total backends", n);
  logger.trace("Allocating instances vector");
  instances.resize(n);
  logger.trace("Zeroing confirmed and streak vectors");
  confirmed.assign(n, 0);
  streak.assign(n, 0);
  logger.debug("Instantiating poll waiter");
  waiter = PollWaiter::create();
  logger.debug("Spawning enumerator thread");
  thread = std::jthread([this](const std::stop_token &stop) { run(stop); });
  std::ostringstream os;
  os << thread.get_id();
  logger.debug("Backend enumerator thread spawned with TID {}", os.str());
#ifdef PRISM_ENABLE_POWER_MANAGEMENT
  if (auto_power_manage) {
    logger.debug("Instantiating power notifier");
    power_notifier =
        PowerNotifier::create([this] { pause(); }, [this] { resume(); });
  }
#endif
  logger.info("Initialization complete");
}

BackendEnumerator::~BackendEnumerator() {
  logger.info("Shutting down");
#ifdef PRISM_ENABLE_POWER_MANAGEMENT
  logger.debug("Destroying power notifier");
  power_notifier.reset();
#endif
  logger.debug("Requesting thread stop");
  thread.request_stop();
  if (waiter) {
    logger.debug("Waking waiter");
    waiter->wake();
  }
  if (thread.joinable()) {
    logger.debug("Awaiting thread join");
    thread.join();
  }
  logger.trace("Releasing registry");
  registry->release();
  logger.info("Shutdown complete");
}

void BackendEnumerator::pause() {
  {
    std::scoped_lock lock(mtx);
    paused = true;
  }
  if (waiter) {
    waiter->wake();
  }
}

void BackendEnumerator::resume() {
  {
    std::scoped_lock lock(mtx);
    paused = false;
  }
  if (waiter) {
    waiter->wake();
  }
}

void BackendEnumerator::run(const std::stop_token &stop) {
#ifdef _WIN32
  const bool com_ok = SUCCEEDED(
      CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_SPEED_OVER_MEMORY));
#endif
  std::stop_callback on_stop(stop, [this] {
    if (waiter) {
      waiter->wake();
    }
  });
  poll_once(SweepMode::Prime);
  const std::uint32_t base = interval_ms;
  const std::uint32_t cap = backoff_max_ms > base ? backoff_max_ms : base;
  std::uint32_t interval = base;
  while (!stop.stop_requested()) {
    bool paused_snapshot;
    {
      std::scoped_lock lock(mtx);
      paused_snapshot = paused;
    }
    if (paused_snapshot) {
      waiter->wait(std::nullopt, std::chrono::milliseconds{0});
      if (stop.stop_requested())
        break;
      bool still_paused;
      {
        std::scoped_lock lock(mtx);
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
      std::scoped_lock lock(mtx);
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
  logger.debug("Polling in mode {}", std::to_underlying(mode));
  const std::size_t n = confirmed.size();
  logger.debug("There are {} instances", n);
  bool disagreement = false;
  for (std::size_t slot = 0; slot < n; ++slot) {
    if (!instances[slot]) {
      logger.debug("Creating instance {}", slot);
      try {
        instances[slot] = registry->create_at(slot);
        assert(instances[slot]);
      } catch (...) {
        instances[slot] = nullptr;
      }
    }
    if (!instances[slot]) {
      logger.debug("Instance slot {} is nullptr; skipping", slot);
      continue;
    }
    logger.debug("Scanning instance slot {}: {}", slot,
                 instances[slot]->get_name());
    bool raw = false;
    try {
      const auto features = instances[slot]->get_features().to_ullong();
      logger.debug("Scan returned feature mask {}", features);
      raw = (features & BackendFeature::IS_SUPPORTED_AT_RUNTIME) != 0;
    } catch (...) {
      logger.debug("Exception was thrown; skipping scan");
      continue;
    }
    const std::uint8_t sample = raw ? 1 : 0;
    if (sample != confirmed[slot])
      disagreement = true;
    if (disagreement)
      logger.debug("Scanned sample disagrees with confirmed one");
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
