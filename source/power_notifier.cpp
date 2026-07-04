// SPDX-License-Identifier: MPL-2.0

#include "power_notifier.h"
#include <utility>
#if defined(PRISM_ENABLE_POWER_MANAGEMENT) && defined(_WIN32)
#include <powerbase.h>
#include <windows.h>

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
  return std::make_unique<WindowsPowerNotifier>(std::move(on_suspend),
                                                std::move(on_resume));
}

bool PowerNotifier::supported() noexcept { return true; }

#elif defined(PRISM_ENABLE_POWER_MANAGEMENT) && defined(__linux__) &&          \
    !defined(__ANDROID__)
#include <giomm/dbusconnection.h>
#include <giomm/init.h>
#include <glibmm/main.h>
#include <glibmm/refptr.h>
#include <glibmm/variant.h>
#include <thread>

namespace {
class LinuxPowerNotifier final : public PowerNotifier {
private:
  std::function<void()> on_suspend;
  std::function<void()> on_resume;
  Glib::RefPtr<Glib::MainContext> context;
  Glib::RefPtr<Glib::MainLoop> loop;
  Glib::RefPtr<Gio::DBus::Connection> connection;
  guint sub_id = 0;
  std::thread thread;

  void thread_main() {
    context->push_thread_default();
    try {
      connection = Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SYSTEM);
      if (connection)
        sub_id = connection->signal_subscribe(
            sigc::mem_fun(*this, &LinuxPowerNotifier::on_signal),
            "org.freedesktop.login1", "org.freedesktop.login1.Manager",
            "PrepareForSleep", "/org/freedesktop/login1");
    } catch (const Glib::Error &) {
    }
    loop->run();
    if (connection && sub_id != 0)
      connection->signal_unsubscribe(sub_id_);
    context->pop_thread_default();
  }

  void on_signal(
      [[maybe_unused]] const Glib::RefPtr<Gio::DBus::Connection> &_connection,
      [[maybe_unused]] const Glib::ustring &sender_name,
      [[maybe_unused]] const Glib::ustring &object_path,
      [[maybe_unused]] const Glib::ustring &interface_name,
      [[maybe_unused]] const Glib::ustring &signal_name,
      const Glib::VariantContainerBase &params) {
    if (params.get_n_children() < 1)
      return;
    Glib::Variant<bool> arg;
    params.get_child(arg, 0);
    if (arg.get()) {
      if (on_suspend)
        on_suspend();
    } else {
      if (on_resume)
        on_resume();
    }
  }

public:
  LinuxPowerNotifier(std::function<void()> on_suspend,
                     std::function<void()> on_resume)
      : on_suspend(std::move(on_suspend)), on_resume(std::move(on_resume)),
        context(Glib::MainContext::create()),
        loop(Glib::MainLoop::create(context, false)) {
    thread = std::thread([this] { thread_main(); });
  }

  ~LinuxPowerNotifier() override {
    loop->quit();
    if (thread.joinable())
      thread.join();
  }
};
} // namespace

std::unique_ptr<PowerNotifier>
PowerNotifier::create(const std::function<void()> &on_suspend,
                      const std::function<void()> &on_resume) {
  Gio::init();
  return std::make_unique<LinuxPowerNotifier>(std::move(on_suspend),
                                              std::move(on_resume));
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
      std::lock_guard lock(mtx);
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
  return std::make_unique<MacPowerNotifier>(std::move(on_suspend),
                                            std::move(on_resume));
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
