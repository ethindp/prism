// SPDX-License-Identifier: MPL-2.0

#include "logging.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fmt/format.h>
#include <memory>
#include <mutex>
#include <simdutf.h>

namespace {
void PRISM_CALL stderr_sink([[maybe_unused]] void *ud, PrismLogLevel level,
                            const char *source, const char *message) {
  constexpr auto names = std::to_array<std::string_view>(
      {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "UNKNOWN"});
  const auto i = std::min<unsigned>(static_cast<unsigned>(level), 5U);
  fmt::println(stderr, "[prism {}] {}: {}", names[i], source, message);
}

std::once_flag logging_initializer;
} // namespace

Logger::Logger() : drain([this](const std::stop_token &st) { run(st); }) {}

Logger::~Logger() { shutdown(); }

PrismLogHandler Logger::set_handler(PrismLogHandler next) noexcept {
  std::scoped_lock lock(handler_mtx);
  return std::exchange(current, next.fn != nullptr ? next : PrismLogHandler{});
}

PrismLogLevel Logger::set_level(PrismLogLevel level) noexcept {
  return static_cast<PrismLogLevel>(threshold.exchange(
      static_cast<std::uint32_t>(level), std::memory_order_relaxed));
}

void Logger::submit(PrismLogLevel level, std::string source,
                    std::string message) {
  Record record{
      .source = std::move(source),
      .message = std::move(message),
      .level = level,
  };
  std::scoped_lock lock(lifecycle_mtx);
  if (lifecycle != Lifecycle::Running)
    return;
  // One producer token preserves order across calling threads.
  if (queue.try_enqueue(producer, std::move(record)))
    ++submitted;
  else
    dropped.fetch_add(1, std::memory_order_relaxed);
}

void Logger::deliver(std::span<const Record> records) noexcept {
  const auto pair = handler();
  report_drops(pair);
  for (const auto &record : records) {
    if (pair.fn != nullptr)
      pair.fn(pair.userdata, record.level, record.source.c_str(),
              record.message.c_str());
  }
  if (!records.empty()) {
    {
      std::scoped_lock lock(lifecycle_mtx);
      completed += records.size();
    }
    lifecycle_cv.notify_all();
  }
}

void Logger::report_drops(const PrismLogHandler &pair) noexcept {
  const auto count = dropped.exchange(0, std::memory_order_relaxed);
  if (count == 0 || pair.fn == nullptr)
    return;
  // The following absorbs exceptions because reporting them would potentially
  // cause infinite recursion
  // NOLINTBEGIN(bugprone-empty-catch)
  try {
    const auto line = fmt::format("{} log message(s) dropped", count);
    pair.fn(pair.userdata, PRISM_LOG_LEVEL_WARN, "prism", line.c_str());
  } catch (...) {
  }
  // NOLINTEND(bugprone-empty-catch)
}

void Logger::run(const std::stop_token &st) noexcept {
  moodycamel::ConsumerToken token(queue);
  std::array<Record, drain_bulk> batch;
  while (!st.stop_requested()) {
    const std::size_t n = queue.wait_dequeue_bulk_timed(
        token, batch.begin(), drain_bulk, std::chrono::milliseconds(100));
    deliver(std::span{batch}.first(n));
  }
  std::size_t n;
  while ((n = queue.try_dequeue_bulk(token, batch.begin(), drain_bulk)) != 0)
    deliver(std::span{batch}.first(n));
}

void Logger::flush() {
  std::unique_lock lock(lifecycle_mtx);
  if (lifecycle == Lifecycle::Stopped || handler().fn == nullptr)
    return;
  const auto target = submitted;
  lifecycle_cv.wait(lock, [this, target] { return completed >= target; });
}

void Logger::shutdown() noexcept {
  {
    std::unique_lock lock(lifecycle_mtx);
    if (lifecycle == Lifecycle::Stopped)
      return;
    if (lifecycle == Lifecycle::Stopping) {
      lifecycle_cv.wait(lock,
                        [this] { return lifecycle == Lifecycle::Stopped; });
      return;
    }
    lifecycle = Lifecycle::Stopping;
  }
  drain.request_stop();
  if (drain.joinable())
    drain.join();
  {
    std::scoped_lock lock(lifecycle_mtx);
    lifecycle = Lifecycle::Stopped;
  }
  lifecycle_cv.notify_all();
}

Logger *logger() noexcept {
  try {
    return new Logger;
  } catch (...) {
    return nullptr;
  }
}

std::string LogSource::to_utf8(std::wstring_view w) {
  std::string out(simdutf::utf8_length_from_utf16le(
                      reinterpret_cast<const char16_t *>(w.data()), w.size()),
                  '\0');
  out.resize(simdutf::convert_utf16le_to_utf8(
      reinterpret_cast<const char16_t *>(w.data()), w.size(), out.data()));
  return out;
}

void init_logging_from_env() noexcept {
  std::call_once(logging_initializer, [] {
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
  // There is sadly no alternative I can find for doing this in an MT-safe way,
  // so... NOLINTNEXTLINE(concurrency-mt-unsafe)
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
    Logger *const lg = logger();
    if (lg == nullptr)
      return;
    lg->set_handler(PrismLogHandler{.fn = &stderr_sink, .userdata = nullptr});
    lg->set_level(level);
  });
}
