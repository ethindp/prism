// SPDX-License-Identifier: MPL-2.0

#include "power_notifier.h"
#include <utility>
#if defined(PRISM_ENABLE_POWER_MANAGEMENT) && defined(_WIN32)
#include <windows.h>
#include <powerbase.h>
#include <powrprof.h>

namespace {
class WindowsPowerNotifier final : public PowerNotifier {
private:
  std::function<void()> on_suspend;
  std::function<void()> on_resume;
  DEVICE_NOTIFY_SUBSCRIBE_PARAMETERS params{};
  HPOWERNOTIFY handle = nullptr;

  static ULONG CALLBACK callback(PVOID context, ULONG type,
                                 [[maybe_unused]] PVOID Setting) {
    auto *self = static_cast<WindowsPowerNotifier *>(context);
    switch (type) {
    case PBT_APMSUSPEND:
      if (self->on_suspend)
        self->on_suspend();
      break;
    case PBT_APMRESUMESUSPEND:
    case PBT_APMRESUMEAUTOMATIC:
      if (self->on_resume)
        self->on_resume();
      break;
    default:
      break;
    }
    return ERROR_SUCCESS;
  }

public:
  WindowsPowerNotifier(std::function<void()> on_suspend,
                       std::function<void()> on_resume)
      : on_suspend(std::move(on_suspend)), on_resume(std::move(on_resume)) {
    params.Callback = &WindowsPowerNotifier::callback;
    params.Context = this;
    // We deliberately ignore the return value of this function: if it fails,
    // this entire class just does nothing.
    PowerRegisterSuspendResumeNotification(DEVICE_NOTIFY_CALLBACK, &params,
                                           &handle);
  }

  ~WindowsPowerNotifier() override {
    if (handle != nullptr)
      PowerUnregisterSuspendResumeNotification(handle);
  }
};
} // namespace

std::unique_ptr<PowerNotifier>
PowerNotifier::create(const std::function<void()> &on_suspend,
                      const std::function<void()> &on_resume) {
  return std::make_unique<WindowsPowerNotifier>(on_suspend,
                                                on_resume);
}

bool PowerNotifier::supported() noexcept { return true; }

#elif defined(PRISM_ENABLE_POWER_MANAGEMENT) && defined(__linux__) &&          \
    !defined(__ANDROID__)
#include <gio/gio.h>
#include <thread>

namespace {
class LinuxPowerNotifier final : public PowerNotifier {
private:
  std::function<void()> on_suspend;
  std::function<void()> on_resume;
  GMainContext *context;
  GMainLoop *loop;
  GDBusConnection *connection;
  std::thread thread;

  void thread_main() {
    g_main_context_push_thread_default(context);
    const guint sub_id = g_dbus_connection_signal_subscribe(
        connection, "org.freedesktop.login1", "org.freedesktop.login1.Manager",
        "PrepareForSleep", "/org/freedesktop/login1", nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE, &LinuxPowerNotifier::on_signal, this,
        nullptr);
    g_main_loop_run(loop);
    g_dbus_connection_signal_unsubscribe(connection, sub_id);
    g_main_context_pop_thread_default(context);
  }

  static void on_signal([[maybe_unused]] GDBusConnection *connection,
                        [[maybe_unused]] const gchar *sender_name,
                        [[maybe_unused]] const gchar *object_path,
                        [[maybe_unused]] const gchar *interface_name,
                        [[maybe_unused]] const gchar *signal_name,
                        GVariant *params, gpointer user_data) {
    if (!g_variant_is_of_type(params, G_VARIANT_TYPE("(b)")))
      return;
    gboolean suspending = FALSE;
    g_variant_get(params, "(b)", &suspending);
    auto *const self = static_cast<LinuxPowerNotifier *>(user_data);
    // NOLINTBEGIN(bugprone-empty-catch)
    try {
      if (suspending) {
        if (self->on_suspend)
          self->on_suspend();
      } else {
        if (self->on_resume)
          self->on_resume();
      }
    } catch (...) {
    }
    // NOLINTEND(bugprone-empty-catch)
  }

  static gboolean quit_loop(gpointer data) {
    g_main_loop_quit(static_cast<GMainLoop *>(data));
    return G_SOURCE_REMOVE;
  }

public:
  LinuxPowerNotifier(std::function<void()> on_suspend,
                     std::function<void()> on_resume, GDBusConnection *conn)
      : on_suspend(std::move(on_suspend)), on_resume(std::move(on_resume)),
        context(g_main_context_new()), loop(g_main_loop_new(context, FALSE)),
        connection(conn) {
    thread = std::thread([this] { thread_main(); });
  }

  ~LinuxPowerNotifier() override {
    g_main_context_invoke(context, &LinuxPowerNotifier::quit_loop, loop);
    if (thread.joinable())
      thread.join();
    g_main_loop_unref(loop);
    g_main_context_unref(context);
    g_object_unref(connection);
  }
};
} // namespace

