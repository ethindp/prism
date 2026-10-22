// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "client.h"
#include <optional>
#include <string>
#include <vector>

namespace prism::dbus::services::bus {
[[nodiscard]] bool has_owner(const Connection &connection,
                             const char *bus_name_to_check);
[[nodiscard]] std::optional<std::vector<std::string>>
names(const Connection &connection, bool activatable);
} // namespace prism::dbus::services::bus
