// SPDX-License-Identifier: MPL-2.0
#include "bus.h"

namespace prism::dbus::services::bus {
namespace {
constexpr Object daemon{
    .service = "org.freedesktop.DBus",
    .path = "/org/freedesktop/DBus",
};
constexpr auto interface = "org.freedesktop.DBus";
constexpr Method<bool, std::string> name_has_owner{
    .interface = interface,
    .member = "NameHasOwner",
};
constexpr Method<std::vector<std::string>> list_names{
    .interface = interface,
    .member = "ListNames",
};
constexpr Method<std::vector<std::string>> list_activatable_names{
    .interface = interface,
    .member = "ListActivatableNames",
};
} // namespace

bool has_owner(const Connection &connection, const char *bus_name_to_check) {
  return connection.call(daemon, name_has_owner, bus_name_to_check)
      .value_or(false);
}

std::optional<std::vector<std::string>> names(const Connection &connection,
                                              bool activatable) {
  auto result = connection.call(daemon, activatable ? list_activatable_names
                                                    : list_names);
  if (!result)
    return std::nullopt;
  return std::move(*result);
}
} // namespace prism::dbus::services::bus
