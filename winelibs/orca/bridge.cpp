// SPDX-License-Identifier: MPL-2.0

#include "bridge.h"
#include "orca-module.h"
#include "orca-service.h"
#include <array>
#include <gio/gio.h>
#include <new>
#include <string_view>

static inline constexpr const char *ORCA_BUS_NAME = "org.gnome.Orca.Service";
static inline constexpr const char *ORCA_SERVICE_PATH =
    "/org/gnome/Orca/Service";
static inline constexpr auto ORCA_SPEECH_MANAGER_MODULE_PATHS =
    std::to_array<std::string_view>({
        "/org/gnome/Orca/Service/SpeechAndVerbosityManager",
        "/org/gnome/Orca/Service/SpeechManager",
    });
static inline constexpr int DBUS_TIMEOUT = 100;

struct Instance {
  GDBusConnection *conn = nullptr;
  OrcaServiceOrgGnomeOrcaService *service = nullptr;
  OrcaModuleOrgGnomeOrcaModule *module = nullptr;
};

static GDBusConnection *open_private_session_bus() {
  GError *err = nullptr;
  gchar *addr =
      g_dbus_address_get_for_bus_sync(G_BUS_TYPE_SESSION, nullptr, &err);
  if (!addr) {
    if (err)
      g_error_free(err);
    return nullptr;
  }
  GDBusConnection *c = g_dbus_connection_new_for_address_sync(
      addr,
      static_cast<GDBusConnectionFlags>(
          G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
          G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION),
      nullptr, nullptr, &err);
  g_free(addr);
  if (err)
    g_error_free(err);
  return c;
}

struct PrismOrcaDBusInstance : Instance {};

extern "C" bool prism_orca_available(void) {
  GError *err = nullptr;
  GDBusConnection *c = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &err);
  if (!c) {
    if (err)
      g_error_free(err);
    return false;
  }
  GVariant *reply = g_dbus_connection_call_sync(
      c, "org.freedesktop.DBus", "/org/freedesktop/DBus",
      "org.freedesktop.DBus", "NameHasOwner",
      g_variant_new("(s)", ORCA_BUS_NAME), G_VARIANT_TYPE("(b)"),
      G_DBUS_CALL_FLAGS_NONE, DBUS_TIMEOUT, nullptr, &err);
  bool result = 0;
  if (reply) {
    gboolean owned = FALSE;
    g_variant_get(reply, "(b)", &owned);
    g_variant_unref(reply);
    result = owned ? true : false;
  }
  if (err)
    g_error_free(err);
  g_object_unref(c); /* shared connection: unref only, never close */
  return result;
}

extern "C" bool prism_orca_create(PrismOrcaDBusInstance **out) {
  if (!out)
    return 0;
  *out = nullptr;
  auto *inst = new (std::nothrow) PrismOrcaDBusInstance();
  if (!inst)
    return false;
  inst->conn = open_private_session_bus();
  if (!inst->conn) {
    delete inst;
    return false;
  }
  GError *err = nullptr;
  inst->service = orca_service_org_gnome_orca_service_proxy_new_sync(
      inst->conn, G_DBUS_PROXY_FLAGS_NONE, ORCA_BUS_NAME, ORCA_SERVICE_PATH,
      nullptr, &err);
  if (err)
    g_error_free(err);
  if (!inst->service) {
    g_dbus_connection_close_sync(inst->conn, nullptr, nullptr);
    g_object_unref(inst->conn);
    delete inst;
    return false;
  }
  for (const auto &path : ORCA_SPEECH_MANAGER_MODULE_PATHS) {
    GError *e2 = nullptr;
    inst->module = orca_module_org_gnome_orca_module_proxy_new_sync(
        inst->conn, G_DBUS_PROXY_FLAGS_NONE, ORCA_BUS_NAME, path.data(),
        nullptr, &e2);
    if (e2)
      g_error_free(e2);
    if (inst->module)
      break;
  }
  if (!inst->module) {
    g_object_unref(inst->service);
    g_dbus_connection_close_sync(inst->conn, nullptr, nullptr);
    g_object_unref(inst->conn);
    delete inst;
    return 0;
  }
  *out = inst;
  return true;
}

extern "C" void prism_orca_destroy(PrismOrcaDBusInstance *h) {
  if (!h)
    return;
  if (h->module)
    g_object_unref(h->module);
  if (h->service)
    g_object_unref(h->service);
  if (h->conn) {
    g_dbus_connection_close_sync(h->conn, nullptr, nullptr);
    g_object_unref(h->conn);
  }
  delete h;
}

extern "C" bool prism_orca_speak(PrismOrcaDBusInstance *h, const char *text) {
  if (!h || !text)
    return false;
  GError *err = nullptr;
  gboolean ok = FALSE;
  gboolean rc = orca_service_org_gnome_orca_service_call_present_message_sync(
      h->service, text, &ok, nullptr, &err);
  if (err)
    g_error_free(err);
  return (rc && ok) ? true : false;
}

extern "C" bool prism_orca_stop(PrismOrcaDBusInstance *h) {
  if (!h)
    return false;
  GError *err = nullptr;
  gboolean ok = FALSE;
  gboolean rc = orca_module_org_gnome_orca_module_call_execute_command_sync(
      h->module, "InterruptSpeech", 0, &ok, nullptr, &err);
  if (err)
    g_error_free(err);
  return (rc && ok) ? true : false;
}