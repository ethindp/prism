// SPDX-License-Identifier: MPL-2.0

#include "bridge.h"
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

typedef struct OrcaDialect OrcaDialect;
static gboolean probe_legacy_module(GDBusConnection *conn, const OrcaDialect *d,
                                    const char *path);
static gboolean probe_v1_speech_manager(GDBusConnection *conn,
                                        const OrcaDialect *d, const char *path);
typedef gboolean (*OrcaProbeFunc)(GDBusConnection *conn, const OrcaDialect *d,
                                  const char *path);

struct OrcaDialect {
  const char *bus_name;
  const char *service_path;
  const char *service_iface;
  const char *const *speech_path_candidates;
  const char *speech_iface;
  gboolean generic_dispatch;
  OrcaProbeFunc probe_speech_path;
};
static const char *const LEGACY_SPEECH_PATHS[] = {
    "/org/gnome/Orca/Service/SpeechAndVerbosityManager",
    "/org/gnome/Orca/Service/SpeechManager",
    NULL,
};
static const char *const V1_SPEECH_PATHS[] = {
    "/org/gnome/Orca1/Service/SpeechManager",
    NULL,
};
static const OrcaDialect ORCA_LEGACY_DIALECT = {
    .bus_name = "org.gnome.Orca.Service",
    .service_path = "/org/gnome/Orca/Service",
    .service_iface = "org.gnome.Orca.Service",
    .speech_path_candidates = LEGACY_SPEECH_PATHS,
    .speech_iface = "org.gnome.Orca.Module",
    .generic_dispatch = TRUE,
    .probe_speech_path = probe_legacy_module,
};
static const OrcaDialect ORCA_V1_DIALECT = {
    .bus_name = "org.gnome.Orca1.Service",
    .service_path = "/org/gnome/Orca1/Service",
    .service_iface = "org.gnome.Orca1.Service",
    .speech_path_candidates = V1_SPEECH_PATHS,
    .speech_iface = "org.gnome.Orca1.SpeechManager",
    .generic_dispatch = FALSE,
    .probe_speech_path = probe_v1_speech_manager,
};
static const OrcaDialect *const DIALECTS[] = {
    &ORCA_V1_DIALECT,
    &ORCA_LEGACY_DIALECT,
    NULL,
};
static const int DBUS_TIMEOUT_MS = 100;

struct PrismOrcaDBusInstance {
  GDBusConnection *conn;
  const OrcaDialect *dialect;
  const char *speech_path; /* points into one of the kXxxSpeechPaths arrays */
};

static gboolean name_has_owner(GDBusConnection *conn, const char *bus_name) {
  GError *err = NULL;
  GVariant *reply = g_dbus_connection_call_sync(
      conn, "org.freedesktop.DBus", "/org/freedesktop/DBus",
      "org.freedesktop.DBus", "NameHasOwner", g_variant_new("(s)", bus_name),
      G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, DBUS_TIMEOUT_MS, NULL,
      &err);
  if (err) {
    g_error_free(err);
  }
  if (!reply) {
    return FALSE;
  }
  gboolean owned = FALSE;
  g_variant_get(reply, "(b)", &owned);
  g_variant_unref(reply);
  return owned;
}

static gboolean probe_legacy_module(GDBusConnection *conn, const OrcaDialect *d,
                                    const char *path) {
  GError *err = NULL;
  GVariant *reply = g_dbus_connection_call_sync(
      conn, d->bus_name, path, d->speech_iface, "ListCommands", NULL, NULL,
      G_DBUS_CALL_FLAGS_NONE, DBUS_TIMEOUT_MS, NULL, &err);
  if (err) {
    g_error_free(err);
  }
  if (!reply) {
    return FALSE;
  }
  g_variant_unref(reply);
  return TRUE;
}

static gboolean probe_v1_speech_manager(GDBusConnection *conn,
                                        const OrcaDialect *d,
                                        const char *path) {
  GError *err = NULL;
  GVariant *reply = g_dbus_connection_call_sync(
      conn, d->bus_name, path, "org.freedesktop.DBus.Properties", "Get",
      g_variant_new("(ss)", d->speech_iface, "Rate"), NULL,
      G_DBUS_CALL_FLAGS_NONE, DBUS_TIMEOUT_MS, NULL, &err);
  if (err) {
    g_error_free(err);
  }
  if (!reply) {
    return FALSE;
  }
  g_variant_unref(reply);
  return TRUE;
}

