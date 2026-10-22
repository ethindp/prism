// SPDX-License-Identifier: MPL-2.0
#include "client.h"

namespace prism::dbus {
void MessageDeleter::operator()(DBusMessage *message) const noexcept {
  if (message != nullptr)
    dbus_message_unref(message);
}

Connection::Connection(DBusConnection *connection) noexcept
    : handle(connection) {}

Connection::Connection(Connection &&other) noexcept
    : handle(std::exchange(other.handle, nullptr)) {}

Connection &Connection::operator=(Connection &&other) noexcept {
  if (this != &other) {
    reset();
    handle = std::exchange(other.handle, nullptr);
  }
  return *this;
}

Connection::~Connection() { reset(); }

void Connection::reset() noexcept {
  if (auto *connection = std::exchange(handle, nullptr)) {
    dbus_connection_close(connection);
    dbus_connection_unref(connection);
  }
}

Connection::operator bool() const noexcept { return handle != nullptr; }

bool Connection::connected() const noexcept {
  return handle != nullptr && dbus_connection_get_is_connected(handle) != FALSE;
}

Connection Connection::open(DBusBusType bus) {
  static const bool threads_ready = dbus_threads_init_default() != FALSE;
  if (!threads_ready)
    return {};
  DBusError error = DBUS_ERROR_INIT;
  auto *raw = dbus_bus_get_private(bus, &error);
  if (dbus_error_is_set(&error) != FALSE) {
    dbus_error_free(&error);
    if (raw != nullptr) {
      dbus_connection_close(raw);
      dbus_connection_unref(raw);
    }
    return {};
  }
  if (raw != nullptr)
    dbus_connection_set_exit_on_disconnect(raw, FALSE);
  return Connection(raw);
}

std::expected<Message, Connection::Error>
Connection::send(DBusMessage *request, int timeout_ms) const {
  DBusError error = DBUS_ERROR_INIT;
  Message reply(dbus_connection_send_with_reply_and_block(handle, request,
                                                          timeout_ms, &error));
  if (dbus_error_is_set(&error) != FALSE) {
    std::string reason =
        error.message != nullptr ? error.message : "Unknown D-Bus error";
    dbus_error_free(&error);
    return std::unexpected(Error{std::move(reason)});
  }
  if (!reply)
    return std::unexpected(Error{"No reply from D-Bus service"});
  if (dbus_message_get_type(reply.get()) != DBUS_MESSAGE_TYPE_METHOD_RETURN)
    return std::unexpected(Error{"D-Bus call did not return successfully"});
  return reply;
}

bool Connection::append(DBusMessageIter &iter, std::string_view text) {
  if (text.contains('\0'))
    return false;
  std::string value(text);
  if (dbus_validate_utf8(value.c_str(), nullptr) == FALSE)
    return false;
  const char *raw = value.c_str();
  return dbus_message_iter_append_basic(
             &iter, DBUS_TYPE_STRING, static_cast<const void *>(&raw)) != FALSE;
}

bool Connection::append(DBusMessageIter &iter, const std::string &text) {
  return append(iter, std::string_view(text));
}

bool Connection::append(DBusMessageIter &iter, const char *text) {
  return text != nullptr && append(iter, std::string_view(text));
}

bool Connection::append(DBusMessageIter &iter, bool value) {
  dbus_bool_t b = value ? TRUE : FALSE;
  return dbus_message_iter_append_basic(&iter, DBUS_TYPE_BOOLEAN, &b) != FALSE;
}

bool Connection::subscribe(std::string_view match_rule) const {
  if (handle == nullptr)
    return false;
  const std::string rule(match_rule);
  DBusError error = DBUS_ERROR_INIT;
  dbus_bus_add_match(handle, rule.c_str(), &error);
  const bool ok = dbus_error_is_set(&error) == FALSE;
  if (!ok) {
    dbus_error_free(&error);
  } else {
    dbus_connection_flush(handle);
    if (dbus_connection_get_is_connected(handle) == FALSE)
      return false;
  }
  return ok;
}

Message Connection::receive(int timeout_ms) const {
  if (handle == nullptr ||
      dbus_connection_read_write(handle, timeout_ms) == FALSE)
    return {};
  return Message(dbus_connection_pop_message(handle));
}

bool is_signal(const Message &message, const char *interface,
               const char *member, const char *path) noexcept {
  return message != nullptr &&
         dbus_message_is_signal(message.get(), interface, member) != FALSE &&
         dbus_message_has_path(message.get(), path) != FALSE;
}

std::optional<bool> boolean(const Message &message) {
  if (message == nullptr ||
      dbus_message_has_signature(message.get(), "b") == FALSE)
    return std::nullopt;
  DBusMessageIter iter;
  if (dbus_message_iter_init(message.get(), &iter) == FALSE)
    return std::nullopt;
  dbus_bool_t value = FALSE;
  dbus_message_iter_get_basic(&iter, &value);
  return value != FALSE;
}

std::optional<std::vector<std::string>> strings(const Message &message) {
  if (message == nullptr ||
      dbus_message_has_signature(message.get(), "as") == FALSE)
    return std::nullopt;
  DBusMessageIter outer;
  DBusMessageIter values;
  if (dbus_message_iter_init(message.get(), &outer) == FALSE)
    return std::nullopt;
  dbus_message_iter_recurse(&outer, &values);
  std::vector<std::string> result;
  while (dbus_message_iter_get_arg_type(&values) == DBUS_TYPE_STRING) {
    const char *value = nullptr;
    dbus_message_iter_get_basic(&values, static_cast<void *>(&value));
    if (value == nullptr)
      return std::nullopt;
    result.emplace_back(value);
    if (dbus_message_iter_next(&values) == FALSE)
      break;
  }
  return result;
}
} // namespace prism::dbus
