// SPDX-License-Identifier: MPL-2.0
#include "bus.h"

namespace prism::dbus::services::bus {
constexpr auto service_name = "org.freedesktop.DBus";
constexpr auto path = "/org/freedesktop/DBus";
constexpr auto interface = "org.freedesktop.DBus";

bool has_owner(const Connection &connection, const char *bus_name_to_check) {
  return boolean(connection.call(service_name, path, interface, "NameHasOwner",
                                 2500, bus_name_to_check))
      .value_or(false);
}

std::optional<std::vector<std::string>> names(const Connection &connection,
                                              bool activatable) {
  return strings(connection.call(
      service_name, path, interface,
      activatable ? "ListActivatableNames" : "ListNames", 2500));
}
} // namespace prism::dbus::services::bus
