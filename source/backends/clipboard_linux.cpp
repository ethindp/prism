// SPDX-License-Identifier: MPL-2.0

#if defined(__unix__) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
#include "../backend.h"
#include "../backend_catalog.h"
#include "../logging.h"
#include "../shared_library.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <memory>
#include <optional>
#include <poll.h>
#include <pthread.h>
#include <string>
#include <string_view>
#include <sys/eventfd.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <variant>
#include <version>
#include <wayland-client-core.h>
#include <xcb/xcb.h>

namespace {
const LogSource &clipboard_log() {
  static const LogSource log{"Clipboard"};
  return log;
}

struct WaylandClient {
  SharedLibrary library;
  decltype(&::wl_display_connect) wl_display_connect = nullptr;
  decltype(&::wl_display_disconnect) wl_display_disconnect = nullptr;
  decltype(&::wl_display_get_fd) wl_display_get_fd = nullptr;
  decltype(&::wl_display_roundtrip) wl_display_roundtrip = nullptr;
  decltype(&::wl_display_flush) wl_display_flush = nullptr;
  decltype(&::wl_display_prepare_read) wl_display_prepare_read = nullptr;
  decltype(&::wl_display_read_events) wl_display_read_events = nullptr;
  decltype(&::wl_display_cancel_read) wl_display_cancel_read = nullptr;
  decltype(&::wl_display_dispatch_pending) wl_display_dispatch_pending =
      nullptr;
  decltype(&::wl_display_get_error) wl_display_get_error = nullptr;
  decltype(&::wl_proxy_marshal_flags) wl_proxy_marshal_flags = nullptr;
  decltype(&::wl_proxy_add_listener) wl_proxy_add_listener = nullptr;
  decltype(&::wl_proxy_destroy) wl_proxy_destroy = nullptr;
  decltype(&::wl_proxy_get_version) wl_proxy_get_version = nullptr;
  decltype(&::wl_proxy_set_user_data) wl_proxy_set_user_data = nullptr;
  decltype(&::wl_proxy_get_user_data) wl_proxy_get_user_data = nullptr;
};

std::optional<WaylandClient> load_wayland_client() {
  WaylandClient api{.library = SharedLibrary{"libwayland-client.so.0"}};
  if (!api.library) {
    clipboard_log().debug("libwayland-client.so.0 could not be loaded: {}",
                          SharedLibrary::last_error());
    return std::nullopt;
  }
  const SharedLibrary &library = api.library;
  const bool complete =
      library.bind(api.wl_display_connect, "wl_display_connect") &&
      library.bind(api.wl_display_disconnect, "wl_display_disconnect") &&
      library.bind(api.wl_display_get_fd, "wl_display_get_fd") &&
      library.bind(api.wl_display_roundtrip, "wl_display_roundtrip") &&
      library.bind(api.wl_display_flush, "wl_display_flush") &&
      library.bind(api.wl_display_prepare_read, "wl_display_prepare_read") &&
      library.bind(api.wl_display_read_events, "wl_display_read_events") &&
      library.bind(api.wl_display_cancel_read, "wl_display_cancel_read") &&
      library.bind(api.wl_display_dispatch_pending,
                   "wl_display_dispatch_pending") &&
      library.bind(api.wl_display_get_error, "wl_display_get_error") &&
      library.bind(api.wl_proxy_marshal_flags, "wl_proxy_marshal_flags") &&
      library.bind(api.wl_proxy_add_listener, "wl_proxy_add_listener") &&
      library.bind(api.wl_proxy_destroy, "wl_proxy_destroy") &&
      library.bind(api.wl_proxy_get_version, "wl_proxy_get_version") &&
      library.bind(api.wl_proxy_set_user_data, "wl_proxy_set_user_data") &&
      library.bind(api.wl_proxy_get_user_data, "wl_proxy_get_user_data");
  if (!complete) {
    clipboard_log().debug(
        "libwayland-client.so.0 is missing a function prism needs");
    return std::nullopt;
  }
  return api;
}

const WaylandClient *wayland_client() {
  static const std::optional<WaylandClient> api = load_wayland_client();
  return api ? &*api : nullptr;
}
} // namespace

