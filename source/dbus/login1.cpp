// SPDX-License-Identifier: MPL-2.0
#include "login1.h"

namespace prism::dbus::services::login1 {
constexpr auto signal_interface = "org.freedesktop.login1.Manager";
constexpr auto signal_name = "PrepareForSleep";
constexpr auto signal_path = "/org/freedesktop/login1";
constexpr auto match =
    "type='signal',sender='org.freedesktop.login1',"
    "interface='org.freedesktop.login1.Manager',"
    "member='PrepareForSleep',path='/org/freedesktop/login1'";

bool subscribe(const Connection &connection) {
  return connection.subscribe(match);
}
std::optional<bool> prepare_for_sleep(const Message &message) {
  if (!is_signal(message, signal_interface, signal_name, signal_path))
    return std::nullopt;
  return boolean(message);
}
} // namespace prism::dbus::services::login1
