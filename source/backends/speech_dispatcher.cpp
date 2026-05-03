// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#include "utils.h"
#if (defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) ||      \
     defined(__OpenBSD__) || defined(__DragonFly__)) &&                        \
    !defined(__ANDROID__)
#ifndef NO_LIBSPEECHD
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <format>
#include <libspeechd.h>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <optional>
#include <ranges>
#include <shared_mutex>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

struct VoiceInfo {
  std::string module;
  std::string name;
  std::string language;
};

struct ModulesGuard {
  char **m;
  ~ModulesGuard() {
    if (m != nullptr)
      free_spd_modules(m);
  }
};

class SpeechDispatcherBackend final : public TextToSpeechBackend {
private:
  SPDConnection *conn{nullptr};
  std::atomic_flag initialized;
  std::vector<VoiceInfo> voices;
  mutable std::shared_mutex state_lock;
  std::atomic_uint64_t voice_idx{0};
  std::atomic_flag paused;
  std::string current_module;

public:
  ~SpeechDispatcherBackend() override {
    if (conn != nullptr) {
      spd_close(conn);
      conn = nullptr;
    }
  }

  [[nodiscard]] std::string_view get_name() const override {
    return "Speech Dispatcher";
  }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    auto *addr = spd_get_default_address(nullptr);
    if (addr != nullptr) {
      bool available = false;
      switch (addr->method) {
      case SPD_METHOD_UNIX_SOCKET: {
        if (addr->unix_socket_name != nullptr && *addr->unix_socket_name != 0) {
          int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
          if (fd >= 0) {
            sockaddr_un sa = {};
            sa.sun_family = AF_UNIX;
            std::string_view path{addr->unix_socket_name};
            auto len = std::min(path.size(), sizeof(sa.sun_path) - 1);
            std::ranges::copy_n(path.begin(), static_cast<std::ptrdiff_t>(len),
                                sa.sun_path);
            int result =
                connect(fd, reinterpret_cast<sockaddr *>(&sa), sizeof(sa));
            available = (result == 0) || (result < 0 && errno == EINPROGRESS);
            close(fd);
          }
        }
      } break;
      case SPD_METHOD_INET_SOCKET: {
        if (addr->inet_socket_host != nullptr && *addr->inet_socket_host != 0 &&
            addr->inet_socket_port > 0) {
          addrinfo hints = {};
          hints.ai_family = AF_INET;
          hints.ai_socktype = SOCK_STREAM;
          auto const port_str = std::to_string(addr->inet_socket_port);
          addrinfo *result = nullptr;
          if (getaddrinfo(addr->inet_socket_host, port_str.c_str(), &hints,
                          &result) == 0) {
            int fd =
                socket(result->ai_family, result->ai_socktype | SOCK_NONBLOCK,
                       result->ai_protocol);
            if (fd >= 0) {
              int status = connect(fd, result->ai_addr, result->ai_addrlen);
              available = (status == 0) || (status < 0 && errno == EINPROGRESS);
              close(fd);
            }
            freeaddrinfo(result);
          }
        }
      } break;
      }
      SPDConnectionAddress__free(addr);
      if (available)
        features |= IS_SUPPORTED_AT_RUNTIME;
    }
    features |= SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_STOP |
                SUPPORTS_SET_VOLUME | SUPPORTS_GET_VOLUME | SUPPORTS_SET_RATE |
                SUPPORTS_GET_RATE | SUPPORTS_SET_PITCH | SUPPORTS_GET_PITCH |
                SUPPORTS_REFRESH_VOICES | SUPPORTS_COUNT_VOICES |
                SUPPORTS_GET_VOICE_NAME | SUPPORTS_GET_VOICE_LANGUAGE |
                SUPPORTS_GET_VOICE | SUPPORTS_SET_VOICE | SUPPORTS_PAUSE |
                SUPPORTS_RESUME;
    return features;
  }

  BackendResult<> initialize() override {
    if (conn != nullptr)
      return std::unexpected(BackendError::AlreadyInitialized);
    char *err = nullptr;
    conn = spd_open2("PRISM", nullptr, nullptr, SPD_MODE_THREADED, nullptr, 1,
                     &err);
    if (conn == nullptr) {
      std::free(err);
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    if (const auto res = refresh_voices(); !res) {
      spd_close(conn);
      conn = nullptr;
      return res;
    }
    char *raw_module = spd_get_output_module(conn);
    if (raw_module != nullptr) {
      current_module = raw_module;
      std::free(raw_module);
    } else {
      current_module.clear();
    }
    initialized.test_and_set();
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!initialized.test() || conn == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (!simdutf::validate_utf8(text.data(), text.size())) {
      return std::unexpected(BackendError::InvalidUtf8);
    }
    std::shared_lock sl(state_lock);
    if (interrupt) {
      if (spd_stop(conn) != 0)
        return std::unexpected(BackendError::InternalBackendError);
      paused.clear();
    }
    if (const auto res = spd_say(conn, SPD_TEXT, text.data()); res < 0) {
      return std::unexpected(BackendError::SpeakFailure);
    }
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<> stop() override {
    if (!initialized.test() || conn == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    std::shared_lock sl(state_lock);
    if (const auto res = spd_stop(conn); res != 0)
      return std::unexpected(BackendError::InternalBackendError);
    paused.clear();
    return {};
  }

  BackendResult<> pause() override {
    if (!initialized.test() || conn == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (paused.test_and_set())
      return std::unexpected(BackendError::AlreadyPaused);
    std::shared_lock sl(state_lock);
    if (const auto res = spd_pause(conn); res != 0) {
      paused.clear();
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }

  BackendResult<> resume() override {
    if (!initialized.test() || conn == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (!paused.test())
      return std::unexpected(BackendError::NotPaused);
    std::shared_lock sl(state_lock);
    if (const auto res = spd_resume(conn); res != 0)
      return std::unexpected(BackendError::InternalBackendError);
    paused.clear();
    return {};
  }

  BackendResult<> set_volume(float volume) override {
    if (!initialized.test() || conn == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (volume < 0.0F || volume > 1.0F ||
        (!std::isnormal(volume) && std::fpclassify(volume) != FP_ZERO)) {
      return std::unexpected(BackendError::RangeOutOfBounds);
    }
    auto const v = static_cast<std::int32_t>(std::round(
        range_convert(static_cast<double>(volume), 0.0, 1.0, -100.0, 100.0)));
    std::shared_lock sl(state_lock);
    if (const auto res = spd_set_volume(conn, v); res != 0) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }

  BackendResult<float> get_volume() override {
    if (!initialized.test() || conn == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    std::shared_lock sl(state_lock);
    auto const raw = spd_get_volume(conn);
    if (raw < -100 || raw > 100)
      return std::unexpected(BackendError::InternalBackendError);
    return static_cast<float>(range_convert(raw, -100.0, 100.0, 0.0, 1.0));
  }

  BackendResult<> set_rate(float rate) override {
    if (!initialized.test() || conn == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (rate < 0.0F || rate > 1.0F ||
        (!std::isnormal(rate) && std::fpclassify(rate) != FP_ZERO)) {
      return std::unexpected(BackendError::RangeOutOfBounds);
    }
    auto const r = static_cast<std::int32_t>(std::round(
        range_convert(static_cast<double>(rate), 0.0, 1.0, -100.0, 100.0)));
    std::shared_lock sl(state_lock);
    if (const auto res = spd_set_voice_rate(conn, r); res != 0) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }

  BackendResult<float> get_rate() override {
    if (!initialized.test() || conn == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    std::shared_lock sl(state_lock);
    auto const raw = spd_get_voice_rate(conn);
    if (raw < -100 || raw > 100)
      return std::unexpected(BackendError::InternalBackendError);
    return static_cast<float>(range_convert(raw, -100.0, 100.0, 0.0, 1.0));
  }

  BackendResult<> set_pitch(float pitch) override {
    if (!initialized.test() || conn == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (pitch < 0.0F || pitch > 1.0F ||
        (!std::isnormal(pitch) && std::fpclassify(pitch) != FP_ZERO)) {
      return std::unexpected(BackendError::RangeOutOfBounds);
    }
    auto const p = static_cast<std::int32_t>(std::round(
        range_convert(static_cast<double>(pitch), 0.0, 1.0, -100.0, 100.0)));
    std::shared_lock sl(state_lock);
    if (const auto res = spd_set_voice_pitch(conn, p); res != 0) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }

  BackendResult<float> get_pitch() override {
    if (!initialized.test() || conn == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    std::shared_lock sl(state_lock);
    auto const raw = spd_get_voice_pitch(conn);
    if (raw < -100 || raw > 100)
      return std::unexpected(BackendError::InternalBackendError);
    return static_cast<float>(range_convert(raw, -100.0, 100.0, 0.0, 1.0));
  }

  BackendResult<> refresh_voices() override {
    if (conn == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    std::unique_lock ul(state_lock);
    char *saved_raw = spd_get_output_module(conn);
    if (saved_raw == nullptr)
      return std::unexpected(BackendError::InternalBackendError);
    std::unique_ptr<char, decltype(&std::free)> saved_module(saved_raw,
                                                             std::free);
    char **modules = spd_list_modules(conn);
    if (modules == nullptr)
      return std::unexpected(BackendError::InternalBackendError);
    ModulesGuard modules_guard{modules};
    std::vector<VoiceInfo> new_voices;
    std::vector<std::string> probed_ok;
    for (char **m = modules; *m != nullptr; ++m) {
      if (spd_set_output_module(conn, *m) != 0)
        continue;
      probed_ok.emplace_back(*m);
      SPDVoice **vs = spd_list_synthesis_voices(conn);
      if (vs == nullptr)
        continue;
      for (SPDVoice **v = vs; *v != nullptr; ++v) {
        new_voices.emplace_back(VoiceInfo{
            .module = *m,
            .name = (*v)->name ? (*v)->name : "",
            .language = (*v)->language ? (*v)->language : "",
        });
      }
      free_spd_voices(vs);
    }
    bool fully_restored = false;
    std::string restored_to;
    if (spd_set_output_module(conn, saved_module.get()) == 0) {
      fully_restored = true;
      restored_to = saved_module.get();
    } else {
      for (auto it = probed_ok.rbegin(); it != probed_ok.rend(); ++it) {
        if (*it == saved_module.get())
          continue;
        if (spd_set_output_module(conn, it->data()) == 0) {
          restored_to = *it;
          break;
        }
      }
    }
    std::optional<std::size_t> new_idx;
    if (auto cur = voice_idx.load(); cur < voices.size()) {
      auto const &cur_voice = voices[cur];
      for (std::size_t i = 0; i < new_voices.size(); ++i) {
        if (new_voices[i].module == cur_voice.module &&
            new_voices[i].name == cur_voice.name) {
          new_idx = i;
          break;
        }
      }
    }
    std::swap(voices, new_voices);
    voice_idx.store(new_idx.value_or(0), std::memory_order_release);
    if (!restored_to.empty()) {
      current_module = std::move(restored_to);
    }
    return fully_restored ? BackendResult<>{}
                          : std::unexpected(BackendError::InternalBackendError);
  }

  BackendResult<std::size_t> count_voices() override {
    if (!initialized.test() || conn == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    std::shared_lock sl(state_lock);
    return voices.size();
  }

  BackendResult<std::string> get_voice_name(std::size_t id) override {
    if (!initialized.test() || conn == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    std::shared_lock sl(state_lock);
    if (id >= voices.size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    return std::format("{} ({})", voices[id].name, voices[id].module);
  }

  BackendResult<std::string> get_voice_language(std::size_t id) override {
    if (!initialized.test() || conn == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    std::shared_lock sl(state_lock);
    if (id >= voices.size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    return voices[id].language;
  }

  BackendResult<> set_voice(std::size_t id) override {
    if (!initialized.test() || conn == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    std::unique_lock ul(state_lock);
    if (id >= voices.size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    if (spd_set_output_module(conn, voices[id].module.data()) != 0) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    if (spd_set_synthesis_voice(conn, voices[id].name.data()) != 0) {
      if (!current_module.empty()) {
        if (spd_set_output_module(conn, current_module.data()) != 0) {
          current_module.clear();
          return std::unexpected(BackendError::BackendEnteredUndefinedState);
        }
      }
      return std::unexpected(BackendError::InternalBackendError);
    }
    voice_idx.store(id);
    current_module = voices[id].module;
    return {};
  }

  BackendResult<std::size_t> get_voice() override {
    if (!initialized.test() || conn == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    std::shared_lock sl(state_lock);
    auto idx = voice_idx.load();
    if (idx >= voices.size())
      return std::unexpected(BackendError::NoVoices);
    return idx;
  }
};

REGISTER_BACKEND_WITH_ID(SpeechDispatcherBackend, Backends::SpeechDispatcher,
                         "Speech Dispatcher", 97);
#endif
#elifdef _WIN32
#include "raw/prism_speech_dispatcher_bridge.h"
#include <atomic>
#include <tchar.h>
#include <windows.h>

class SpeechDispatcherBackend final : public TextToSpeechBackend {
private:
  std::atomic<PrismSpeechDispatcherInstance *> instance{nullptr};

public:
  ~SpeechDispatcherBackend() override {
    if (instance != nullptr) {
      prism_speechd_destroy(instance);
      instance = nullptr;
    }
  }

  [[nodiscard]] std::string_view get_name() const override {
    return "Speech Dispatcher";
  }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    if (auto *const ntdll_handle = GetModuleHandle(_T("ntdll.dll"));
        ntdll_handle != nullptr) {
      if (auto *const wgv_addr =
              GetProcAddress(ntdll_handle, "wine_get_version");
          wgv_addr != nullptr) {
        if (prism_speechd_available()) {
          features |= IS_SUPPORTED_AT_RUNTIME;
        }
      }
    }
    features |= SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_STOP;
    return features;
  }

  BackendResult<> initialize() override {
    if (instance != nullptr) {
      return std::unexpected(BackendError::AlreadyInitialized);
    }
    if (auto *const ntdll_handle = GetModuleHandle(_T("ntdll.dll"));
        ntdll_handle == nullptr) {
      return std::unexpected(BackendError::InternalBackendError);
    } else {
      if (auto *const wgv_addr =
              GetProcAddress(ntdll_handle, "wine_get_version");
          wgv_addr == nullptr) {
        return std::unexpected(BackendError::BackendNotAvailable);
      } else {
        if (!prism_speechd_available()) {
          return std::unexpected(BackendError::BackendNotAvailable);
        }
      }
    }
    PrismSpeechDispatcherInstance *h = nullptr;
    if (!prism_speechd_create(&h)) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    if (h == nullptr) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    instance.store(h);
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (instance == nullptr) {
      return std::unexpected(BackendError::NotInitialized);
    }
    if (!simdutf::validate_utf8(text.data(), text.size())) {
      return std::unexpected(BackendError::InvalidUtf8);
    }
    if (interrupt)
      if (const auto res = stop(); !res)
        return res;
    if (const auto res = prism_speechd_speak(instance, text.data()); !res) {
      return std::unexpected(BackendError::SpeakFailure);
    }
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<> stop() override {
    if (instance == nullptr) {
      return std::unexpected(BackendError::NotInitialized);
    }
    if (const auto res = prism_speechd_stop(instance); !res) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(SpeechDispatcherBackend, Backends::SpeechDispatcher,
                         "Speech Dispatcher", 97);
#endif