std::unique_ptr<PowerNotifier>
PowerNotifier::create(const std::function<void()> &on_suspend,
                      const std::function<void()> &on_resume) {
  GError *error = nullptr;
  GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
  if (error != nullptr)
    g_error_free(error);
  if (conn == nullptr)
    return nullptr;
  return std::make_unique<LinuxPowerNotifier>(on_suspend, on_resume, conn);
}

bool PowerNotifier::supported() noexcept { return true; }

#elif defined(PRISM_ENABLE_POWER_MANAGEMENT) && defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_OSX
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace {
class MacPowerNotifier final : public PowerNotifier {
private:
  std::function<void()> on_suspend;
  std::function<void()> on_resume;
  io_connect_t root_port = MACH_PORT_NULL;
  CFRunLoopRef runloop = nullptr;
  CFRunLoopSourceRef stop_source = nullptr;
  std::mutex mtx;
  std::condition_variable ready_cv;
  bool ready = false;
  std::thread thread;

  static void stop_perform(void *info) {
    auto *self = static_cast<MacPowerNotifier *>(info);
    if (self->runloop != nullptr)
      CFRunLoopStop(self->runloop);
  }

  static void power_callback(void *ctx, [[maybe_unused]] io_service_t service,
                             natural_t type, void *arg) {
    auto *self = static_cast<MacPowerNotifier *>(ctx);
    switch (type) {
    case kIOMessageCanSystemSleep:
      IOAllowPowerChange(self->root_port, reinterpret_cast<intptr_t>(arg));
      break;
    case kIOMessageSystemWillSleep:
      if (self->on_suspend)
        self->on_suspend();
      IOAllowPowerChange(self->root_port, reinterpret_cast<intptr_t>(arg));
      break;
    case kIOMessageSystemHasPoweredOn:
      if (self->on_resume)
        self->on_resume();
      break;
    default:
      break;
    }
  }

  void thread_main() {
    IONotificationPortRef notify_port = nullptr;
    io_object_t notifier = 0;
    CFRunLoopSourceRef power_source = nullptr;
    root_port = IORegisterForSystemPower(this, &notify_port, &power_callback,
                                         &notifier);
    if (root_port != MACH_PORT_NULL) {
      runloop = CFRunLoopGetCurrent();
      power_source = IONotificationPortGetRunLoopSource(notify_port);
      CFRunLoopAddSource(runloop, power_source, kCFRunLoopCommonModes);
      CFRunLoopSourceContext ctx{};
      ctx.info = this;
      ctx.perform = &MacPowerNotifier::stop_perform;
      stop_source = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &ctx);
      CFRunLoopAddSource(runloop, stop_source, kCFRunLoopCommonModes);
    }
    {
      std::scoped_lock lock(mtx);
      ready = true;
    }
    ready_cv.notify_one();
    if (root_port == MACH_PORT_NULL)
      return;
    CFRunLoopRun();
    if (stop_source != nullptr) {
      CFRunLoopRemoveSource(runloop, stop_source, kCFRunLoopCommonModes);
      CFRelease(stop_source);
      stop_source = nullptr;
    }
    if (power_source != nullptr)
      CFRunLoopRemoveSource(runloop, power_source, kCFRunLoopCommonModes);
    IODeregisterForSystemPower(&notifier);
    IOServiceClose(root_port);
    IONotificationPortDestroy(notify_port);
    root_port = MACH_PORT_NULL;
    runloop = nullptr;
  }

public:
  MacPowerNotifier(std::function<void()> on_suspend,
                   std::function<void()> on_resume)
      : on_suspend(std::move(on_suspend)), on_resume(std::move(on_resume)) {
    thread = std::thread([this] { thread_main(); });
    std::unique_lock lock(mtx);
    ready_cv.wait(lock, [this] { return ready; });
  }

  ~MacPowerNotifier() override {
    if (runloop != nullptr && stop_source != nullptr) {
      CFRunLoopSourceSignal(stop_source);
      CFRunLoopWakeUp(runloop);
    }
    if (thread.joinable())
      thread.join();
  }
};
} // namespace

std::unique_ptr<PowerNotifier>
PowerNotifier::create(const std::function<void()> &on_suspend,
                      const std::function<void()> &on_resume) {
  return std::make_unique<MacPowerNotifier>(on_suspend, on_resume);
}

bool PowerNotifier::supported() noexcept { return true; }

#else

std::unique_ptr<PowerNotifier>
PowerNotifier::create(const std::function<void()> &,
                      const std::function<void()> &) {
  return nullptr;
}

bool PowerNotifier::supported() noexcept { return false; }

#endif

#else

std::unique_ptr<PowerNotifier>
PowerNotifier::create([[maybe_unused]] const std::function<void()> &on_suspend,
                      [[maybe_unused]] const std::function<void()> &on_resume) {
  return nullptr;
}

bool PowerNotifier::supported() noexcept { return false; }

#endif
