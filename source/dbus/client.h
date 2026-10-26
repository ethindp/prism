// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <dbus/dbus.h>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace prism::dbus {
struct MessageDeleter {
  void operator()(DBusMessage *message) const noexcept;
};

using Message = std::unique_ptr<DBusMessage, MessageDeleter>;

struct Error {
  std::string message;
};

struct Any {};

template <class T> struct Codec;

template <> struct Codec<bool> {
  static constexpr std::string_view signature = "b";
  using Argument = bool;
  static bool append(DBusMessageIter &iter, bool value);
  static std::optional<bool> read(DBusMessageIter &iter);
};

template <> struct Codec<std::string> {
  static constexpr std::string_view signature = "s";
  using Argument = std::string_view;
  static bool append(DBusMessageIter &iter, std::string_view value);
  static std::optional<std::string> read(DBusMessageIter &iter);
};

template <> struct Codec<std::vector<std::string>> {
  static constexpr std::string_view signature = "as";
  static std::optional<std::vector<std::string>> read(DBusMessageIter &iter);
};

struct Object {
  const char *service;
  const char *path;
};

template <class Reply, class... Args> struct Method {
  const char *interface;
  const char *member;
};

template <class Payload> struct Signal {
  const char *sender;
  const char *path;
  const char *interface;
  const char *member;
};

class Connection {
  DBusConnection *handle{nullptr};

public:
  static constexpr int default_timeout_ms = 2500;

  Connection() = default;
  explicit Connection(DBusConnection *connection) noexcept;
  Connection(const Connection &) = delete;
  Connection &operator=(const Connection &) = delete;
  Connection(Connection &&other) noexcept;
  Connection &operator=(Connection &&other) noexcept;
  ~Connection();
  void reset() noexcept;
  explicit operator bool() const noexcept;
  [[nodiscard]] bool connected() const noexcept;
  [[nodiscard]] static Connection open(DBusBusType bus);

  template <class Reply, class... Args>
  [[nodiscard]] std::expected<Reply, Error>
  call(const Object &object, const Method<Reply, Args...> &method,
       Codec<Args>::Argument... args) const {
    if (handle == nullptr)
      return std::unexpected(Error{"D-Bus connection is unavailable"});
    Message request(dbus_message_new_method_call(
        object.service, object.path, method.interface, method.member));
    if (!request)
      return std::unexpected(Error{"Could not allocate D-Bus method call"});
    DBusMessageIter iter;
    dbus_message_iter_init_append(request.get(), &iter);
    if (!(Codec<Args>::append(iter, args) && ...))
      return std::unexpected(Error{"Could not serialize D-Bus arguments"});
    auto reply = send(request.get(), default_timeout_ms);
    if (!reply)
      return std::unexpected(std::move(reply.error()));
    return decode<Reply>(*reply);
  }

  template <class Payload>
  [[nodiscard]] bool subscribe(const Signal<Payload> &signal) const {
    return add_match(std::string("type='signal',sender='") + signal.sender +
                     "',interface='" + signal.interface + "',member='" +
                     signal.member + "',path='" + signal.path + "'");
  }

  [[nodiscard]] Message receive(int timeout_ms) const;

  template <class T>
  [[nodiscard]] static std::optional<T> arguments(const Message &message) {
    if (message == nullptr ||
        !has_signature(message.get(), Codec<T>::signature))
      return std::nullopt;
    DBusMessageIter iter;
    if (dbus_message_iter_init(message.get(), &iter) == FALSE)
      return std::nullopt;
    return Codec<T>::read(iter);
  }

  template <class Payload>
  [[nodiscard]] static std::optional<Payload>
  payload(const Message &message, const Signal<Payload> &signal) {
    if (message == nullptr ||
        dbus_message_is_signal(message.get(), signal.interface,
                               signal.member) == FALSE ||
        dbus_message_has_path(message.get(), signal.path) == FALSE)
      return std::nullopt;
    return arguments<Payload>(message);
  }

private:
  template <class Reply>
  static std::expected<Reply, Error> decode(const Message &reply) {
    if constexpr (std::is_same_v<Reply, Any>) {
      return Any{};
    } else if constexpr (std::is_void_v<Reply>) {
      if (!has_signature(reply.get(), ""))
        return std::unexpected(signature_error(reply.get(), ""));
      return {};
    } else {
      if (!has_signature(reply.get(), Codec<Reply>::signature))
        return std::unexpected(
            signature_error(reply.get(), Codec<Reply>::signature));
      auto value = arguments<Reply>(reply);
      if (!value)
        return std::unexpected(Error{"Malformed D-Bus reply"});
      return std::move(*value);
    }
  }

  [[nodiscard]] static bool has_signature(DBusMessage *message,
                                          std::string_view signature);
  [[nodiscard]] static Error signature_error(DBusMessage *message,
                                             std::string_view expected);
  [[nodiscard]] std::expected<Message, Error> send(DBusMessage *request,
                                                   int timeout_ms) const;
  [[nodiscard]] bool add_match(const std::string &rule) const;
};
} // namespace prism::dbus
