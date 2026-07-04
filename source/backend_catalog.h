#pragma once

#include "backend.h"
#include "logging.h"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <string_view>
#include <vector>
#ifdef __ANDROID__
#include <jni.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif
#include <string>

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
inline constexpr auto BoyPCReader = "BoyPCReader"_bid;
inline constexpr auto PCTalker = "PCTalker"_bid;
inline constexpr auto SenseReader = "SenseReader"_bid;
inline constexpr auto SystemAccess = "SystemAccess"_bid;
inline constexpr auto WindowEyes = "WindowEyes"_bid;
inline constexpr auto Spiel = "Spiel"_bid;
} // namespace Backends

using BackendFactory = std::function<std::shared_ptr<TextToSpeechBackend>()>;

struct Registration {
  BackendId id;
  std::string name;
  int priority;
  BackendFactory factory;
};

class BackendCatalog {
public:
  static BackendCatalog &instance();
  void add(Registration registration);
  [[nodiscard]] std::vector<Registration> snapshot() const;

private:
  BackendCatalog() = default;
  mutable std::mutex mutex;
  std::vector<Registration> registrations;
};

template <typename T> struct BackendRegistrar {
  BackendRegistrar(BackendId id, const char *name, int priority) noexcept {
    try {
      BackendCatalog::instance().add(
          Registration{.id = id,
                       .name = std::string{name},
                       .priority = priority,
                       .factory = []() { return std::make_shared<T>(); }});
    } catch (...) {
    }
  }
  BackendRegistrar(const char *name, int priority) noexcept {
    try {
      std::string owned{name};
      const auto id = make_backend_id(owned);
      BackendCatalog::instance().add(
          Registration{.id = id,
                       .name = std::move(owned),
                       .priority = priority,
                       .factory = []() { return std::make_shared<T>(); }});
    } catch (...) {
    }
  }
};

#define REGISTER_BACKEND(cls, name, priority)                                  \
  [[gnu::used, gnu::retain]] static ::BackendRegistrar<cls>                    \
  registrar_##cls##_(name, priority)

#define REGISTER_BACKEND_WITH_ID(cls, id, name, priority)                      \
  [[gnu::used, gnu::retain]] static ::BackendRegistrar<cls>                    \
  registrar_##cls##_(id, name, priority)
