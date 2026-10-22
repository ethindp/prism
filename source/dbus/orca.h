// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "client.h"
#include <expected>
#include <optional>
#include <string_view>

namespace prism::dbus::services::orca {
struct Dialect;
class Service {
  const Dialect *dialect;
  const char *speech_path;

public:
  Service(const Dialect &dialect_value, const char *path) noexcept;

  [[nodiscard]] static std::optional<Service>
  detect(const Connection &connection);
  [[nodiscard]] std::expected<void, Connection::Error>
  present(const Connection &connection, std::string_view text) const;
  [[nodiscard]] std::expected<void, Connection::Error>
  interrupt(const Connection &connection) const;
};
} // namespace prism::dbus::services::orca
