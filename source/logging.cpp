// SPDX-License-Identifier: MPL-2.0

#include "logging.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <print>

namespace {
struct FlushSignal {
  std::mutex m;
  std::condition_variable cv;
  bool done = false;
};

void PRISM_CALL stderr_sink([[maybe_unused]] void *ud, PrismLogLevel level,
                            const char *source, const char *message) {
  constexpr auto names = std::to_array<std::string_view>(
      {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "UNKNOWN"});
  const auto i = std::min<unsigned>(static_cast<unsigned>(level), 5U);
  std::println(stderr, "[prism {}] {}: {}", names[i], source, message);
}

} // namespace

Logger::Logger() : drain([this] { run(); }) {}

Logger::~Logger() { shutdown(); }

PrismLogHandler Logger::set_handler(PrismLogHandler next) noexcept {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
  const Handler *fresh =
      next.fn != nullptr ? new (std::nothrow)
                               Handler{.fn = next.fn, .userdata = next.userdata}
                         : nullptr;
  const Handler *old = this->current.exchange(fresh, std::memory_order_acq_rel);
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
  PrismLogHandler previous{};
  if (old != nullptr)
    previous = PrismLogHandler{.fn = old->fn, .userdata = old->userdata};
  // old is intentionally leaked since we can't know when the drain thread is
  // done with it
  return previous;
}

PrismLogLevel Logger::set_level(PrismLogLevel level) noexcept {
  return static_cast<PrismLogLevel>(threshold.exchange(
      static_cast<std::uint32_t>(level), std::memory_order_relaxed));
}

void Logger::submit(PrismLogLevel level, std::string source,
                    std::string message) {
  Record record{.source = std::move(source),
                .message = std::move(message),
                .flush_signal = nullptr,
                .level = level};
  if (!queue.try_enqueue(std::move(record)))
    dropped.fetch_add(1, std::memory_order_relaxed);
}

void Logger::deliver(Record &record, const Handler *pair) noexcept {
  if (record.flush_signal != nullptr) {
    auto *signal = static_cast<FlushSignal *>(record.flush_signal);
    {
      std::lock_guard lock(signal->m);
      signal->done = true;
    }
    signal->cv.notify_all();
    return;
  }
  if (pair != nullptr && pair->fn != nullptr)
    pair->fn(pair->userdata, record.level, record.source.c_str(),
             record.message.c_str());
}

void Logger::report_drops(const Handler *pair) noexcept {
  const auto count = dropped.exchange(0, std::memory_order_relaxed);
  if (count == 0 || pair == nullptr || pair->fn == nullptr)
    return;
  // The following absorbs exceptions because reporting them would potentially
  // cause infinite recursion
  // NOLINTBEGIN(bugprone-empty-catch)
  try {
    const auto line = std::format("{} log message(s) dropped", count);
    pair->fn(pair->userdata, PRISM_LOG_LEVEL_WARN, "prism", line.c_str());
  } catch (...) {
  }
  // NOLINTEND(bugprone-empty-catch)
}

void Logger::run() noexcept {
  moodycamel::ConsumerToken token(queue);
  std::array<Record, drain_bulk> batch;
  while (!stop.test(std::memory_order_relaxed)) {
    const std::size_t n = queue.wait_dequeue_bulk_timed(
        token, batch.begin(), drain_bulk, std::chrono::milliseconds(100));
    const Handler *pair = handler();
    report_drops(pair);
    for (std::size_t i = 0; i < n; ++i)
      Logger::deliver(batch[i], pair);
  }
  std::size_t n;
  while ((n = queue.try_dequeue_bulk(token, batch.begin(), drain_bulk)) != 0) {
    const Handler *pair = handler();
    for (std::size_t i = 0; i < n; ++i)
      Logger::deliver(batch[i], pair);
  }
}

void Logger::flush() {
  if (handler() == nullptr)
    return;
  FlushSignal signal;
  Record record{
      .source = {},
      .message = {},
      .flush_signal = &signal,
      .level = PRISM_LOG_LEVEL_NONE,
  };
  queue.enqueue(std::move(record)); // a flush is never dropped
  std::unique_lock lock(signal.m);
  signal.cv.wait(lock, [&signal] { return signal.done; });
}

void Logger::shutdown() noexcept {
  if (stop.test_and_set(std::memory_order_relaxed))
    return;
  queue.enqueue(Record{});
  if (drain.joinable())
    drain.join();
}

Logger &logger() noexcept {
  static auto *instance = new (std::nothrow) Logger;
  return *instance;
}

void init_logging_from_env() noexcept {
#ifdef _WIN32
  char *env_raw = nullptr;
  size_t len = 0;
  if (_dupenv_s(&env_raw, &len, "PRISM_LOG") != 0)
    return;
  std::unique_ptr<char, decltype(&std::free)> env{env_raw, &std::free};
  if (!env || *env == '\0')
    return;
  const std::string_view value{env.get()};
#else
  const char *env = std::getenv("PRISM_LOG");
  if (env == nullptr || *env == '\0')
    return;
  const std::string_view value{env};
#endif
  PrismLogLevel level = PRISM_LOG_LEVEL_NONE;
  if (value == "trace")
    level = PRISM_LOG_LEVEL_TRACE;
  else if (value == "debug")
    level = PRISM_LOG_LEVEL_DEBUG;
  else if (value == "warn")
    level = PRISM_LOG_LEVEL_WARN;
  else if (value == "error")
    level = PRISM_LOG_LEVEL_ERROR;
  else if (value == "info")
    level = PRISM_LOG_LEVEL_INFO;
  else if (value == "none")
    level = PRISM_LOG_LEVEL_NONE;
  Logger &lg = logger();
  lg.set_handler(PrismLogHandler{&stderr_sink, nullptr});
  lg.set_level(level);
}
