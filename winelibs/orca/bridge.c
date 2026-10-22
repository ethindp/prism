// SPDX-License-Identifier: MPL-2.0
#include "bridge.h"
#include <dbus/dbus.h>
#include <stdlib.h>

typedef struct {
  const char *name;
  const char *service_path;
  const char *service_interface;
  const char *speech_interface;
  const char *const *speech_paths;
  int legacy;
} OrcaDialect;

static const char *const v1_paths[] = {"/org/gnome/Orca1/Service/SpeechManager",
                                       NULL};
static const char *const legacy_paths[] = {
    "/org/gnome/Orca/Service/SpeechAndVerbosityManager",
    "/org/gnome/Orca/Service/SpeechManager", NULL};
static const OrcaDialect dialects[] = {
    {"org.gnome.Orca1.Service", "/org/gnome/Orca1/Service",
     "org.gnome.Orca1.Service", "org.gnome.Orca1.SpeechManager", v1_paths, 0},
    {"org.gnome.Orca.Service", "/org/gnome/Orca/Service",
     "org.gnome.Orca.Service", "org.gnome.Orca.Module", legacy_paths, 1}};

struct PrismOrcaDBusInstance {
  DBusConnection *connection;
  const OrcaDialect *dialect;
  const char *speech_path;
};

static void close_bus(DBusConnection *connection) {
  if (connection != NULL) {
    dbus_connection_close(connection);
    dbus_connection_unref(connection);
  }
}

static DBusConnection *open_bus(void) {
  if (dbus_threads_init_default() == FALSE)
    return NULL;
  DBusError error = DBUS_ERROR_INIT;
  DBusConnection *connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
  if (dbus_error_is_set(&error)) {
    dbus_error_free(&error);
    close_bus(connection);
    return NULL;
  }
  if (connection != NULL)
    dbus_connection_set_exit_on_disconnect(connection, FALSE);
  return connection;
}

static DBusMessage *invoke(DBusConnection *connection, const char *destination,
                           const char *path, const char *interface,
                           const char *method, const char *first,
                           const char *second, int with_false) {
  DBusMessage *request =
      dbus_message_new_method_call(destination, path, interface, method);
  if (request == NULL)
    return NULL;
  DBusMessageIter iter;
  dbus_message_iter_init_append(request, &iter);
  const char *strings[2] = {first, second};
  for (int i = 0; i != 2; ++i) {
    if (strings[i] != NULL &&
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &strings[i]) ==
            FALSE) {
      dbus_message_unref(request);
      return NULL;
    }
  }
  if (with_false) {
    dbus_bool_t value = FALSE;
    if (dbus_message_iter_append_basic(&iter, DBUS_TYPE_BOOLEAN, &value) ==
        FALSE) {
      dbus_message_unref(request);
      return NULL;
    }
  }
  DBusError error = DBUS_ERROR_INIT;
  DBusMessage *reply = dbus_connection_send_with_reply_and_block(
      connection, request, 2500, &error);
  dbus_message_unref(request);
  if (dbus_error_is_set(&error)) {
    dbus_error_free(&error);
    if (reply != NULL)
      dbus_message_unref(reply);
    return NULL;
  }
  if (reply != NULL &&
      dbus_message_get_type(reply) != DBUS_MESSAGE_TYPE_METHOD_RETURN) {
    dbus_message_unref(reply);
    return NULL;
  }
  return reply;
}

static int reply_bool(DBusMessage *reply) {
  if (reply == NULL || !dbus_message_has_signature(reply, "b"))
    return 0;
  DBusMessageIter iter;
  if (dbus_message_iter_init(reply, &iter) == FALSE)
    return 0;
  dbus_bool_t value = FALSE;
  dbus_message_iter_get_basic(&iter, &value);
  return value != FALSE;
}

static int has_owner(DBusConnection *connection, const char *name) {
  DBusMessage *reply =
      invoke(connection, "org.freedesktop.DBus", "/org/freedesktop/DBus",
             "org.freedesktop.DBus", "NameHasOwner", name, NULL, 0);
  int result = reply_bool(reply);
  if (reply != NULL)
    dbus_message_unref(reply);
  return result;
}

static const OrcaDialect *detect(DBusConnection *connection,
                                 const char **path) {
  for (unsigned i = 0; i < sizeof(dialects) / sizeof(dialects[0]); ++i) {
    const OrcaDialect *dialect = &dialects[i];
    if (!has_owner(connection, dialect->name))
      continue;
    for (const char *const *candidate = dialect->speech_paths;
         *candidate != NULL; ++candidate) {
      DBusMessage *reply =
          dialect->legacy
              ? invoke(connection, dialect->name, *candidate,
                       dialect->speech_interface, "ListCommands", NULL, NULL, 0)
              : invoke(connection, dialect->name, *candidate,
                       "org.freedesktop.DBus.Properties", "Get",
                       dialect->speech_interface, "Rate", 0);
      if (reply != NULL) {
        dbus_message_unref(reply);
        *path = *candidate;
        return dialect;
      }
    }
  }
  return NULL;
}

PRISM_WINELIB_ABI bool prism_orca_available(void) {
  DBusConnection *connection = open_bus();
  if (connection == NULL)
    return false;
  const char *path = NULL;
  bool available = detect(connection, &path) != NULL;
  close_bus(connection);
  return available;
}

PRISM_WINELIB_ABI bool prism_orca_create(PrismOrcaDBusInstance **out) {
  if (out == NULL)
    return false;
  *out = NULL;
  DBusConnection *connection = open_bus();
  if (connection == NULL)
    return false;
  const char *path = NULL;
  const OrcaDialect *dialect = detect(connection, &path);
  if (dialect == NULL) {
    close_bus(connection);
    return false;
  }
  PrismOrcaDBusInstance *instance = calloc(1, sizeof(*instance));
  if (instance == NULL) {
    close_bus(connection);
    return false;
  }
  instance->connection = connection;
  instance->dialect = dialect;
  instance->speech_path = path;
  *out = instance;
  return true;
}

PRISM_WINELIB_ABI void prism_orca_destroy(PrismOrcaDBusInstance *instance) {
  if (instance == NULL)
    return;
  close_bus(instance->connection);
  free(instance);
}

PRISM_WINELIB_ABI bool prism_orca_speak(PrismOrcaDBusInstance *instance,
                                        const char *text) {
  if (instance == NULL || text == NULL)
    return false;
  DBusMessage *reply = invoke(instance->connection, instance->dialect->name,
                              instance->dialect->service_path,
                              instance->dialect->service_interface,
                              "PresentMessage", text, NULL, 0);
  int result = reply_bool(reply);
  if (reply != NULL)
    dbus_message_unref(reply);
  return result != 0;
}

PRISM_WINELIB_ABI bool prism_orca_stop(PrismOrcaDBusInstance *instance) {
  if (instance == NULL)
    return false;
  const OrcaDialect *dialect = instance->dialect;
  DBusMessage *reply =
      dialect->legacy
          ? invoke(instance->connection, dialect->name, instance->speech_path,
                   dialect->speech_interface, "ExecuteCommand",
                   "InterruptSpeech", NULL, 1)
          : invoke(instance->connection, dialect->name, instance->speech_path,
                   dialect->speech_interface, "InterruptSpeech", NULL, NULL, 1);
  int result = reply_bool(reply);
  if (reply != NULL)
    dbus_message_unref(reply);
  return result != 0;
}
