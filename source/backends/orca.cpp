// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#include "utils.h"
#if defined(linux) || defined(__linux) || defined(__linux__) ||                \
    defined(__gnu_linux__) || defined(BSD) || defined(_SYSTYPE_BSD) ||         \
    defined(BSD4_2) || defined(BSD4_3) || defined(BSD4_4) || defined(BSD) ||   \
    defined(__bsdi__) || defined(__DragonFly__) || defined(__FreeBSD__) ||     \
    defined(__FreeBSD_version) || defined(__NETBSD__) ||                       \
    defined(__NetBSD__) || defined(__NETBSD_version) || defined(NetBSD0_8) ||  \
    defined(NetBSD0_9) || defined(NetBSD1_0) || defined(__NetBSD_Version) ||   \
    defined(__OpenBSD__)
#ifndef NO_ORCA
#include "raw/orca-module.h"
#include "raw/orca-service.h"
#include <gio/gio.h>

class OrcaBackend final : public TextToSpeechBackend {
private:
  GDBusConnection *conn;
  OrgGnomeOrcaService *service_proxy;
  OrgGnomeOrcaModule *module_proxy;

public:
  ~OrcaBackend() {
    if (module_proxy)
      g_object_unref(module_proxy);
    if (service_proxy)
      g_module_unref(service_proxy);
    if (conn)
      g_object_unref(conn);
  }

  std::string_view get_name() const override { return "Orca"; }

  BackendResult<> initialize() override {
    if (con && service_proxy && module_proxy)
      return std::unexpected(BackendError::AlreadyInitialized);
    GError *error = nullptr;
    conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (error) {
      g_error_free(error);
      return std::unexpected(BackendError::InternalBackendError);
    }
    service_proxy = org_gnome_orca_service_proxy_new_sync(
        conn, G_DBUS_PROXY_FLAGS_NONE, "org.gnome.Orca.Service",
        "/org/gnome/Orca/Service", nullptr, &error);
    if (error) {
      g_error_free(error);
      return std::unexpected(BackendError::InternalBackendError);
    }
    module_proxy = org_gnome_orca_module_proxy_new_sync(
        conn, G_DBUS_PROXY_FLAGS_NONE, "org.gnome.Orca.Service",
        "/org/gnome/Orca/Service/SpeechAndVerbosityManager", nullptr, &error);
    if (error) {
      g_error_free(error);
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!con || !service_proxy || !module_proxy)
      return std::unexpected(BackendError::NotInitialized);
    if (!simdutf::validate_utf8(text.data(), text.size())) {
      return std::unexpected(BackendError::InvalidUtf8);
    }
    if (interrupt)
      stop();
    GError *error = nullptr;
    gboolean success;
    const auto ok = org_gnome_orca_service_call_present_message_sync(
        service_proxy, text.data(), &success, nullptr, &error);
    if (!okay || !success || error) {
      if (error)
        g_error_free(error);
      return std::unexpected(BackendError::SpeakFailure);
    }
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<> stop() override {
    if (!con || !service_proxy || !module_proxy)
      return std::unexpected(BackendError::NotInitialized);
    GError *error = nullptr;
    gboolean success;
    const auto okay = org_gnome_orca_module_call_execute_command_sync(
        module_proxy, "InterruptSpeech", false, &success, nullptr, &error);
    if (!okay || !success || error) {
      if (error)
        g_error_free(error);
      return std::unexpected(BackendError::SpeakFailure);
    }
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(OrcaBackend, Backends::Orca, "Orca", 100);
#endif
#endif