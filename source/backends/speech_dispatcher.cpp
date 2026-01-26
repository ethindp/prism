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
#include <cerrno>
#include <cstdlib>
#include <libspeechd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <ranges>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

class SpeechDispatcherBackend final : public TextToSpeechBackend {
private:
  SPDConnection *conn{nullptr};

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
    features |= SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_STOP;
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
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (conn == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (!simdutf::validate_utf8(text.data(), text.size())) {
      return std::unexpected(BackendError::InvalidUtf8);
    }
    if (interrupt)
      if (const auto res = stop(); !res)
        return res;
    if (const auto res = spd_say(conn, SPD_TEXT, text.data()); res != 0) {
      return std::unexpected(BackendError::SpeakFailure);
    }
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<> stop() override {
    if (conn == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = spd_stop(conn); res != 0)
      return std::unexpected(BackendError::InternalBackendError);
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(SpeechDispatcherBackend, Backends::SpeechDispatcher,
                         "Speech Dispatcher", 97);
#endif
#endif