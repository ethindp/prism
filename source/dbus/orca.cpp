// SPDX-License-Identifier: MPL-2.0
#include "orca.h"
#include "bus.h"
#include <array>

namespace prism::dbus::services::orca {
struct Dialect {
  Object service;
  const char *service_interface;
  const char *speech_interface;
  bool legacy;
  std::array<const char *, 2> speech_paths;
};

namespace {
constexpr std::array dialects{
    Dialect{
        .service =
            {
                .service = "org.gnome.Orca1.Service",
                .path = "/org/gnome/Orca1/Service",
            },
        .service_interface = "org.gnome.Orca1.Service",
        .speech_interface = "org.gnome.Orca1.SpeechManager",
        .legacy = false,
        .speech_paths = {"/org/gnome/Orca1/Service/SpeechManager", nullptr},
    },
    Dialect{
        .service =
            {
                .service = "org.gnome.Orca.Service",
                .path = "/org/gnome/Orca/Service",
            },
        .service_interface = "org.gnome.Orca.Service",
        .speech_interface = "org.gnome.Orca.Module",
        .legacy = true,
        .speech_paths =
            {
                "/org/gnome/Orca/Service/SpeechAndVerbosityManager",
                "/org/gnome/Orca/Service/SpeechManager",
            },
    },
};

using PresentMessage = Method<bool, std::string>;
using InterruptSpeech = Method<bool, bool>;
using ExecuteCommand = Method<bool, std::string, bool>;
using ListCommands = Method<Any>;
constexpr Method<Any, std::string, std::string> get_property{
    .interface = "org.freedesktop.DBus.Properties",
    .member = "Get",
};

std::expected<void, Error> accepted(std::expected<bool, Error> reply,
                                    const char *refusal) {
  if (!reply)
    return std::unexpected(std::move(reply.error()));
  if (!*reply)
    return std::unexpected(Error{refusal});
  return {};
}
} // namespace

Service::Service(const Dialect &dialect_value, const char *path) noexcept
    : dialect(&dialect_value), speech_path(path) {}

std::optional<Service> Service::detect(const Connection &connection) {
  for (const auto &dialect : dialects) {
    if (!bus::has_owner(connection, dialect.service.service))
      continue;
    for (const auto *path : dialect.speech_paths) {
      if (path == nullptr)
        continue;
      const Object speech{.service = dialect.service.service, .path = path};
      const bool answers =
          dialect.legacy ? connection
                               .call(speech,
                                     ListCommands{
                                         .interface = dialect.speech_interface,
                                         .member = "ListCommands",
                                     })
                               .has_value()
                         : connection
                               .call(speech, get_property,
                                     dialect.speech_interface, "Rate")
                               .has_value();
      if (answers)
        return Service(dialect, path);
    }
  }
  return std::nullopt;
}

std::expected<void, Error> Service::present(const Connection &connection,
                                            std::string_view text) const {
  return accepted(connection.call(dialect->service,
                                  PresentMessage{
                                      .interface = dialect->service_interface,
                                      .member = "PresentMessage",
                                  },
                                  text),
                  "Orca rejected PresentMessage");
}

std::expected<void, Error>
Service::interrupt(const Connection &connection) const {
  const Object speech{.service = dialect->service.service, .path = speech_path};
  auto reply = dialect->legacy
                   ? connection.call(speech,
                                     ExecuteCommand{
                                         .interface = dialect->speech_interface,
                                         .member = "ExecuteCommand",
                                     },
                                     "InterruptSpeech", false)
                   : connection.call(speech,
                                     InterruptSpeech{
                                         .interface = dialect->speech_interface,
                                         .member = "InterruptSpeech",
                                     },
                                     false);
  return accepted(std::move(reply), "Orca rejected speech interruption");
}
} // namespace prism::dbus::services::orca
