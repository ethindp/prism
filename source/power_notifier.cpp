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
  return std::make_unique<WindowsPowerNotifier>(on_suspend, on_resume);
}

bool PowerNotifier::supported() noexcept { return true; }

#elif defined(PRISM_ENABLE_POWER_MANAGEMENT) && defined(__linux__) &&          \
    !defined(__ANDROID__)
#include "dbus/login1.h"
#include "logging.h"
#include <atomic>
#include <thread>

namespace {
class LinuxPowerNotifier final : public PowerNotifier {
  const LogSource log{"Power notifier"};
  std::function<void()> on_suspend;
  std::function<void()> on_resume;
  prism::dbus::Connection connection;
  std::atomic_flag stopping = ATOMIC_FLAG_INIT;
  std::thread worker;

  void run() noexcept {
    while (!stopping.test(std::memory_order_acquire)) {
      auto message = connection.receive(100);
      if (!message) {
        if (!connection.connected()) {
          log.warn(
              "System D-Bus connection lost; power notifications disabled");
          break;
        }
        continue;
      }
      const auto sleeping =
          prism::dbus::services::login1::prepare_for_sleep(message);
      if (!sleeping)
        continue;
      try {
        if (*sleeping) {
          if (on_suspend)
            on_suspend();
        } else {
          if (on_resume)
            on_resume();
        }
      } catch (...) {
        log.warn("Suspend/resume callback threw an exception");
      }
    }
  }

public:
  LinuxPowerNotifier(std::function<void()> suspend,
                     std::function<void()> resume, prism::dbus::Connection conn)
      : on_suspend(std::move(suspend)), on_resume(std::move(resume)),
        connection(std::move(conn)), worker(&LinuxPowerNotifier::run, this) {}

  ~LinuxPowerNotifier() override {
    stopping.test_and_set(std::memory_order_release);
    if (worker.joinable())
      worker.join();
  }
};
} // namespace

std::unique_ptr<PowerNotifier>
PowerNotifier::create(const std::function<void()> &on_suspend,
                      const std::function<void()> &on_resume) {
  auto bus = prism::dbus::Connection::open(DBUS_BUS_SYSTEM);
  if (!bus || !prism::dbus::services::login1::subscribe(bus)) {
    static const LogSource log{"Power notifier"};
    log.debug("System D-Bus unavailable or login1 subscription failed");
    return nullptr;
  }
  try {
    return std::make_unique<LinuxPowerNotifier>(on_suspend, on_resume,
                                                std::move(bus));
  } catch (...) {
    return nullptr;
  }
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
