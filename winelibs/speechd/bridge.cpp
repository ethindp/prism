// SPDX-License-Identifier: MPL-2.0

#include "bridge.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <libspeechd.h>
#include <limits>
#include <memory>
#include <mutex>
#include <netdb.h>
#include <netinet/in.h>
#include <new>
#include <optional>
#include <poll.h>
#include <ranges>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

struct SpeechdVoice {
  std::string module;
  std::string name;
  std::string language;
};

struct PrismSpeechDispatcherInstance {
  SPDConnection *conn{nullptr};
  std::shared_mutex lock;
  std::vector<SpeechdVoice> voices;
  std::size_t voice_idx{0};
  std::string current_module;
  std::atomic_flag paused;

  PrismSpeechDispatcherInstance() = default;
  PrismSpeechDispatcherInstance(const PrismSpeechDispatcherInstance &) = delete;
  PrismSpeechDispatcherInstance &
  operator=(const PrismSpeechDispatcherInstance &) = delete;
  ~PrismSpeechDispatcherInstance() {
    if (conn != nullptr)
      spd_close(conn);
  }
};

static constexpr int CONNECT_TIMEOUT_MS = 100;

static bool nonblocking_connect_succeeded(int fd, int connect_result,
                                          int connect_error) noexcept {
  if (connect_result == 0)
    return true;
  if (connect_error != EINPROGRESS && connect_error != EWOULDBLOCK)
    return false;
  pollfd descriptor{.fd = fd, .events = POLLOUT, .revents = 0};
  int result;
  do {
    result = poll(&descriptor, 1, CONNECT_TIMEOUT_MS);
  } while (result < 0 && errno == EINTR);
  if (result <= 0)
    return false;
  int socket_error = 0;
  socklen_t error_size = sizeof(socket_error);
  return getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_size) ==
             0 &&
         socket_error == 0;
}

static PrismWinelibStatus
copy_out(std::initializer_list<std::string_view> parts, char *buf, uint32_t cap,
         uint32_t *needed) noexcept {
  std::size_t total = 0;
  for (const auto part : parts)
    total += part.size();
  if (total >= std::numeric_limits<uint32_t>::max())
    return PRISM_WINELIB_INTERNAL;
  *needed = static_cast<uint32_t>(total);
  if (buf == nullptr || cap <= total)
    return PRISM_WINELIB_BUFFER_TOO_SMALL;
  for (const auto part : parts)
    buf = std::ranges::copy(part, buf).out;
  *buf = '\0';
  return PRISM_WINELIB_OK;
}

