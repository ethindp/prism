// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#include "utils.h"
#if (defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) ||      \
     defined(__OpenBSD__) || defined(__DragonFly__)) &&                        \
    !defined(__ANDROID__)
#ifndef NO_LIBSPEECHD
#include <cstdlib>
#include <libspeechd.h>

class SpeechDispatcherBackend final : public TextToSpeechBackend {
private:
  SPDConnection *conn;

public:
  ~SpeechDispatcherBackend() override {
    if (conn) {
      spd_close(conn);
      conn = nullptr;
    }
  }

  std::string_view get_name() const override { return "Speech Dispatcher"; }

  BackendResult<> initialize() override {
    if (conn)
      return std::unexpected(BackendError::AlreadyInitialized);
    char *err = nullptr;
    ;
    conn = spd_open2("PRISM", nullptr, nullptr, SPD_MODE_THREADED, nullptr,
                     true, &err);
    if (!conn) {
      std::free(err);
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!conn)
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
    if (!conn)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = spd_stop(conn); res != 0)
      return std::unexpected(BackendError::InternalBackendError);
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(SpeechDispatcherBackend, Backends::SpeechDispatcher,
                         "Speech Dispatcher", 98);
#endif
#endif