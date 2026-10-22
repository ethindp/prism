// SPDX-License-Identifier: MPL-2.0
#include "spiel.h"
#include "bus.h"
#include <string_view>
namespace prism::dbus::services::spiel {
bool has_provider(const Connection &connection) {
  for (bool activatable : {true, false}) {
    const auto result = bus::names(connection, activatable);
    if (!result)
      continue;
    for (const auto &name : *result) {
      if (std::string_view(name).ends_with(".Speech.Provider"))
        return true;
    }
  }
  return false;
}
} // namespace prism::dbus::services::spiel
