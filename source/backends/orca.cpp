// SPDX-License-Identifier: MPL-2.0

#include "backend.h"
#include "backend_registry.h"
#include "utils.h"
#include <simdutf/simdutf.h>
#if (defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) ||      \
     defined(__OpenBSD__) || defined(__DragonFly__)) &&                        \
    !defined(__ANDROID__)
#ifndef NO_ORCA
#include <array>
#include <functional>
#include <giomm/dbusconnection.h>
#include <giomm/dbuserror.h>
#include <glibmm/error.h>
#include <glibmm/variant.h>
#include <optional>
#include <span>
#include <vector>

namespace {
struct OrcaDialect {
  const char *bus_name;
  const char *service_path;
  const char *service_iface;
  std::span<const char *const> speech_path_candidates;
  const char *speech_iface;
  bool generic_dispatch;
  bool (*probe_speech_path)(const Glib::RefPtr<Gio::DBus::Connection> &,
                            const OrcaDialect &, const char *path);
};

struct ResolvedDialect {
  const OrcaDialect *dialect;
  const char *speech_path;
};

bool probe_legacy_module(const Glib::RefPtr<Gio::DBus::Connection> &conn,
                         const OrcaDialect &d, const char *path) {
  try {
    conn->call_sync(path, d.speech_iface, "ListCommands",
                    Glib::VariantContainerBase::create_tuple(
                        std::vector<Glib::VariantBase>{}),
                    d.bus_name);
    return true;
  } catch (const Glib::Error &) {
    return false;
  }
}

bool probe_v1_speech_manager(const Glib::RefPtr<Gio::DBus::Connection> &conn,
                             const OrcaDialect &d, const char *path) {
  try {
    const auto params = Glib::VariantContainerBase::create_tuple({
        Glib::Variant<Glib::ustring>::create(d.speech_iface),
        Glib::Variant<Glib::ustring>::create("Rate"),
    });
    conn->call_sync(path, "org.freedesktop.DBus.Properties", "Get", params,
                    d.bus_name);
    return true;
  } catch (const Glib::Error &) {
    return false;
  }
}

bool name_has_owner(const Glib::RefPtr<Gio::DBus::Connection> &conn,
                    const char *bus_name) {
  try {
    const auto params = Glib::VariantContainerBase::create_tuple(
        Glib::Variant<Glib::ustring>::create(bus_name));
    const auto reply =
        conn->call_sync("/org/freedesktop/DBus", "org.freedesktop.DBus",
                        "NameHasOwner", params, "org.freedesktop.DBus");
    const auto child = Glib::VariantBase::cast_dynamic<Glib::Variant<bool>>(
        reply.get_child(0));
    return child.get();
  } catch (const Glib::Error &) {
    return false;
  }
}
} // namespace

class OrcaBackend final : public TextToSpeechBackend {
private:
  static constexpr auto LEGACY_SPEECH_PATHS = std::to_array<const char *>({
      "/org/gnome/Orca/Service/SpeechAndVerbosityManager",
      "/org/gnome/Orca/Service/SpeechManager",
  });
  static constexpr auto V1_SPEECH_PATHS = std::to_array<const char *>({
      "/org/gnome/Orca1/Service/SpeechManager",
  });
  static constexpr auto ORCA_LEGACY_DIALECT = OrcaDialect{
      .bus_name = "org.gnome.Orca.Service",
      .service_path = "/org/gnome/Orca/Service",
      .service_iface = "org.gnome.Orca.Service",
      .speech_path_candidates = LEGACY_SPEECH_PATHS,
      .speech_iface = "org.gnome.Orca.Module",
      .generic_dispatch = true,
      .probe_speech_path = &probe_legacy_module,
  };
  static constexpr auto ORCA_V1_DIALECT = OrcaDialect{
      .bus_name = "org.gnome.Orca1.Service",
      .service_path = "/org/gnome/Orca1/Service",
      .service_iface = "org.gnome.Orca1.Service",
      .speech_path_candidates = V1_SPEECH_PATHS,
      .speech_iface = "org.gnome.Orca1.SpeechManager",
      .generic_dispatch = false,
      .probe_speech_path = &probe_v1_speech_manager,
  };
  Glib::RefPtr<Gio::DBus::Connection> conn;
  const OrcaDialect *dialect{nullptr};
  const char *speech_path{nullptr};