// The generated protocol headers call libwayland-client directly, so their
// calls are sent to the loaded library instead.
#define wl_display_connect (wayland_client()->wl_display_connect)
#define wl_display_disconnect (wayland_client()->wl_display_disconnect)
#define wl_display_get_fd (wayland_client()->wl_display_get_fd)
#define wl_display_roundtrip (wayland_client()->wl_display_roundtrip)
#define wl_display_flush (wayland_client()->wl_display_flush)
#define wl_display_prepare_read (wayland_client()->wl_display_prepare_read)
#define wl_display_read_events (wayland_client()->wl_display_read_events)
#define wl_display_cancel_read (wayland_client()->wl_display_cancel_read)
#define wl_display_dispatch_pending                                            \
  (wayland_client()->wl_display_dispatch_pending)
#define wl_display_get_error (wayland_client()->wl_display_get_error)
#define wl_proxy_marshal_flags (wayland_client()->wl_proxy_marshal_flags)
#define wl_proxy_add_listener (wayland_client()->wl_proxy_add_listener)
#define wl_proxy_destroy (wayland_client()->wl_proxy_destroy)
#define wl_proxy_get_version (wayland_client()->wl_proxy_get_version)
#define wl_proxy_set_user_data (wayland_client()->wl_proxy_set_user_data)
#define wl_proxy_get_user_data (wayland_client()->wl_proxy_get_user_data)

#include "ext-data-control-v1-client-protocol.h"
#include "wlr-data-control-unstable-v1-client-protocol.h"
#include <wayland-client-protocol.h>

namespace {
struct Xcb {
  SharedLibrary library;
  decltype(&::xcb_connect) xcb_connect = nullptr;
  decltype(&::xcb_disconnect) xcb_disconnect = nullptr;
  decltype(&::xcb_connection_has_error) xcb_connection_has_error = nullptr;
  decltype(&::xcb_get_setup) xcb_get_setup = nullptr;
  decltype(&::xcb_setup_roots_iterator) xcb_setup_roots_iterator = nullptr;
  decltype(&::xcb_screen_next) xcb_screen_next = nullptr;
  decltype(&::xcb_generate_id) xcb_generate_id = nullptr;
  decltype(&::xcb_create_window) xcb_create_window = nullptr;
  decltype(&::xcb_destroy_window) xcb_destroy_window = nullptr;
  decltype(&::xcb_intern_atom) xcb_intern_atom = nullptr;
  decltype(&::xcb_intern_atom_reply) xcb_intern_atom_reply = nullptr;
  decltype(&::xcb_set_selection_owner) xcb_set_selection_owner = nullptr;
  decltype(&::xcb_get_selection_owner) xcb_get_selection_owner = nullptr;
  decltype(&::xcb_get_selection_owner_reply) xcb_get_selection_owner_reply =
      nullptr;
  decltype(&::xcb_change_property) xcb_change_property = nullptr;
  decltype(&::xcb_change_window_attributes) xcb_change_window_attributes =
      nullptr;
  decltype(&::xcb_send_event) xcb_send_event = nullptr;
  decltype(&::xcb_convert_selection) xcb_convert_selection = nullptr;
  decltype(&::xcb_flush) xcb_flush = nullptr;
  decltype(&::xcb_poll_for_event) xcb_poll_for_event = nullptr;
  decltype(&::xcb_get_file_descriptor) xcb_get_file_descriptor = nullptr;
  decltype(&::xcb_get_maximum_request_length) xcb_get_maximum_request_length =
      nullptr;
};

std::optional<Xcb> load_xcb() {
  Xcb api{.library = SharedLibrary{"libxcb.so.1"}};
  if (!api.library) {
    clipboard_log().debug("libxcb.so.1 could not be loaded: {}",
                          SharedLibrary::last_error());
    return std::nullopt;
  }
  const SharedLibrary &library = api.library;
  const bool complete =
      library.bind(api.xcb_connect, "xcb_connect") &&
      library.bind(api.xcb_disconnect, "xcb_disconnect") &&
      library.bind(api.xcb_connection_has_error, "xcb_connection_has_error") &&
      library.bind(api.xcb_get_setup, "xcb_get_setup") &&
      library.bind(api.xcb_setup_roots_iterator, "xcb_setup_roots_iterator") &&
      library.bind(api.xcb_screen_next, "xcb_screen_next") &&
      library.bind(api.xcb_generate_id, "xcb_generate_id") &&
      library.bind(api.xcb_create_window, "xcb_create_window") &&
      library.bind(api.xcb_destroy_window, "xcb_destroy_window") &&
      library.bind(api.xcb_intern_atom, "xcb_intern_atom") &&
      library.bind(api.xcb_intern_atom_reply, "xcb_intern_atom_reply") &&
      library.bind(api.xcb_set_selection_owner, "xcb_set_selection_owner") &&
      library.bind(api.xcb_get_selection_owner, "xcb_get_selection_owner") &&
      library.bind(api.xcb_get_selection_owner_reply,
                   "xcb_get_selection_owner_reply") &&
      library.bind(api.xcb_change_property, "xcb_change_property") &&
      library.bind(api.xcb_change_window_attributes,
                   "xcb_change_window_attributes") &&
      library.bind(api.xcb_send_event, "xcb_send_event") &&
      library.bind(api.xcb_convert_selection, "xcb_convert_selection") &&
      library.bind(api.xcb_flush, "xcb_flush") &&
      library.bind(api.xcb_poll_for_event, "xcb_poll_for_event") &&
      library.bind(api.xcb_get_file_descriptor, "xcb_get_file_descriptor") &&
      library.bind(api.xcb_get_maximum_request_length,
                   "xcb_get_maximum_request_length");
  if (!complete) {
    clipboard_log().debug("libxcb.so.1 is missing a function prism needs");
    return std::nullopt;
  }
  return api;
}

const Xcb *xcb() {
  static const std::optional<Xcb> api = load_xcb();
  return api ? &*api : nullptr;
}

bool environment_has(const char *variable) {
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  const char *value = std::getenv(variable);
  return value != nullptr && *value != '\0';
}

// Keeps host signal handlers off the serving threads, and turns SIGPIPE from a
// closed reader into EPIPE.
void block_signals() {
  sigset_t all{};
  sigfillset(&all);
  pthread_sigmask(SIG_BLOCK, &all, nullptr);
}

bool has_event(const pollfd &fd, short event) {
  return (static_cast<unsigned>(fd.revents) & static_cast<unsigned>(event)) !=
         0;
}

static_assert(std::atomic<std::string *>::is_always_lock_free &&
              std::atomic<bool>::is_always_lock_free);

class WaylandClipboard {
private:
  static constexpr std::array<const char *, 5> mime_types{
      "text/plain;charset=utf-8", "text/plain", "UTF8_STRING", "TEXT", "STRING",
  };
  static constexpr auto send_timeout = std::chrono::seconds(2);

