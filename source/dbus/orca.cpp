// SPDX-License-Identifier: MPL-2.0
#include "orca.h"
#include "bus.h"
#include <array>

namespace prism::dbus::services::orca {
struct Dialect {
  const char *name;
  const char *service_path;
  const char *service_interface;
  const char *speech_interface;
  bool legacy;
  std::array<const char *, 2> speech_paths;
};

constexpr std::array dialects{
    Dialect{
        .name = "org.gnome.Orca1.Service",
        .service_path = "/org/gnome/Orca1/Service",
        .service_interface = "org.gnome.Orca1.Service",
        .speech_interface = "org.gnome.Orca1.SpeechManager",
        .legacy = false,
        .speech_paths = {"/org/gnome/Orca1/Service/SpeechManager", nullptr}},
    Dialect{
        .name = "org.gnome.Orca.Service",
        .service_path = "/org/gnome/Orca/Service",
        .service_interface = "org.gnome.Orca.Service",
        .speech_interface = "org.gnome.Orca.Module",
        .legacy = true,
        .speech_paths = {"/org/gnome/Orca/Service/SpeechAndVerbosityManager",
                         "/org/gnome/Orca/Service/SpeechManager"}}};

Service::Service(const Dialect &dialect_value, const char *path) noexcept
    : dialect(&dialect_value), speech_path(path) {}
std::optional<Service> Service::detect(const Connection &connection) {
  for (const auto &dialect : dialects) {
    if (!bus::has_owner(connection, dialect.name))
      continue;
    for (const auto *path : dialect.speech_paths) {
      if (path == nullptr)
        continue;
      const auto result =
          dialect.legacy
              ? connection.call(dialect.name, path, dialect.speech_interface,
                                "ListCommands", 2500)
              : connection.call(dialect.name, path,
                                "org.freedesktop.DBus.Properties", "Get", 2500,
                                dialect.speech_interface, "Rate");
      if (result)
        return Service(dialect, path);
    }
  }
  return std::nullopt;
}

std::expected<void, Connection::Error>
Service::present(const Connection &connection, std::string_view text) const {
  auto reply = connection.request(dialect->name, dialect->service_path,
                                  dialect->service_interface, "PresentMessage",
                                  2500, text);
  if (!reply)
    return std::unexpected(reply.error());
  const auto ok = boolean(*reply);
  if (!ok)
    return std::unexpected(
        Connection::Error{"Unexpected PresentMessage reply signature"});
  if (!*ok)
    return std::unexpected(Connection::Error{"Orca rejected PresentMessage"});
  return {};
}

std::expected<void, Connection::Error>
Service::interrupt(const Connection &connection) const {
  auto reply = dialect->legacy
                   ? connection.request(
                         dialect->name, speech_path, dialect->speech_interface,
                         "ExecuteCommand", 2500, "InterruptSpeech", false)
                   : connection.request(dialect->name, speech_path,
                                        dialect->speech_interface,
                                        "InterruptSpeech", 2500, false);
  if (!reply)
    return std::unexpected(reply.error());
  const auto ok = boolean(*reply);
  if (!ok)
    return std::unexpected(
        Connection::Error{"Unexpected interrupt reply signature"});
  if (!*ok)
    return std::unexpected(
        Connection::Error{"Orca rejected speech interruption"});
  return {};
}

} // namespace prism::dbus::services::orca
