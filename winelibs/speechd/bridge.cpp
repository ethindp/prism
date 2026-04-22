// SPDX-License-Identifier: MPL-2.0

#include "bridge.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <libspeechd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <new>
#include <ranges>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

struct Instance {
  SPDConnection *conn{nullptr};
};

struct PrismSpeechDispatcherInstance : Instance {};

extern "C" bool prism_speechd_available(void) {
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
    return available;
  }
  return false;
}

extern "C" bool prism_speechd_create(PrismSpeechDispatcherInstance **out) {
  if (!out)
    return false;
  *out = nullptr;
  auto *inst = new (std::nothrow) PrismSpeechDispatcherInstance();
  if (!inst)
    return false;
  char *err = nullptr;
  inst->conn =
      spd_open2("PRISM", nullptr, nullptr, SPD_MODE_THREADED, nullptr, 1, &err);
  if (!inst->conn) {
    delete inst;
    std::free(err);
    return false;
  }
  *out = inst;
  return true;
}

extern "C" void prism_speechd_destroy(PrismSpeechDispatcherInstance *h) {
  if (!h)
    return;
  if (h->conn != nullptr) {
    spd_close(h->conn);
    h->conn = nullptr;
  }
  delete h;
}

extern "C" bool prism_speechd_speak(PrismSpeechDispatcherInstance *h,
                                    const char *text) {
  if (!h || !text || !h->conn)
    return false;
  return spd_say(h->conn, SPD_TEXT, text) >= 0;
}

extern "C" bool prism_speechd_stop(PrismSpeechDispatcherInstance *h) {
  if (!h || !h->conn)
    return false;
  return spd_stop(h->conn) == 0;
}