  wl_display *display = nullptr;
  wl_registry *registry = nullptr;
  wl_seat *seat = nullptr;
  std::uint32_t ext_manager_name = 0;
  std::uint32_t wlr_manager_name = 0;
  ext_data_control_manager_v1 *manager = nullptr;
  ext_data_control_device_v1 *device = nullptr;
  std::map<ext_data_control_source_v1 *, std::string> sources;
  int wake_fd = -1;
  std::atomic<std::string *> pending{nullptr};
  std::atomic<bool> serving{false};
  std::jthread thread;

  static void on_global(void *data, wl_registry *registry, std::uint32_t name,
                        const char *interface, std::uint32_t version) {
    auto *self = static_cast<WaylandClipboard *>(data);
    const std::string_view kind{interface};
    if (kind == wl_seat_interface.name && self->seat == nullptr)
      self->seat = static_cast<wl_seat *>(
          wl_registry_bind(registry, name, &wl_seat_interface, 1));
    else if (kind == ext_data_control_manager_v1_interface.name && version >= 1)
      self->ext_manager_name = name;
    else if (kind == zwlr_data_control_manager_v1_interface.name &&
             version >= 1)
      self->wlr_manager_name = name;
  }

  static void on_global_remove([[maybe_unused]] void *data,
                               [[maybe_unused]] wl_registry *registry,
                               [[maybe_unused]] std::uint32_t name) {}

  static constexpr wl_registry_listener registry_listener{
      .global = &on_global,
      .global_remove = &on_global_remove,
  };

  static void on_data_offer([[maybe_unused]] void *data,
                            [[maybe_unused]] ext_data_control_device_v1 *device,
                            [[maybe_unused]] ext_data_control_offer_v1 *offer) {
  }

  static void on_selection([[maybe_unused]] void *data,
                           [[maybe_unused]] ext_data_control_device_v1 *device,
                           ext_data_control_offer_v1 *offer) {
    if (offer != nullptr)
      ext_data_control_offer_v1_destroy(offer);
  }

