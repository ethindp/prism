// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "client.h"
#include <optional>
namespace prism::dbus::services::login1 {
[[nodiscard]] bool subscribe(const Connection &connection);
[[nodiscard]] std::optional<bool> prepare_for_sleep(const Message &message);
} // namespace prism::dbus::services::login1