  static std::optional<ResolvedDialect>
  detect_orca_dialect(const Glib::RefPtr<Gio::DBus::Connection> &conn) {
    for (const OrcaDialect *d : {&ORCA_V1_DIALECT, &ORCA_LEGACY_DIALECT}) {
      if (!name_has_owner(conn, d->bus_name)) {
        continue;
      }
      for (const char *candidate : d->speech_path_candidates) {
        if (d->probe_speech_path(conn, *d, candidate)) {
          return ResolvedDialect{
              .dialect = d,
              .speech_path = candidate,
          };
        }
      }
    }
    return std::nullopt;
  }

public:
  ~OrcaBackend() override = default;

  [[nodiscard]] std::string_view get_name() const override { return "Orca"; }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    features |= SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_STOP;
    try {
      const auto probe_conn =
          Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SESSION);
      if (probe_conn && detect_orca_dialect(probe_conn).has_value()) {
        features |= IS_SUPPORTED_AT_RUNTIME;
      }
    } catch (const Glib::Error &) {
      features &= ~IS_SUPPORTED_AT_RUNTIME;
    }
    return features;
  }

  BackendResult<> initialize() override {
    if (conn && dialect != nullptr) {
      return std::unexpected(BackendError::AlreadyInitialized);
    }
    try {
      conn = Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SESSION);
    } catch (const Glib::Error &) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    if (!conn) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    const auto resolved = detect_orca_dialect(conn);
    if (!resolved) {
      conn.reset();
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    dialect = resolved->dialect;
    speech_path = resolved->speech_path;
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!conn || dialect == nullptr) {
      return std::unexpected(BackendError::NotInitialized);
    }
    if (interrupt) {
      if (const auto res = stop(); !res) {
        return res;
      }
    }
    try {
      const auto params = Glib::VariantContainerBase::create_tuple(
          Glib::Variant<Glib::ustring>::create(
              Glib::ustring(std::string(text.data(), text.size()))));
      const auto reply =
          conn->call_sync(dialect->service_path, dialect->service_iface,
                          "PresentMessage", params, dialect->bus_name);
      const auto ok = Glib::VariantBase::cast_dynamic<Glib::Variant<bool>>(
          reply.get_child(0));
      if (!ok.get()) {
        return std::unexpected(BackendError::SpeakFailure);
      }
    } catch (const Glib::Error &) {
      return std::unexpected(BackendError::SpeakFailure);
    }
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<> stop() override {
    if (!conn || dialect == nullptr) {
      return std::unexpected(BackendError::NotInitialized);
    }
    try {
      Glib::VariantContainerBase params;
      std::string method_name;
      if (dialect->generic_dispatch) {
        params = Glib::VariantContainerBase::create_tuple({
            Glib::Variant<Glib::ustring>::create("InterruptSpeech"),
            Glib::Variant<bool>::create(false),
        });
        method_name = "ExecuteCommand";
      } else {
        params = Glib::VariantContainerBase::create_tuple(
            Glib::Variant<bool>::create(false));
        method_name = "InterruptSpeech";
      }
      const auto reply =
          conn->call_sync(speech_path, dialect->speech_iface, method_name,
                          params, dialect->bus_name);
      const auto ok = Glib::VariantBase::cast_dynamic<Glib::Variant<bool>>(
          reply.get_child(0));
      if (!ok.get()) {
        return std::unexpected(BackendError::SpeakFailure);
      }
    } catch (const Glib::Error &) {
      return std::unexpected(BackendError::SpeakFailure);
    }
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(OrcaBackend, Backends::Orca, "Orca", 100);
#endif
#elifdef _WIN32
#include <atomic>
#include <raw/prism_orca_bridge.h>
#include <tchar.h>
#include <windows.h>

class OrcaBackend final : public TextToSpeechBackend {
private:
  std::atomic<PrismOrcaDBusInstance *> instance{nullptr};

public:
  ~OrcaBackend() override {
    if (instance != nullptr) {
      prism_orca_destroy(instance);
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
        if (prism_orca_available()) {
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
        if (!prism_orca_available()) {
          return std::unexpected(BackendError::BackendNotAvailable);
        }
      }
    }
    PrismOrcaDBusInstance *h = nullptr;
    if (!prism_orca_create(&h)) {
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
    if (const auto res = prism_orca_speak(instance, text.data()); !res) {
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
    if (const auto res = prism_orca_stop(instance); !res) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(OrcaBackend, Backends::Orca, "Orca", 100);
#endif