  static void on_finished(void *data, ext_data_control_device_v1 *device) {
    ext_data_control_device_v1_destroy(device);
    static_cast<WaylandClipboard *>(data)->device = nullptr;
  }

  static constexpr ext_data_control_device_v1_listener device_listener{
      .data_offer = &on_data_offer,
      .selection = &on_selection,
      .finished = &on_finished,
      .primary_selection = &on_selection,
  };

  static void on_send(void *data, ext_data_control_source_v1 *source,
                      [[maybe_unused]] const char *mime_type, std::int32_t fd) {
    const auto &sources = static_cast<WaylandClipboard *>(data)->sources;
    if (const auto found = sources.find(source); found != sources.end())
      write_all(fd, found->second);
    close(fd);
  }

  static void on_cancelled(void *data, ext_data_control_source_v1 *source) {
    static_cast<WaylandClipboard *>(data)->sources.erase(source);
    ext_data_control_source_v1_destroy(source);
  }

  static constexpr ext_data_control_source_v1_listener source_listener{
      .send = &on_send,
      .cancelled = &on_cancelled,
  };

  static void write_all(int fd, std::string_view data) {
    const int flags = fcntl(fd, F_GETFL);
    if (flags < 0 ||
        fcntl(fd, F_SETFL,
              static_cast<int>(static_cast<unsigned>(flags) |
                               static_cast<unsigned>(O_NONBLOCK))) < 0)
      return;
    const auto deadline = std::chrono::steady_clock::now() + send_timeout;
    while (!data.empty()) {
      const auto written = write(fd, data.data(), data.size());
      if (written > 0) {
        data.remove_prefix(static_cast<std::size_t>(written));
        continue;
      }
      if (written < 0 && errno == EINTR)
        continue;
      if (written == 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
        return;
      const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
          deadline - std::chrono::steady_clock::now());
      if (left.count() <= 0)
        return;
      pollfd ready{.fd = fd, .events = POLLOUT, .revents = 0};
      if (poll(&ready, 1, static_cast<int>(left.count())) < 0 && errno != EINTR)
        return;
    }
  }

  void wake() const {
    if (wake_fd >= 0)
      eventfd_write(wake_fd, 1);
  }

  void publish(std::string text) {
    auto *const source =
        ext_data_control_manager_v1_create_data_source(manager);
    if (source == nullptr)
      return;
    sources.emplace(source, std::move(text));
    ext_data_control_source_v1_add_listener(source, &source_listener, this);
    for (const char *mime_type : mime_types)
      ext_data_control_source_v1_offer(source, mime_type);
    ext_data_control_device_v1_set_selection(device, source);
  }

  void serve(const std::stop_token &stop) {
    const std::array<pollfd, 2> watched{
        {
            {.fd = wl_display_get_fd(display), .events = POLLIN, .revents = 0},
            {.fd = wake_fd, .events = POLLIN, .revents = 0},
        },
    };
    while (!stop.stop_requested()) {
      while (wl_display_prepare_read(display) != 0)
        if (wl_display_dispatch_pending(display) < 0)
          return;
      wl_display_flush(display);
      auto ready = watched;
      if (poll(ready.data(), ready.size(), -1) < 0) {
        wl_display_cancel_read(display);
        if (errno == EINTR)
          continue;
        return;
      }
      if (has_event(ready[0], POLLIN)) {
        if (wl_display_read_events(display) < 0)
          return;
      } else {
        wl_display_cancel_read(display);
        if (has_event(ready[0], POLLERR) || has_event(ready[0], POLLHUP))
          return;
      }
      if (has_event(ready[1], POLLIN)) {
        eventfd_t count = 0;
        eventfd_read(wake_fd, &count);
      }
      if (wl_display_dispatch_pending(display) < 0 || device == nullptr)
        return;
      if (const std::unique_ptr<std::string> text{
              pending.exchange(nullptr, std::memory_order_acquire)})
        publish(std::move(*text));
    }
  }

