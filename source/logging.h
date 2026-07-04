// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "prism.h"
#include <atomic>
#include <blockingconcurrentqueue.h>
#include <cstddef>
#include <cstdint>
#include <format>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

class Logger {
private:
  struct Record {
    std::string source;
    std::string message;
    void *flush_signal = nullptr;
    PrismLogLevel level = PRISM_LOG_LEVEL_INFO;
  };

  struct Handler {
    PrismLogCallback fn = nullptr;
    void *userdata = nullptr;
  };

  static constexpr std::size_t capacity = 4096;
  static constexpr std::size_t drain_bulk = 32;
  alignas(std::hardware_destructive_interference_size)
      std::atomic_uint32_t threshold;
  alignas(
      std::hardware_destructive_interference_size) std::atomic_uint64_t dropped;
  std::atomic<const Handler *> current{nullptr};
  std::atomic_flag stop;
  moodycamel::BlockingConcurrentQueue<Record> queue{capacity};
  std::thread drain;

  void run() noexcept;

  [[nodiscard]] const Handler *handler() const noexcept {
    return current.load(std::memory_order_acquire);
  }

  static void deliver(Record &record, const Handler *pair) noexcept;

  void report_drops(const Handler *pair) noexcept;

public:
  Logger();

  ~Logger();

  Logger(const Logger &) = delete;

  Logger &operator=(const Logger &) = delete;

  PrismLogHandler set_handler(PrismLogHandler next) noexcept;

  PrismLogLevel set_level(PrismLogLevel level) noexcept;

  [[nodiscard]] bool wants(PrismLogLevel level) const noexcept {
    return static_cast<std::uint32_t>(level) >=
           threshold.load(std::memory_order_relaxed);
  }

  void submit(PrismLogLevel level, std::string source, std::string message);

  void flush();

  void shutdown() noexcept;
};

[[nodiscard]] Logger &logger() noexcept;

class LogSource {
private:
  std::string name;

  template <typename... Args>
  void write(PrismLogLevel level, std::format_string<Args...> fmt,
             Args &&...args) const noexcept {
    Logger &lg = logger();
    if (!lg.wants(level))
      return;
    try {
      lg.submit(level, name, std::format(fmt, std::forward<Args>(args)...));
    } catch (...) {
      // We swallow all exceptions here; they cannot be allowed to escape into a
      // backends path.
    }
  }

public:
  explicit LogSource(std::string name) : name(std::move(name)) {}

  template <typename... Args>
  void error(std::format_string<Args...> fmt, Args &&...args) const noexcept {
    write(PRISM_LOG_LEVEL_ERROR, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void warn(std::format_string<Args...> fmt, Args &&...args) const noexcept {
    write(PRISM_LOG_LEVEL_WARN, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void info(std::format_string<Args...> fmt, Args &&...args) const noexcept {
    write(PRISM_LOG_LEVEL_INFO, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void debug(std::format_string<Args...> fmt, Args &&...args) const noexcept {
    write(PRISM_LOG_LEVEL_DEBUG, fmt, std::forward<Args>(args)...);
  }
};

void init_logging_from_env() noexcept;
