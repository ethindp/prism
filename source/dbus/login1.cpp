// SPDX-License-Identifier: MPL-2.0
#include "login1.h"

namespace prism::dbus::services::login1 {
namespace {
constexpr Signal<bool> prepare_for_sleep_signal{
    .sender = "org.freedesktop.login1",
    .path = "/org/freedesktop/login1",
    .interface = "org.freedesktop.login1.Manager",
    .member = "PrepareForSleep",
};
} // namespace

bool subscribe(const Connection &connection) {
  return connection.subscribe(prepare_for_sleep_signal);
}

std::optional<bool> prepare_for_sleep(const Message &message) {
  return Connection::payload(message, prepare_for_sleep_signal);
}
} // namespace prism::dbus::services::login1