  bool connect() {
    if (!environment_has("WAYLAND_DISPLAY") || wayland_client() == nullptr)
      return false;
    display = wl_display_connect(nullptr);
    if (display == nullptr)
      return false;
    registry = wl_display_get_registry(display);
    if (registry == nullptr)
      return false;
    wl_registry_add_listener(registry, &registry_listener, this);
    if (wl_display_roundtrip(display) < 0 || seat == nullptr)
      return false;
    if (ext_manager_name == 0 && wlr_manager_name == 0) {
      clipboard_log().debug(
          "the Wayland compositor offers no data control protocol");
      return false;
    }
    return true;
  }

public:
  WaylandClipboard() = default;
  WaylandClipboard(const WaylandClipboard &) = delete;
  WaylandClipboard &operator=(const WaylandClipboard &) = delete;
  WaylandClipboard(WaylandClipboard &&) = delete;
  WaylandClipboard &operator=(WaylandClipboard &&) = delete;

  ~WaylandClipboard() {
    if (thread.joinable()) {
      thread.request_stop();
      wake();
      thread.join();
    }
    for (const auto &entry : sources)
      ext_data_control_source_v1_destroy(entry.first);
    if (device != nullptr)
      ext_data_control_device_v1_destroy(device);
    if (manager != nullptr)
      ext_data_control_manager_v1_destroy(manager);
    if (seat != nullptr)
      wl_seat_destroy(seat);
    if (registry != nullptr)
      wl_registry_destroy(registry);
    if (display != nullptr) {
      wl_display_flush(display);
      wl_display_disconnect(display);
    }
    if (wake_fd >= 0)
      close(wake_fd);
    delete pending.load(std::memory_order_relaxed);
  }

  static bool available() { return WaylandClipboard{}.connect(); }

  bool open() {
    if (!connect())
      return false;
    // ext-data-control-v1 is wlr-data-control-unstable-v1 renamed, with the
    // same messages, so a wlr manager is bound by its own name and then used
    // through the ext functions.
    const bool ext = ext_manager_name != 0;
    manager = static_cast<ext_data_control_manager_v1 *>(
        wl_registry_bind(registry, ext ? ext_manager_name : wlr_manager_name,
                         ext ? &ext_data_control_manager_v1_interface
                             : &zwlr_data_control_manager_v1_interface,
                         1));
    if (manager == nullptr)
      return false;
    device = ext_data_control_manager_v1_get_data_device(manager, seat);
    if (device == nullptr)
      return false;
    ext_data_control_device_v1_add_listener(device, &device_listener, this);
    if (wl_display_roundtrip(display) < 0 || device == nullptr)
      return false;
    wake_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (wake_fd < 0)
      return false;
    serving.store(true, std::memory_order_relaxed);
    thread = std::jthread([this](const std::stop_token &stop) {
      block_signals();
      serve(stop);
      serving.store(false, std::memory_order_relaxed);
    });
    clipboard_log().debug("serving the clipboard through {}",
                          ext ? "ext-data-control-v1"
                              : "wlr-data-control-unstable-v1");
    return true;
  }

  BackendResult<> set(std::string_view text) {
    if (!serving.load(std::memory_order_relaxed))
      return std::unexpected(BackendError::BackendNotAvailable);
    auto next = std::make_unique<std::string>(text);
    const std::unique_ptr<std::string> replaced{
        pending.exchange(next.release(), std::memory_order_acq_rel)};
    wake();
    return {};
  }
};

class X11Clipboard {
private:
  enum Atom : std::uint8_t {
    Clipboard,
    Targets,
    Utf8String,
    Text,
    MimeUtf8,
    Mime,
    Incr,
    Manager,
    SaveTargets,
    SaveProperty,
    AtomCount,
  };
  static constexpr std::array<std::string_view, AtomCount> atom_names{
      "CLIPBOARD",
      "TARGETS",
      "UTF8_STRING",
      "TEXT",
      "text/plain;charset=utf-8",
      "text/plain",
      "INCR",
      "CLIPBOARD_MANAGER",
      "SAVE_TARGETS",
      "PRISM_CLIPBOARD_SAVE",
  };
  static constexpr auto transfer_lifetime = std::chrono::seconds(10);
  static constexpr auto save_timeout = std::chrono::seconds(1);
  static constexpr std::size_t chunk_limit = std::size_t{256} * 1024;

  struct Transfer {
    std::string text;
    xcb_atom_t type = XCB_NONE;
    std::size_t offset = 0;
    std::chrono::steady_clock::time_point touched;
  };

  struct FreeEvent {
    void operator()(xcb_generic_event_t *event) const noexcept {
      std::free(event);
    }
  };
  using Event = std::unique_ptr<xcb_generic_event_t, FreeEvent>;