static PrismWinelibStatus set_param(PrismSpeechDispatcherInstance *h,
                                    float value,
                                    int (*setter)(SPDConnection *, int)) {
  if (h == nullptr || h->conn == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  if (value < 0.0F || value > 1.0F ||
      (!std::isnormal(value) && std::fpclassify(value) != FP_ZERO))
    return PRISM_WINELIB_RANGE;
  const auto native = static_cast<int>(
      std::lround((static_cast<double>(value) * 200.0) - 100.0));
  std::shared_lock sl(h->lock);
  return setter(h->conn, native) == 0 ? PRISM_WINELIB_OK
                                      : PRISM_WINELIB_INTERNAL;
}

static PrismWinelibStatus get_param(PrismSpeechDispatcherInstance *h,
                                    float *out,
                                    int (*getter)(SPDConnection *)) {
  if (h == nullptr || h->conn == nullptr || out == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  std::shared_lock sl(h->lock);
  const int raw = getter(h->conn);
  if (raw < -100 || raw > 100)
    return PRISM_WINELIB_INTERNAL;
  *out = static_cast<float>((static_cast<double>(raw) + 100.0) / 200.0);
  return PRISM_WINELIB_OK;
}

extern "C" PRISM_WINELIB_ABI uint32_t prism_speechd_abi_version(void) noexcept {
  return PRISM_SPEECHD_BRIDGE_ABI_VERSION;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_speechd_available(void) noexcept {
  auto *addr = spd_get_default_address(nullptr);
  if (addr == nullptr)
    return PRISM_WINELIB_NOT_AVAILABLE;
  bool available = false;
  switch (addr->method) {
  case SPD_METHOD_UNIX_SOCKET: {
    if (addr->unix_socket_name != nullptr && *addr->unix_socket_name != 0) {
      const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
      if (fd >= 0) {
        sockaddr_un sa = {};
        sa.sun_family = AF_UNIX;
        const std::string_view path{addr->unix_socket_name};
        const auto len = std::min(path.size(), sizeof(sa.sun_path) - 1);
        std::ranges::copy_n(path.begin(), static_cast<std::ptrdiff_t>(len),
                            sa.sun_path);
        const int result =
            connect(fd, reinterpret_cast<sockaddr *>(&sa), sizeof(sa));
        available =
            nonblocking_connect_succeeded(fd, result, result < 0 ? errno : 0);
        close(fd);
      }
    }
  } break;
  case SPD_METHOD_INET_SOCKET: {
    if (addr->inet_socket_host != nullptr && *addr->inet_socket_host != 0 &&
        addr->inet_socket_port > 0) {
      std::array<char, 16> port{};
      std::to_chars(port.data(), port.data() + port.size() - 1,
                    addr->inet_socket_port);
      addrinfo hints = {};
      hints.ai_family = AF_INET;
      hints.ai_socktype = SOCK_STREAM;
      addrinfo *result = nullptr;
      if (getaddrinfo(addr->inet_socket_host, port.data(), &hints, &result) ==
          0) {
        const int fd =
            socket(result->ai_family, result->ai_socktype | SOCK_NONBLOCK,
                   result->ai_protocol);
        if (fd >= 0) {
          const int status = connect(fd, result->ai_addr, result->ai_addrlen);
          available =
              nonblocking_connect_succeeded(fd, status, status < 0 ? errno : 0);
          close(fd);
        }
        freeaddrinfo(result);
      }
    }
  } break;
  }
  SPDConnectionAddress__free(addr);
  return available ? PRISM_WINELIB_OK : PRISM_WINELIB_NOT_AVAILABLE;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_speechd_refresh_voices(PrismSpeechDispatcherInstance *h) noexcept try {
  if (h == nullptr || h->conn == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  std::unique_lock ul(h->lock);
  std::unique_ptr<char, decltype(&std::free)> saved_module(
      spd_get_output_module(h->conn), std::free);
  if (saved_module == nullptr)
    return PRISM_WINELIB_INTERNAL;
  std::unique_ptr<char *, decltype(&free_spd_modules)> modules(
      spd_list_modules(h->conn), free_spd_modules);
  if (modules == nullptr)
    return PRISM_WINELIB_INTERNAL;
  std::vector<SpeechdVoice> new_voices;
  std::vector<std::string> probed_ok;
  for (char **m = modules.get(); *m != nullptr; ++m) {
    if (spd_set_output_module(h->conn, *m) != 0)
      continue;
    probed_ok.emplace_back(*m);
    std::unique_ptr<SPDVoice *, decltype(&free_spd_voices)> vs(
        spd_list_synthesis_voices(h->conn), free_spd_voices);
    if (vs == nullptr)
      continue;
    for (SPDVoice **v = vs.get(); *v != nullptr; ++v) {
      new_voices.push_back({
          .module = *m,
          .name = (*v)->name != nullptr ? (*v)->name : "",
          .language = (*v)->language != nullptr ? (*v)->language : "",
      });
    }
  }
  bool fully_restored = false;
  std::string restored_to;
  if (spd_set_output_module(h->conn, saved_module.get()) == 0) {
    fully_restored = true;
    restored_to = saved_module.get();
  } else {
    for (const auto &it : std::ranges::reverse_view(probed_ok)) {
      if (it == saved_module.get())
        continue;
      if (spd_set_output_module(h->conn, it.c_str()) == 0) {
        restored_to = it;
        break;
      }
    }
  }
  std::optional<std::size_t> new_idx;
  if (h->voice_idx < h->voices.size()) {
    const auto &cur = h->voices[h->voice_idx];
    for (std::size_t i = 0; i < new_voices.size(); ++i) {
      if (new_voices[i].module == cur.module &&
          new_voices[i].name == cur.name) {
        new_idx = i;
        break;
      }
    }
  }
  std::swap(h->voices, new_voices);
  h->voice_idx = new_idx.value_or(0);
  if (!restored_to.empty())
    h->current_module = std::move(restored_to);
  return fully_restored ? PRISM_WINELIB_OK : PRISM_WINELIB_INTERNAL;
} catch (const std::bad_alloc &) {
  return PRISM_WINELIB_MEMORY;
} catch (...) {
  return PRISM_WINELIB_INTERNAL;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_speechd_create(PrismSpeechDispatcherInstance **out) noexcept try {
  if (out == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  *out = nullptr;
  auto inst = std::make_unique<PrismSpeechDispatcherInstance>();
  char *err = nullptr;
  inst->conn =
      spd_open2("PRISM", nullptr, nullptr, SPD_MODE_THREADED, nullptr, 1, &err);
  if (inst->conn == nullptr) {
    std::free(err);
    return PRISM_WINELIB_NOT_AVAILABLE;
  }
  if (const auto st = prism_speechd_refresh_voices(inst.get());
      st != PRISM_WINELIB_OK)
    return st;
  std::unique_ptr<char, decltype(&std::free)> module(
      spd_get_output_module(inst->conn), std::free);
  if (module != nullptr)
    inst->current_module = module.get();
  *out = inst.release();
  return PRISM_WINELIB_OK;
} catch (const std::bad_alloc &) {
  return PRISM_WINELIB_MEMORY;
} catch (...) {
  return PRISM_WINELIB_INTERNAL;
}

extern "C" PRISM_WINELIB_ABI void
prism_speechd_destroy(PrismSpeechDispatcherInstance *h) noexcept {
  delete h;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_speechd_speak(PrismSpeechDispatcherInstance *h, const char *text,
                    int32_t interrupt) noexcept {
  if (h == nullptr || h->conn == nullptr || text == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  std::shared_lock sl(h->lock);
  if (interrupt != 0) {
    if (spd_stop(h->conn) != 0)
      return PRISM_WINELIB_INTERNAL;
    h->paused.clear();
  }
  return spd_say(h->conn, SPD_MESSAGE, text) >= 0 ? PRISM_WINELIB_OK
                                                  : PRISM_WINELIB_SPEAK_FAILURE;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_speechd_stop(PrismSpeechDispatcherInstance *h) noexcept {
  if (h == nullptr || h->conn == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  std::shared_lock sl(h->lock);
  if (spd_stop(h->conn) != 0)
    return PRISM_WINELIB_INTERNAL;
  h->paused.clear();
  return PRISM_WINELIB_OK;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_speechd_pause(PrismSpeechDispatcherInstance *h) noexcept {
  if (h == nullptr || h->conn == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  if (h->paused.test_and_set())
    return PRISM_WINELIB_ALREADY_PAUSED;
  std::shared_lock sl(h->lock);
  if (spd_pause(h->conn) != 0) {
    h->paused.clear();
    return PRISM_WINELIB_INTERNAL;
  }
  return PRISM_WINELIB_OK;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_speechd_resume(PrismSpeechDispatcherInstance *h) noexcept {
  if (h == nullptr || h->conn == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  if (!h->paused.test())
    return PRISM_WINELIB_NOT_PAUSED;
  std::shared_lock sl(h->lock);
  if (spd_resume(h->conn) != 0)
    return PRISM_WINELIB_INTERNAL;
  h->paused.clear();
  return PRISM_WINELIB_OK;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus prism_speechd_set_volume(
    PrismSpeechDispatcherInstance *h, float value) noexcept {
  return set_param(h, value, spd_set_volume);
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus prism_speechd_get_volume(
    PrismSpeechDispatcherInstance *h, float *out) noexcept {
  return get_param(h, out, spd_get_volume);
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_speechd_set_rate(PrismSpeechDispatcherInstance *h, float value) noexcept {
  return set_param(h, value, spd_set_voice_rate);
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_speechd_get_rate(PrismSpeechDispatcherInstance *h, float *out) noexcept {
  return get_param(h, out, spd_get_voice_rate);
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus prism_speechd_set_pitch(
    PrismSpeechDispatcherInstance *h, float value) noexcept {
  return set_param(h, value, spd_set_voice_pitch);
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_speechd_get_pitch(PrismSpeechDispatcherInstance *h, float *out) noexcept {
  return get_param(h, out, spd_get_voice_pitch);
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus prism_speechd_count_voices(
    PrismSpeechDispatcherInstance *h, uint32_t *out) noexcept {
  if (h == nullptr || out == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  std::shared_lock sl(h->lock);
  if (h->voices.size() > std::numeric_limits<uint32_t>::max())
    return PRISM_WINELIB_INTERNAL;
  *out = static_cast<uint32_t>(h->voices.size());
  return PRISM_WINELIB_OK;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus prism_speechd_get_voice_name(
    PrismSpeechDispatcherInstance *h, uint32_t index, char *buf, uint32_t cap,
    uint32_t *needed) noexcept {
  if (h == nullptr || needed == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  std::shared_lock sl(h->lock);
  if (index >= h->voices.size())
    return PRISM_WINELIB_RANGE;
  const auto &v = h->voices[index];
  return copy_out({v.name, " (", v.module, ")"}, buf, cap, needed);
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_speechd_get_voice_language(PrismSpeechDispatcherInstance *h,
                                 uint32_t index, char *buf, uint32_t cap,
                                 uint32_t *needed) noexcept {
  if (h == nullptr || needed == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  std::shared_lock sl(h->lock);
  if (index >= h->voices.size())
    return PRISM_WINELIB_RANGE;
  return copy_out({h->voices[index].language}, buf, cap, needed);
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus prism_speechd_set_voice(
    PrismSpeechDispatcherInstance *h, uint32_t index) noexcept try {
  if (h == nullptr || h->conn == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  std::unique_lock ul(h->lock);
  if (index >= h->voices.size())
    return PRISM_WINELIB_RANGE;
  const auto &v = h->voices[index];
  if (spd_set_output_module(h->conn, v.module.c_str()) != 0)
    return PRISM_WINELIB_INTERNAL;
  if (spd_set_synthesis_voice(h->conn, v.name.c_str()) != 0) {
    if (!h->current_module.empty() &&
        spd_set_output_module(h->conn, h->current_module.c_str()) != 0) {
      h->current_module.clear();
      return PRISM_WINELIB_UNDEFINED_STATE;
    }
    return PRISM_WINELIB_INTERNAL;
  }
  h->voice_idx = index;
  h->current_module = v.module;
  return PRISM_WINELIB_OK;
} catch (const std::bad_alloc &) {
  return PRISM_WINELIB_MEMORY;
} catch (...) {
  return PRISM_WINELIB_INTERNAL;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus prism_speechd_get_voice(
    PrismSpeechDispatcherInstance *h, uint32_t *out) noexcept {
  if (h == nullptr || out == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  std::shared_lock sl(h->lock);
  if (h->voice_idx >= h->voices.size())
    return PRISM_WINELIB_NO_VOICES;
  *out = static_cast<uint32_t>(h->voice_idx);
  return PRISM_WINELIB_OK;
}
