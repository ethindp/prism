// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <dbus/dbus.h>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace prism::dbus {
struct MessageDeleter {
  void operator()(DBusMessage *message) const noexcept;
};

using Message = std::unique_ptr<DBusMessage, MessageDeleter>;

class Connection {
  DBusConnection *handle{nullptr};

public:
  struct Error {
    std::string message;
  };
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
  template <class... Args>
  [[nodiscard]] std::expected<Message, Error>
  request(const char *service, const char *path, const char *interface,
          const char *method, int timeout_ms, const Args &...args) const {
    if (handle == nullptr)
      return std::unexpected(Error{"D-Bus connection is unavailable"});
    Message request(
        dbus_message_new_method_call(service, path, interface, method));
    if (!request)
      return std::unexpected(Error{"Could not allocate D-Bus method call"});
    DBusMessageIter iter;
    dbus_message_iter_init_append(request.get(), &iter);
    if (!(append(iter, args) && ...))
      return std::unexpected(Error{"Could not serialize D-Bus arguments"});
    return send(request.get(), timeout_ms);
  }

  template <class... Args>
  [[nodiscard]] Message call(const char *service, const char *path,
                             const char *interface, const char *method,
                             int timeout_ms, const Args &...args) const {
    auto result =
        request(service, path, interface, method, timeout_ms, args...);
    return result ? std::move(*result) : Message{};
  }

  [[nodiscard]] bool subscribe(std::string_view match_rule) const;
  [[nodiscard]] Message receive(int timeout_ms) const;

private:
  [[nodiscard]] std::expected<Message, Error> send(DBusMessage *request,
                                                   int timeout_ms) const;
  static bool append(DBusMessageIter &iter, std::string_view text);
  static bool append(DBusMessageIter &iter, const std::string &text);
  static bool append(DBusMessageIter &iter, const char *text);
  static bool append(DBusMessageIter &iter, bool value);
};
[[nodiscard]] bool is_signal(const Message &message, const char *interface,
                             const char *member, const char *path) noexcept;
[[nodiscard]] std::optional<bool> boolean(const Message &message);
[[nodiscard]] std::optional<std::vector<std::string>>
strings(const Message &message);
} // namespace prism::dbus