  const Xcb *api = xcb();
  xcb_connection_t *connection = nullptr;
  xcb_window_t window = XCB_NONE;
  std::array<xcb_atom_t, AtomCount> atoms{};
  std::size_t chunk = 0;
  std::string text;
  std::map<std::pair<xcb_window_t, xcb_atom_t>, Transfer> transfers;
  int wake_fd = -1;
  std::atomic<std::string *> pending{nullptr};
  std::atomic<bool> serving{false};
  std::jthread thread;

  template <class Specific>
  static Specific event_as(const xcb_generic_event_t &event) {
    static_assert(sizeof(Specific) <= sizeof(xcb_generic_event_t));
    Specific specific{};
    std::memcpy(&specific, &event, sizeof(specific));
    return specific;
  }

  template <class Specific>
  void send_event(xcb_window_t destination, const Specific &event) {
    std::array<char, 32> bytes{};
    static_assert(sizeof(Specific) <= bytes.size());
    std::memcpy(bytes.data(), &event, sizeof(event));
    api->xcb_send_event(connection, 0, destination, XCB_EVENT_MASK_NO_EVENT,
                        bytes.data());
  }

  [[nodiscard]] xcb_window_t selection_owner(xcb_atom_t selection) {
    xcb_get_selection_owner_reply_t *reply = api->xcb_get_selection_owner_reply(
        connection, api->xcb_get_selection_owner(connection, selection),
        nullptr);
    const xcb_window_t owner = reply != nullptr ? reply->owner : XCB_NONE;
    std::free(reply);
    return owner;
  }

  void send_text(xcb_window_t requestor, xcb_atom_t property, xcb_atom_t type) {
    if (text.size() <= chunk) {
      api->xcb_change_property(
          connection, XCB_PROP_MODE_REPLACE, requestor, property, type, 8,
          static_cast<std::uint32_t>(text.size()), text.data());
      return;
    }
    const std::uint32_t events = XCB_EVENT_MASK_PROPERTY_CHANGE;
    api->xcb_change_window_attributes(connection, requestor, XCB_CW_EVENT_MASK,
                                      &events);
    const auto size = static_cast<std::uint32_t>(text.size());
    api->xcb_change_property(connection, XCB_PROP_MODE_REPLACE, requestor,
                             property, atoms[Incr], 32, 1, &size);
    transfers.insert_or_assign(std::pair{requestor, property},
                               Transfer{
                                   .text = text,
                                   .type = type,
                                   .offset = 0,
                                   .touched = std::chrono::steady_clock::now(),
                               });
  }

  void answer(const xcb_selection_request_event_t &request) {
    const xcb_atom_t property =
        request.property == XCB_NONE ? request.target : request.property;
    const xcb_atom_t target = request.target;
    bool answered = false;
    if (request.selection == atoms[Clipboard]) {
      if (target == atoms[Targets]) {
        const std::array<xcb_atom_t, 5> offered{
            atoms[Targets],  atoms[Utf8String], atoms[Text],
            atoms[MimeUtf8], atoms[Mime],
        };
        api->xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
                                 request.requestor, property, XCB_ATOM_ATOM, 32,
                                 static_cast<std::uint32_t>(offered.size()),
                                 offered.data());
        answered = true;
      } else if (target == atoms[Utf8String] || target == atoms[Text] ||
                 target == atoms[MimeUtf8] || target == atoms[Mime]) {
        send_text(request.requestor, property,
                  target == atoms[Text] ? atoms[Utf8String] : target);
        answered = true;
      }
    }
    xcb_selection_notify_event_t notify{};
    notify.response_type = XCB_SELECTION_NOTIFY;
    notify.time = request.time;
    notify.requestor = request.requestor;
    notify.selection = request.selection;
    notify.target = target;
    notify.property = answered ? property : XCB_NONE;
    send_event(request.requestor, notify);
  }

  void continue_transfer(const xcb_property_notify_event_t &notice) {
    if (notice.state != XCB_PROPERTY_DELETE)
      return;
    const auto found = transfers.find({notice.window, notice.atom});
    if (found == transfers.end())
      return;
    Transfer &transfer = found->second;
    const std::size_t length =
        std::min(chunk, transfer.text.size() - transfer.offset);
    api->xcb_change_property(connection, XCB_PROP_MODE_REPLACE, notice.window,
                             notice.atom, transfer.type, 8,
                             static_cast<std::uint32_t>(length),
                             transfer.text.data() + transfer.offset);
    transfer.offset += length;
    transfer.touched = std::chrono::steady_clock::now();
    if (length == 0)
      transfers.erase(found);
  }

