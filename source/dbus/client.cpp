// SPDX-License-Identifier: MPL-2.0
#include "client.h"

namespace prism::dbus {
void MessageDeleter::operator()(DBusMessage *message) const noexcept {
  if (message != nullptr)
    dbus_message_unref(message);
}

bool Codec<bool>::append(DBusMessageIter &iter, bool value) {
  const dbus_bool_t wire = value ? TRUE : FALSE;
  return dbus_message_iter_append_basic(&iter, DBUS_TYPE_BOOLEAN, &wire) !=
         FALSE;
}

std::optional<bool> Codec<bool>::read(DBusMessageIter &iter) {
  if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_BOOLEAN)
    return std::nullopt;
  dbus_bool_t value = FALSE;
  dbus_message_iter_get_basic(&iter, &value);
  return value != FALSE;
}

bool Codec<std::string>::append(DBusMessageIter &iter, std::string_view value) {
  if (value.contains('\0'))
    return false;
  const std::string owned(value);
  if (dbus_validate_utf8(owned.c_str(), nullptr) == FALSE)
    return false;
  const char *raw = owned.c_str();
  return dbus_message_iter_append_basic(
             &iter, DBUS_TYPE_STRING, static_cast<const void *>(&raw)) != FALSE;
}

std::optional<std::string> Codec<std::string>::read(DBusMessageIter &iter) {
  if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
    return std::nullopt;
  const char *value = nullptr;
  dbus_message_iter_get_basic(&iter, static_cast<void *>(&value));
  if (value == nullptr)
    return std::nullopt;
  return std::string(value);
}

std::optional<std::vector<std::string>>
Codec<std::vector<std::string>>::read(DBusMessageIter &iter) {
  if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY ||
      dbus_message_iter_get_element_type(&iter) != DBUS_TYPE_STRING)
    return std::nullopt;
  DBusMessageIter values;
  dbus_message_iter_recurse(&iter, &values);
  std::vector<std::string> result;
  while (dbus_message_iter_get_arg_type(&values) == DBUS_TYPE_STRING) {
    auto value = Codec<std::string>::read(values);
    if (!value)
      return std::nullopt;
    result.push_back(std::move(*value));
    dbus_message_iter_next(&values);
  }
  return result;
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

bool Connection::has_signature(DBusMessage *message,
                               std::string_view signature) {
  const char *actual = dbus_message_get_signature(message);
  return actual != nullptr && std::string_view(actual) == signature;
}

Error Connection::signature_error(DBusMessage *message,
                                  std::string_view expected) {
  const char *actual = dbus_message_get_signature(message);
  return Error{
      std::string("D-Bus reply has signature '") +
          (actual != nullptr ? actual : "") + "', expected '" +
          std::string(expected) + "'",
  };
}

std::expected<Message, Error> Connection::send(DBusMessage *request,
                                               int timeout_ms) const {
  DBusError error = DBUS_ERROR_INIT;
  Message reply(dbus_connection_send_with_reply_and_block(handle, request,
                                                          timeout_ms, &error));
  if (dbus_error_is_set(&error) != FALSE) {
    std::string reason = error.name != nullptr ? error.name : "D-Bus error";
    if (error.message != nullptr)
      reason.append(": ").append(error.message);
    dbus_error_free(&error);
    return std::unexpected(Error{std::move(reason)});
  }
  if (!reply)
    return std::unexpected(Error{"No reply from D-Bus service"});
  if (dbus_message_get_type(reply.get()) != DBUS_MESSAGE_TYPE_METHOD_RETURN)
    return std::unexpected(Error{"D-Bus call did not return successfully"});
  return reply;
}

bool Connection::add_match(const std::string &rule) const {
  if (handle == nullptr)
    return false;
  DBusError error = DBUS_ERROR_INIT;
  dbus_bus_add_match(handle, rule.c_str(), &error);
  if (dbus_error_is_set(&error) != FALSE) {
    dbus_error_free(&error);
    return false;
  }
  dbus_connection_flush(handle);
  return dbus_connection_get_is_connected(handle) != FALSE;
}

Message Connection::receive(int timeout_ms) const {
  if (handle == nullptr)
    return {};
  if (Message queued(dbus_connection_pop_message(handle)); queued)
    return queued;
  if (dbus_connection_read_write(handle, timeout_ms) == FALSE)
    return {};
  return Message(dbus_connection_pop_message(handle));
}
} // namespace prism::dbus
