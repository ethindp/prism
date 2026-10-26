// SPDX-License-Identifier: MPL-2.0

#include "../backend.h"
#include "../backend_catalog.h"
#include "../logging.h"
#include "../utils.h"
#if (defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) ||      \
     defined(__OpenBSD__) || defined(__DragonFly__)) &&                        \
    !defined(__ANDROID__)
#ifdef PRISM_HAVE_ORCA
#include "../dbus/orca.h"
#include <optional>
#include <string>

namespace {
using prism::dbus::Connection;
using OrcaService = prism::dbus::services::orca::Service;
} // namespace

class OrcaBackend final : public TextToSpeechBackend {
private:
  const LogSource log{"Orca"};
  Connection conn;
  std::optional<OrcaService> service;

public:
  ~OrcaBackend() override = default;

  [[nodiscard]] std::string_view get_name() const override { return "Orca"; }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    features |= SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_STOP;
    auto probe = Connection::open(DBUS_BUS_SESSION);
    if (probe) {
      if (OrcaService::detect(probe))
        features |= IS_SUPPORTED_AT_RUNTIME;
    }
    return features;
  }

  BackendResult<> initialize() override {
    if (conn)
      return std::unexpected(BackendError::AlreadyInitialized);
    auto candidate = Connection::open(DBUS_BUS_SESSION);
    if (!candidate) {
      log.debug("Session D-Bus connection unavailable");
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    const auto resolved = OrcaService::detect(candidate);
    if (!resolved) {
      log.debug("No compatible Orca service found on session D-Bus");
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    conn = std::move(candidate);
    service = *resolved;
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    const auto *active = service ? &*service : nullptr;
    if (!conn || active == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (interrupt) {
      if (auto res = stop(); !res)
        return res;
    }
    if (auto result = active->present(conn, text); !result) {
      log.warn("Orca speech failed: {}", result.error().message);
      return std::unexpected(BackendError::SpeakFailure);
    }
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<> stop() override {
    const auto *active = service ? &*service : nullptr;
    if (!conn || active == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (auto result = active->interrupt(conn); !result) {
      log.warn("Orca speech interruption failed: {}", result.error().message);
      return std::unexpected(BackendError::SpeakFailure);
    }
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(OrcaBackend, Backends::Orca, "Orca", 100);
#endif
#elifdef _WIN32
#include "../optional_library.h"
#include <atomic>
#include <optional>
#include <raw/prism_orca_bridge.h>
#include <tchar.h>
#include <windows.h>

namespace {
constexpr const TCHAR *orca_bridge_dll = _T("prism_orca_bridge.dll");

struct OrcaBridge {
  SharedLibrary library;
  decltype(&::prism_orca_available) prism_orca_available = nullptr;
  decltype(&::prism_orca_create) prism_orca_create = nullptr;
  decltype(&::prism_orca_destroy) prism_orca_destroy = nullptr;
  decltype(&::prism_orca_speak) prism_orca_speak = nullptr;
  decltype(&::prism_orca_stop) prism_orca_stop = nullptr;
};

std::optional<OrcaBridge> load_orca_bridge() {
  static const LogSource log{"Orca"};
  OrcaBridge api{.library = open_optional_library(orca_bridge_dll, nullptr)};
  if (!api.library) {
    log.debug(_T("{} could not be loaded"), orca_bridge_dll);
    return std::nullopt;
  }
  const SharedLibrary &library = api.library;
  const bool complete =
      library.bind(api.prism_orca_available, "prism_orca_available") &&
      library.bind(api.prism_orca_create, "prism_orca_create") &&
      library.bind(api.prism_orca_destroy, "prism_orca_destroy") &&
      library.bind(api.prism_orca_speak, "prism_orca_speak") &&
      library.bind(api.prism_orca_stop, "prism_orca_stop");
  if (!complete) {
    log.debug(_T("{} is missing a function prism needs"), orca_bridge_dll);
    return std::nullopt;
  }
  return api;
}

const OrcaBridge *orca_bridge() {
  static const std::optional<OrcaBridge> api = load_orca_bridge();
  return api ? &*api : nullptr;
}
} // namespace

class OrcaBackend final : public TextToSpeechBackend {
private:
  std::atomic<PrismOrcaDBusInstance *> instance{nullptr};
  const OrcaBridge *api = nullptr;

public:
  ~OrcaBackend() override {
    if (instance != nullptr) {
      api->prism_orca_destroy(instance);
      instance = nullptr;
    }
  }

  [[nodiscard]] std::string_view get_name() const override { return "Orca"; }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    if (auto *const ntdll_handle = GetModuleHandle(_T("ntdll.dll"));
        ntdll_handle != nullptr) {
      if (auto *const wgv_addr =
              GetProcAddress(ntdll_handle, "wine_get_version");
          wgv_addr != nullptr) {
        if (const auto *bridge = orca_bridge();
            bridge != nullptr && bridge->prism_orca_available()) {
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
        api = orca_bridge();
        if (api == nullptr || !api->prism_orca_available()) {
          return std::unexpected(BackendError::BackendNotAvailable);
        }
      }
    }
    PrismOrcaDBusInstance *h = nullptr;
    if (!api->prism_orca_create(&h)) {
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
    if (interrupt)
      if (const auto res = stop(); !res)
        return res;
    if (const auto res = api->prism_orca_speak(instance, text.data()); !res) {
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
    if (const auto res = api->prism_orca_stop(instance); !res) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(OrcaBackend, Backends::Orca, "Orca", 100);
#endif