  // Returns false once a clipboard manager has taken the text.
  bool handle(const xcb_generic_event_t &event) {
    switch (event.response_type & 0x7F) {
    case XCB_SELECTION_REQUEST:
      answer(event_as<xcb_selection_request_event_t>(event));
      break;
    case XCB_PROPERTY_NOTIFY:
      continue_transfer(event_as<xcb_property_notify_event_t>(event));
      break;
    case XCB_SELECTION_NOTIFY:
      return event_as<xcb_selection_notify_event_t>(event).selection !=
             atoms[Manager];
    default:
      break;
    }
    return true;
  }

  void wake() const {
    if (wake_fd >= 0)
      eventfd_write(wake_fd, 1);
  }

  // Replies are awaited only before the queue is drained, so that no event
  // waits in it while the thread sleeps in poll.
  void serve(const std::stop_token &stop) {
    using Clock = std::chrono::steady_clock;
    const int display_fd = api->xcb_get_file_descriptor(connection);
    std::optional<Clock::time_point> deadline;
    for (;;) {
      if (const std::unique_ptr<std::string> next{
              pending.exchange(nullptr, std::memory_order_acquire)}) {
        text = std::move(*next);
        api->xcb_set_selection_owner(connection, window, atoms[Clipboard],
                                     XCB_CURRENT_TIME);
      }
      if (!deadline && stop.stop_requested()) {
        if (!hand_off_to_manager())
          return;
        deadline = Clock::now() + save_timeout;
      }
      while (const Event event{api->xcb_poll_for_event(connection)})
        if (!handle(*event))
          return;
      const auto now = Clock::now();
      std::erase_if(transfers, [now](const auto &entry) {
        return now - entry.second.touched > transfer_lifetime;
      });
      if (api->xcb_flush(connection) <= 0)
        return;
      int timeout = -1;
      if (deadline) {
        timeout = static_cast<int>(
            std::chrono::ceil<std::chrono::milliseconds>(*deadline - now)
                .count());
        if (timeout <= 0)
          return;
      }
      std::array<pollfd, 2> ready{
          {
              {.fd = display_fd, .events = POLLIN, .revents = 0},
              {.fd = wake_fd, .events = POLLIN, .revents = 0},
          },
      };
      if (poll(ready.data(), ready.size(), timeout) < 0 && errno != EINTR)
        return;
      if (has_event(ready[1], POLLIN)) {
        eventfd_t count = 0;
        eventfd_read(wake_fd, &count);
      }
    }
  }

  // Asks a clipboard manager to keep the text. Returns false if there is no
  // manager or nothing of Prism's to keep.
  bool hand_off_to_manager() {
    if (selection_owner(atoms[Clipboard]) != window ||
        selection_owner(atoms[Manager]) == XCB_NONE)
      return false;
    const std::array<xcb_atom_t, 4> targets{
        atoms[Utf8String],
        atoms[Text],
        atoms[MimeUtf8],
        atoms[Mime],
    };
    api->xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window,
                             atoms[SaveProperty], XCB_ATOM_ATOM, 32,
                             static_cast<std::uint32_t>(targets.size()),
                             targets.data());
    api->xcb_convert_selection(connection, window, atoms[Manager],
                               atoms[SaveTargets], atoms[SaveProperty],
                               XCB_CURRENT_TIME);
    return true;
  }

  bool connect() {
    if (!environment_has("DISPLAY") || api == nullptr)
      return false;
    int screen_number = 0;
    connection = api->xcb_connect(nullptr, &screen_number);
    if (api->xcb_connection_has_error(connection) != 0)
      return false;
    auto screens =
        api->xcb_setup_roots_iterator(api->xcb_get_setup(connection));
    for (int index = 0; index < screen_number && screens.rem > 0; ++index)
      api->xcb_screen_next(&screens);
    if (screens.rem <= 0 || screens.data == nullptr)
      return false;
    window = api->xcb_generate_id(connection);
    api->xcb_create_window(
        connection, XCB_COPY_FROM_PARENT, window, screens.data->root, 0, 0, 1,
        1, 0, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, 0, nullptr);
    std::array<xcb_intern_atom_cookie_t, AtomCount> cookies{};
    for (std::size_t index = 0; index < AtomCount; ++index)
      cookies[index] = api->xcb_intern_atom(
          connection, 0, static_cast<std::uint16_t>(atom_names[index].size()),
          atom_names[index].data());
    for (std::size_t index = 0; index < AtomCount; ++index) {
      xcb_intern_atom_reply_t *reply =
          api->xcb_intern_atom_reply(connection, cookies[index], nullptr);
      if (reply != nullptr)
        atoms[index] = reply->atom;
      std::free(reply);
    }
#ifdef __cpp_lib_ranges_contains
    if (std::ranges::contains(atoms, XCB_NONE))
#else
    if (std::ranges::find(atoms, XCB_NONE) != atoms.end())
#endif
      return false;
    // The request length is in four-byte units, less a property change header.
    chunk = std::min(
        chunk_limit,
        (std::size_t{api->xcb_get_maximum_request_length(connection)} * 4) -
            64);
    return true;
  }

