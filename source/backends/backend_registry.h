#pragma once

#include "backend.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string_view>
#include <vector>
#ifdef __ANDROID__
#include <jni.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif

enum class BackendId : std::uint64_t {};

constexpr BackendId operator""_bid(const char *str, std::size_t len) {
  std::uint64_t hash = 0xCBF29CE484222325;
  for (std::size_t i = 0; i < len; ++i) {
    hash ^= static_cast<std::uint64_t>(str[i]);
    hash *= 0x100000001B3;
  }
  return static_cast<BackendId>(hash);
}

constexpr BackendId make_backend_id(std::string_view str) {
  std::uint64_t hash = 0xCBF29CE484222325;
  for (std::size_t i = 0; i < str.size(); ++i) {
    hash ^= static_cast<std::uint64_t>(str[i]);
    hash *= 0x100000001B3;
  }
  return static_cast<BackendId>(hash);
}

namespace Backends {
inline constexpr auto SAPI = "SAPI"_bid;
inline constexpr auto AVSpeech = "AVSpeech"_bid;
inline constexpr auto VoiceOver = "VoiceOver"_bid;
inline constexpr auto SpeechDispatcher = "SpeechDispatcher"_bid;
inline constexpr auto NVDA = "NVDA"_bid;
inline constexpr auto JAWS = "JAWS"_bid;
inline constexpr auto OneCore = "OneCore"_bid;
inline constexpr auto Orca = "Orca"_bid;
inline constexpr auto AndroidScreenReader = "AndroidScreenReader"_bid;
inline constexpr auto AndroidTextToSpeech = "AndroidTextToSpeech"_bid;
inline constexpr auto WebSpeechSynthesis = "WebSpeechSynthesis"_bid;
inline constexpr auto UIA = "UIA"_bid;
inline constexpr auto ZDSR = "ZDSR"_bid;
inline constexpr auto ZoomText = "ZoomText"_bid;
} // namespace Backends

class BackendRegistry {
public:
  using Factory = std::function<std::shared_ptr<TextToSpeechBackend>()>;

  static BackendRegistry &instance();
  void register_backend(BackendId id, std::string_view name, int priority,
                        Factory factory);
  [[nodiscard]] bool has(BackendId id) const;
  [[nodiscard]] bool has(std::string_view name) const;
  [[nodiscard]] std::string_view name(BackendId id) const;
  [[nodiscard]] BackendId id(std::string_view name) const;
  [[nodiscard]] int priority(BackendId id) const;
  [[nodiscard]] std::vector<BackendId> list() const;
  [[nodiscard]] std::shared_ptr<TextToSpeechBackend> get(BackendId id);
  [[nodiscard]] std::shared_ptr<TextToSpeechBackend> get(std::string_view name);
  [[nodiscard]] std::shared_ptr<TextToSpeechBackend> create(BackendId id);
  [[nodiscard]] std::shared_ptr<TextToSpeechBackend>
  create(std::string_view name);
  [[nodiscard]] std::shared_ptr<TextToSpeechBackend> create_best();
  [[nodiscard]] std::shared_ptr<TextToSpeechBackend> acquire(BackendId id);
  [[nodiscard]] std::shared_ptr<TextToSpeechBackend>
  acquire(std::string_view name);
  [[nodiscard]] std::shared_ptr<TextToSpeechBackend> acquire_best();
  void clear_cache();
#ifdef __ANDROID__
  void set_java_vm(JavaVM *vm);
#endif
#ifdef _WIN32
  void set_hwnd(HWND hwnd);
#endif
private:
  struct Entry {
    BackendId id;
    std::string_view name;
    int priority;
    Factory factory;
    std::weak_ptr<TextToSpeechBackend> cached;
  };
  BackendRegistry() = default;
  mutable std::shared_mutex mutex;
#ifdef __ANDROID__
  JavaVM *java_vm;
#endif
#ifdef _WIN32
  HWND hwnd_in;
#endif
  std::vector<Entry> entries;
};

template <typename T> struct BackendRegistrar {
  BackendRegistrar(BackendId id, std::string_view name, int priority) {
    BackendRegistry::instance().register_backend(
        id, name, priority, []() { return std::make_shared<T>(); });
  }
  BackendRegistrar(std::string_view name, int priority)
      : BackendRegistrar(make_backend_id(name), name, priority) {}
};

#define REGISTER_BACKEND(cls, name, priority)                                  \
  static ::BackendRegistrar<cls> registrar_##cls##_(name, priority)

#define REGISTER_BACKEND_WITH_ID(cls, id, name, priority)                      \
  static ::BackendRegistrar<cls> registrar_##cls##_(id, name, priority)
