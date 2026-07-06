// SPDX-License-Identifier: MPL-2.0

#pragma once
#include "prism.h"
#include <atomic>
#include <blockingconcurrentqueue.h>
#include <cstddef>
#include <cstdint>
#include <fmt/format.h>
#include <fmt/xchar.h>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <version>

#ifdef __cpp_lib_hardware_interference_size
static_assert(__cpp_lib_hardware_interference_size >= 201703L,
              "__cpp_lib_hardware_interference_size must be >= 201703L");
using std::hardware_destructive_interference_size;
#else
constexpr std::size_t hardware_destructive_interference_size = 64;
#endif

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
  alignas(
      hardware_destructive_interference_size) std::atomic_uint32_t threshold;
  alignas(hardware_destructive_interference_size) std::atomic_uint64_t dropped;
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
  void write(PrismLogLevel level, fmt::format_string<Args...> fmt,
             Args &&...args) const noexcept {
    Logger &lg = logger();
    if (!lg.wants(level))
      return;
    try {
      lg.submit(level, name, fmt::format(fmt, std::forward<Args>(args)...));
    } catch (...) {
      // We swallow all exceptions here; they cannot be allowed to escape into a
      // backends path.
    }
  }

  template <typename... Args>
  void write(PrismLogLevel level, fmt::wformat_string<Args...> fmt,
             Args &&...args) const noexcept {
    Logger &lg = logger();
    if (!lg.wants(level))
      return;
    try {
      lg.submit(level, name,
                to_utf8(fmt::format(fmt, std::forward<Args>(args)...)));
    } catch (...) {
      // Diddo for this overload as above
    }
  }

  static std::string to_utf8(std::wstring_view w);

public:
  explicit LogSource(std::string name) : name(std::move(name)) {}

  template <typename... Args>
  void error(fmt::format_string<Args...> fmt, Args &&...args) const noexcept {
    write(PRISM_LOG_LEVEL_ERROR, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void error(fmt::wformat_string<Args...> fmt, Args &&...args) const noexcept {
    write(PRISM_LOG_LEVEL_ERROR, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void warn(fmt::format_string<Args...> fmt, Args &&...args) const noexcept {
    write(PRISM_LOG_LEVEL_WARN, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void warn(fmt::wformat_string<Args...> fmt, Args &&...args) const noexcept {
    write(PRISM_LOG_LEVEL_WARN, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void info(fmt::format_string<Args...> fmt, Args &&...args) const noexcept {
    write(PRISM_LOG_LEVEL_INFO, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void info(fmt::wformat_string<Args...> fmt, Args &&...args) const noexcept {
    write(PRISM_LOG_LEVEL_INFO, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void debug(fmt::format_string<Args...> fmt, Args &&...args) const noexcept {
    write(PRISM_LOG_LEVEL_DEBUG, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void debug(fmt::wformat_string<Args...> fmt, Args &&...args) const noexcept {
    write(PRISM_LOG_LEVEL_DEBUG, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void trace(fmt::format_string<Args...> fmt, Args &&...args) const noexcept {
    write(PRISM_LOG_LEVEL_TRACE, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void trace(fmt::wformat_string<Args...> fmt, Args &&...args) const noexcept {
    write(PRISM_LOG_LEVEL_TRACE, fmt, std::forward<Args>(args)...);
  }
};

void init_logging_from_env() noexcept;