public:
  X11Clipboard() = default;
  X11Clipboard(const X11Clipboard &) = delete;
  X11Clipboard &operator=(const X11Clipboard &) = delete;
  X11Clipboard(X11Clipboard &&) = delete;
  X11Clipboard &operator=(X11Clipboard &&) = delete;

  ~X11Clipboard() {
    if (thread.joinable()) {
      thread.request_stop();
      wake();
      thread.join();
    }
    if (connection != nullptr) {
      if (window != XCB_NONE &&
          api->xcb_connection_has_error(connection) == 0) {
        api->xcb_destroy_window(connection, window);
        api->xcb_flush(connection);
      }
      api->xcb_disconnect(connection);
    }
    if (wake_fd >= 0)
      close(wake_fd);
    delete pending.load(std::memory_order_relaxed);
  }

  static bool available() { return X11Clipboard{}.connect(); }

  bool open() {
    if (!connect())
      return false;
    wake_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (wake_fd < 0)
      return false;
    serving.store(true, std::memory_order_relaxed);
    thread = std::jthread([this](const std::stop_token &stop) {
      block_signals();
      serve(stop);
      serving.store(false, std::memory_order_relaxed);
    });
    clipboard_log().debug("serving the clipboard through X11");
    return true;
  }

  BackendResult<> set(std::string_view value) {
    if (!serving.load(std::memory_order_relaxed))
      return std::unexpected(BackendError::BackendNotAvailable);
    auto next = std::make_unique<std::string>(value);
    const std::unique_ptr<std::string> replaced{
        pending.exchange(next.release(), std::memory_order_acq_rel)};
    wake();
    return {};
  }
};
} // namespace

class ClipboardBackend final : public TextToSpeechBackend {
private:
  std::variant<std::monostate, WaylandClipboard, X11Clipboard> clipboard;

public:
  static constexpr bool requires_explicit_selection = true;

  [[nodiscard]] std::string_view get_name() const override {
    return "Clipboard";
  }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features =
        SUPPORTS_SPEAK | SUPPORTS_OUTPUT | REQUIRES_EXPLICIT_SELECTION;
    if (!std::holds_alternative<std::monostate>(clipboard) ||
        WaylandClipboard::available() || X11Clipboard::available())
      features |= IS_SUPPORTED_AT_RUNTIME;
    return features;
  }

  BackendResult<> initialize() override {
    if (!std::holds_alternative<std::monostate>(clipboard))
      return std::unexpected(BackendError::AlreadyInitialized);
    if (clipboard.emplace<WaylandClipboard>().open())
      return {};
    if (clipboard.emplace<X11Clipboard>().open())
      return {};
    clipboard.emplace<std::monostate>();
    return std::unexpected(BackendError::BackendNotAvailable);
  }

  BackendResult<> speak(std::string_view text,
                        [[maybe_unused]] bool interrupt) override {
    if (auto *const wayland = std::get_if<WaylandClipboard>(&clipboard))
      return wayland->set(text);
    if (auto *const x11 = std::get_if<X11Clipboard>(&clipboard))
      return x11->set(text);
    return std::unexpected(BackendError::NotInitialized);
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }
};

REGISTER_BACKEND_WITH_ID(ClipboardBackend, Backends::Clipboard, "Clipboard", 0);
#endif
