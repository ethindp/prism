// SPDX-License-Identifier: MPL-2.0
#include "bridge.h"
#include "../../source/dbus/orca.h"
#include <memory>
#include <new>
#include <optional>

using prism::dbus::Connection;
using OrcaService = prism::dbus::services::orca::Service;

struct PrismOrcaDBusInstance {
  Connection connection;
  OrcaService service;
};

extern "C" PRISM_WINELIB_ABI bool prism_orca_available() noexcept {
  try {
    const auto bus = Connection::open(DBUS_BUS_SESSION);
    return bus && OrcaService::detect(bus).has_value();
  } catch (...) {
    return false;
  }
}

extern "C" PRISM_WINELIB_ABI bool
prism_orca_create(PrismOrcaDBusInstance **out) noexcept {
  if (out == nullptr)
    return false;
  *out = nullptr;
  try {
    auto bus = Connection::open(DBUS_BUS_SESSION);
    if (!bus)
      return false;
    auto service = OrcaService::detect(bus);
    if (!service)
      return false;
    *out = new PrismOrcaDBusInstance{
        .connection = std::move(bus),
        .service = *service,
    };
    return true;
  } catch (...) {
    return false;
  }
}

extern "C" PRISM_WINELIB_ABI void
prism_orca_destroy(PrismOrcaDBusInstance *h) noexcept {
  delete h;
}

extern "C" PRISM_WINELIB_ABI bool prism_orca_speak(PrismOrcaDBusInstance *h,
                                                   const char *text) noexcept {
  if (h == nullptr || text == nullptr)
    return false;
  try {
    return h->service.present(h->connection, text).has_value();
  } catch (...) {
    return false;
  }
}

extern "C" PRISM_WINELIB_ABI bool
prism_orca_stop(PrismOrcaDBusInstance *h) noexcept {
  if (h == nullptr)
    return false;
  try {
    return h->service.interrupt(h->connection).has_value();
  } catch (...) {
    return false;
  }
}
