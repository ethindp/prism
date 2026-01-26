// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#include "utils.h"
#if (defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) ||      \
     defined(__OpenBSD__) || defined(__DragonFly__)) &&                        \
    !defined(__ANDROID__)
#ifndef NO_ORCA
#include "orca-module.h"
#include "orca-service.h"
#include <gio/gio.h>

class OrcaBackend final : public TextToSpeechBackend {
private:
  GDBusConnection *conn{nullptr};
  OrcaServiceOrgGnomeOrcaService *service_proxy{nullptr};
  OrcaModuleOrgGnomeOrcaModule *module_proxy{nullptr};

public:
  ~OrcaBackend() override {
    if (module_proxy != nullptr) {
      g_object_unref(module_proxy);
      module_proxy = nullptr;
    }
    if (service_proxy != nullptr) {
      g_object_unref(service_proxy);
      service_proxy = nullptr;
    }
    if (conn != nullptr) {
      g_object_unref(conn);
      conn = nullptr;
    }
  }

  [[nodiscard]] std::string_view get_name() const override { return "Orca"; }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    GDBusConnection *temp_conn =
        g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
    if (temp_conn != nullptr) {
      GError *error = nullptr;
      GVariant *result = g_dbus_connection_call_sync(
          temp_conn, "org.freedesktop.DBus", "/org/freedesktop/DBus",
          "org.freedesktop.DBus", "NameHasOwner",
          g_variant_new("(s)", "org.gnome.Orca.Service"), G_VARIANT_TYPE("(b)"),
          G_DBUS_CALL_FLAGS_NONE, 100, nullptr, &error);
      bool exists = false;
      if (result != nullptr) {
        g_variant_get(result, "(b)", &exists);
        g_variant_unref(result);
        if (exists) {
          features |= IS_SUPPORTED_AT_RUNTIME;
        }
      }
      if (error != nullptr)
        g_error_free(error);
      g_object_unref(temp_conn);
    }
    features |= SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_STOP;
    return features;
  }

  BackendResult<> initialize() override {
    if (conn != nullptr && service_proxy != nullptr && module_proxy != nullptr)
      return std::unexpected(BackendError::AlreadyInitialized);
    GError *error = nullptr;
    conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (error != nullptr) {
      g_error_free(error);
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    service_proxy = orca_service_org_gnome_orca_service_proxy_new_sync(
        conn, G_DBUS_PROXY_FLAGS_NONE, "org.gnome.Orca.Service",
        "/org/gnome/Orca/Service", nullptr, &error);
    if (error != nullptr) {
      g_error_free(error);
      g_object_unref(conn);
      conn = nullptr;
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    module_proxy = orca_module_org_gnome_orca_module_proxy_new_sync(
        conn, G_DBUS_PROXY_FLAGS_NONE, "org.gnome.Orca.Service",
        "/org/gnome/Orca/Service/SpeechAndVerbosityManager", nullptr, &error);
    if (error != nullptr) {
      g_error_free(error);
      g_object_unref(service_proxy);
      service_proxy = nullptr;
      g_object_unref(conn);
      conn = nullptr;
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (conn == nullptr || service_proxy == nullptr || module_proxy == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (!simdutf::validate_utf8(text.data(), text.size())) {
      return std::unexpected(BackendError::InvalidUtf8);
    }
    if (interrupt)
      if (const auto res = stop(); !res)
        return res;
    GError *error = nullptr;
    gboolean success;
    const auto ok =
        orca_service_org_gnome_orca_service_call_present_message_sync(
            service_proxy, text.data(), &success, nullptr, &error);
    if (ok == 0 || success == 0 || error != nullptr) {
      if (error != nullptr)
        g_error_free(error);
      return std::unexpected(BackendError::SpeakFailure);
    }
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<> stop() override {
    if (conn == nullptr || service_proxy == nullptr || module_proxy == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    GError *error = nullptr;
    gboolean success;
    const auto ok = orca_module_org_gnome_orca_module_call_execute_command_sync(
        module_proxy, "InterruptSpeech", 0, &success, nullptr, &error);
    if (ok == 0 || success == 0 || error != nullptr) {
      if (error != nullptr)
        g_error_free(error);
      return std::unexpected(BackendError::SpeakFailure);
    }
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(OrcaBackend, Backends::Orca, "Orca", 100);
#endif
#endif