static GDBusConnection *open_private_session_bus(void) {
  GError *err = NULL;
  gchar *addr = g_dbus_address_get_for_bus_sync(G_BUS_TYPE_SESSION, NULL, &err);
  if (err) {
    g_error_free(err);
    err = NULL;
  }
  if (!addr) {
    return NULL;
  }
  GDBusConnection *c = g_dbus_connection_new_for_address_sync(
      addr,
      G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
          G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
      NULL, NULL, &err);
  g_free(addr);
  if (err) {
    g_error_free(err);
  }
  return c;
}

bool prism_orca_available(void) {
  GError *err = NULL;
  GDBusConnection *c = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
  if (err) {
    g_error_free(err);
    err = NULL;
  }
  if (!c) {
    return false;
  }
  bool result = false;
  for (int i = 0; DIALECTS[i] != NULL; i++) {
    if (name_has_owner(c, DIALECTS[i]->bus_name)) {
      for (int j = 0; DIALECTS[i]->speech_path_candidates[j] != NULL; j++) {
        if (DIALECTS[i]->probe_speech_path(
                c, DIALECTS[i], DIALECTS[i]->speech_path_candidates[j])) {
          result = true;
          break;
        }
      }
      if (result) {
        break;
      }
    }
  }
  g_object_unref(c);
  return result;
}

bool prism_orca_create(PrismOrcaDBusInstance **out) {
  if (!out) {
    return false;
  }
  *out = NULL;
  PrismOrcaDBusInstance *inst = calloc(1, sizeof(*inst));
  if (!inst) {
    return false;
  }
  inst->conn = open_private_session_bus();
  if (!inst->conn) {
    free(inst);
    return false;
  }
  for (int i = 0; DIALECTS[i] != NULL; i++) {
    const OrcaDialect *d = DIALECTS[i];
    if (!name_has_owner(inst->conn, d->bus_name)) {
      continue;
    }
    for (int j = 0; d->speech_path_candidates[j] != NULL; j++) {
      if (d->probe_speech_path(inst->conn, d, d->speech_path_candidates[j])) {
        inst->dialect = d;
        inst->speech_path = d->speech_path_candidates[j];
        break;
      }
    }
    if (inst->dialect) {
      break;
    }
  }
  if (!inst->dialect) {
    g_dbus_connection_close_sync(inst->conn, NULL, NULL);
    g_object_unref(inst->conn);
    free(inst);
    return false;
  }
  *out = inst;
  return true;
}

void prism_orca_destroy(PrismOrcaDBusInstance *h) {
  if (!h) {
    return;
  }
  if (h->conn) {
    g_dbus_connection_close_sync(h->conn, NULL, NULL);
    g_object_unref(h->conn);
  }
  free(h);
}

bool prism_orca_speak(PrismOrcaDBusInstance *h, const char *text) {
  if (!h || !text || !h->conn || !h->dialect) {
    return false;
  }
  GError *err = NULL;
  GVariant *reply = g_dbus_connection_call_sync(
      h->conn, h->dialect->bus_name, h->dialect->service_path,
      h->dialect->service_iface, "PresentMessage", g_variant_new("(s)", text),
      G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, DBUS_TIMEOUT_MS, NULL,
      &err);
  if (err) {
    g_error_free(err);
  }
  if (!reply) {
    return false;
  }
  gboolean ok = FALSE;
  g_variant_get(reply, "(b)", &ok);
  g_variant_unref(reply);
  return ok ? true : false;
}

bool prism_orca_stop(PrismOrcaDBusInstance *h) {
  if (!h || !h->conn || !h->dialect || !h->speech_path) {
    return false;
  }
  GVariant *params;
  const char *method_name;
  if (h->dialect->generic_dispatch) {
    params = g_variant_new("(sb)", "InterruptSpeech", FALSE);
    method_name = "ExecuteCommand";
  } else {
    params = g_variant_new("(b)", FALSE);
    method_name = "InterruptSpeech";
  }
  GError *err = NULL;
  GVariant *reply = g_dbus_connection_call_sync(
      h->conn, h->dialect->bus_name, h->speech_path, h->dialect->speech_iface,
      method_name, params, G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE,
      DBUS_TIMEOUT_MS, NULL, &err);
  if (err) {
    g_error_free(err);
  }
  if (!reply) {
    return false;
  }
  gboolean ok = FALSE;
  g_variant_get(reply, "(b)", &ok);
  g_variant_unref(reply);
  return ok ? true : false;
}
