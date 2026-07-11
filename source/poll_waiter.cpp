// SPDX-License-Identifier: MPL-2.0

#include "poll_waiter.h"
#include "logging.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#ifdef _WIN32
#include <windows.h>

namespace {
class WindowsWaiter final : public PollWaiter {
private:
  HANDLE timer = nullptr;
  HANDLE event = nullptr;
  LogSource logger{"Poll Waiter/win32"};

public:
  WindowsWaiter() {
    logger.info("Initializing");
    logger.debug("Creating timer with CreateWaitableTimerEx");
    timer = CreateWaitableTimer(nullptr, FALSE, nullptr);
    assert(timer != nullptr);
    if (timer == nullptr) {
      logger.error("Could not create waitable timer, code {:X}",
                   GetLastError());
      return;
    }
    event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(event != nullptr);
    if (event == nullptr) {
      logger.error("Could not create event, code {:X}", GetLastError());
      return;
    }
    logger.debug(
        "Timer created with handle {:X} and event created with handle {:X}",
        reinterpret_cast<std::uintptr_t>(timer),
        reinterpret_cast<std::uintptr_t>(event));
    logger.info("Initialization complete");
  }

  ~WindowsWaiter() override {
    logger.info("Shutting down");
    if (timer != nullptr)
      CloseHandle(timer);
    if (event != nullptr)
      CloseHandle(event);
    logger.info("Shutdown complete");
  }

  Wake wait(std::optional<std::chrono::milliseconds> timeout,
            std::chrono::milliseconds leeway) override {
    if (!timeout) {
      logger.debug("Timeout is nullopt; waiting infintely");
      WaitForSingleObject(event, INFINITE);
      logger.debug("Event triggered, waking up");
      return Wake::Signal;
    }
    LARGE_INTEGER due;
    due.QuadPart = -static_cast<LONGLONG>(timeout->count()) * 10000;
    const auto tolerable =
        static_cast<ULONG>(std::max<ULONG>(leeway.count(), 0));
    logger.debug("Setting timer to time out in {}ns with leeway of {}ns",
                 std::abs(due.QuadPart), tolerable);
    if (SetWaitableTimerEx(timer, &due, 0, nullptr, nullptr, nullptr,
                           tolerable) == 0) {
      logger.error("SetWaitableTimerEx failed, code {:X}; failing with wake "
                   "source as signal",
                   GetLastError());
      return Wake::Signal;
    }
    const auto handles = std::to_array<HANDLE>({timer, event});
    logger.debug("Entering wait state for handles {:X} and {:X}",
                 reinterpret_cast<std::uintptr_t>(handles[0]),
                 reinterpret_cast<std::uintptr_t>(handles[1]));
    const auto r =
        WaitForMultipleObjects(handles.size(), handles.data(), FALSE, INFINITE);
    logger.debug("Woke up with return code {:X}, cancelling timer", r);
    CancelWaitableTimer(timer);
    return (r == WAIT_OBJECT_0) ? Wake::Timer : Wake::Signal;
  }

  void wake() override {
    logger.debug("Immediate wake requested!");
    SetEvent(event);
  }
};
} // namespace

std::unique_ptr<PollWaiter> PollWaiter::create() {
  return std::make_unique<WindowsWaiter>();
}

#elifdef __linux__
#include <atomic>
#include <cerrno>
#include <ctime>
#include <limits>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/prctl.h>
#include <unistd.h>

namespace {
class LinuxWaiter final : public PollWaiter {
private:
  int efd = -1;
  unsigned long last_slack = 0;
  bool slack_set = false;
  std::atomic_flag woken;

  static timespec to_timespec(std::chrono::nanoseconds ns) {
    if (ns < std::chrono::nanoseconds{0})
      ns = std::chrono::nanoseconds{0};
    timespec ts;
    ts.tv_sec = static_cast<time_t>(ns.count() / 1000000000);
    ts.tv_nsec = static_cast<long>(ns.count() % 1000000000);
    return ts;
  }

  void apply_slack(std::chrono::milliseconds leeway) {
    const unsigned long slack_ns =
        leeway.count() > 0
            ? static_cast<unsigned long>(std::min<std::uint64_t>(
                  static_cast<std::uint64_t>(leeway.count()) * 1000000,
                  static_cast<std::uint64_t>(
                      std::numeric_limits<unsigned long>::max())))
            : 1;
    if (!slack_set || slack_ns != last_slack) {
      prctl(PR_SET_TIMERSLACK, slack_ns, 0, 0, 0);
      last_slack = slack_ns;
      slack_set = true;
    }
  }

  Wake fallback_wait(std::optional<std::chrono::milliseconds> timeout) {
    constexpr auto slice =
        std::chrono::nanoseconds{std::chrono::milliseconds{20}};
    const auto deadline =
        timeout ? std::optional{std::chrono::steady_clock::now() + *timeout}
                : std::nullopt;
    while (true) {
      if (woken.test()) {
        woken.clear();
        return Wake::Signal;
      }
      auto nap = slice;
      if (deadline) {
        const auto rem = std::chrono::duration_cast<std::chrono::nanoseconds>(
            *deadline - std::chrono::steady_clock::now());
        if (rem <= std::chrono::nanoseconds{0})
          return Wake::Timer;
        nap = std::min(nap, rem);
      }
      const timespec ts = to_timespec(nap);
      nanosleep(&ts, nullptr);
    }
  }

public:
  LinuxWaiter() { efd = eventfd(0, EFD_CLOEXEC); }

  ~LinuxWaiter() override {
    if (efd >= 0)
      close(efd);
  }

  Wake wait(std::optional<std::chrono::milliseconds> timeout,
            std::chrono::milliseconds leeway) override {
    if (efd < 0)
      return fallback_wait(timeout);
    apply_slack(leeway);
    const auto deadline =
        timeout ? std::optional{std::chrono::steady_clock::now() + *timeout}
                : std::nullopt;
    for (;;) {
      timespec ts;
      timespec *pts = nullptr;
      if (deadline) {
        const auto rem = std::chrono::duration_cast<std::chrono::nanoseconds>(
            *deadline - std::chrono::steady_clock::now());
        if (rem <= std::chrono::nanoseconds{0})
          return Wake::Timer;
        ts = to_timespec(rem);
        pts = &ts;
      }
      pollfd pfd{};
      pfd.fd = efd;
      pfd.events = POLLIN;
      const int r = ppoll(&pfd, 1, pts, nullptr);
      if (r < 0) {
        if (errno == EINTR)
          continue;
        auto nap = std::chrono::nanoseconds{std::chrono::milliseconds{20}};
        if (deadline) {
          const auto rem = std::chrono::duration_cast<std::chrono::nanoseconds>(
              *deadline - std::chrono::steady_clock::now());
          if (rem <= std::chrono::nanoseconds{0})
            return Wake::Timer;
          nap = std::min(nap, rem);
        }
        const timespec nts = to_timespec(nap);
        nanosleep(&nts, nullptr);
        continue;
      }
      if (r == 0)
        return Wake::Timer;
      if ((pfd.revents & POLLIN) != 0) {
        std::uint64_t v = 0;
        [[maybe_unused]] ssize_t n = read(efd, &v, sizeof(v));
        return Wake::Signal;
      }
      return Wake::Timer;
    }
  }

  void wake() override {
    if (efd >= 0) {
      std::uint64_t one = 1;
      [[maybe_unused]] ssize_t n = write(efd, &one, sizeof(one));
    } else {
      woken.test_and_set();
    }
  }
};
} // namespace

std::unique_ptr<PollWaiter> PollWaiter::create() {
  return std::make_unique<LinuxWaiter>();
}

#elifdef __APPLE__
#include <atomic>
#include <cerrno>
#include <ctime>
#include <sys/event.h>
#include <sys/types.h>
#include <unistd.h>

namespace {
constexpr std::uintptr_t timer_id = 1;
constexpr std::uintptr_t user_id = 2;

class MacWaiter final : public PollWaiter {
private:
  int kq = -1;
  std::atomic_flag woken;

  static timespec to_timespec(std::chrono::nanoseconds ns) {
    if (ns < std::chrono::nanoseconds{0})
      ns = std::chrono::nanoseconds{0};
    timespec ts;
    ts.tv_sec = static_cast<time_t>(ns.count() / 1000000000);
    ts.tv_nsec = static_cast<long>(ns.count() % 1000000000);
    return ts;
  }

  Wake fallback_wait(std::optional<std::chrono::milliseconds> timeout) {
    constexpr auto slice =
        std::chrono::nanoseconds{std::chrono::milliseconds{20}};
    const auto deadline =
        timeout ? std::optional{std::chrono::steady_clock::now() + *timeout}
                : std::nullopt;
    while (true) {
      if (woken.test()) {
        woken.clear();
        return Wake::Signal;
      }
      auto nap = slice;
      if (deadline) {
        const auto rem = std::chrono::duration_cast<std::chrono::nanoseconds>(
            *deadline - std::chrono::steady_clock::now());
        if (rem <= std::chrono::nanoseconds{0})
          return Wake::Timer;
        nap = std::min(nap, rem);
      }
      const timespec ts = to_timespec(nap);
      nanosleep(&ts, nullptr);
    }
  }

public:
  MacWaiter() {
    kq = kqueue();
    if (kq >= 0) {
      struct kevent ev{};
      EV_SET(&ev, user_id, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, nullptr);
      kevent(kq, &ev, 1, nullptr, 0, nullptr);
    }
  }

  ~MacWaiter() override {
    if (kq >= 0)
      close(kq);
  }

  Wake wait(std::optional<std::chrono::milliseconds> timeout,
            std::chrono::milliseconds leeway) override {
    if (kq < 0)
      return fallback_wait(timeout);
    if (timeout) {
      struct kevent64_s tev{};
      EV_SET64(
          &tev, timer_id, EVFILT_TIMER, EV_ADD | EV_ONESHOT, NOTE_LEEWAY,
          static_cast<std::int64_t>(timeout->count()), 0, 0,
          static_cast<std::uint64_t>(
              std::max<std::chrono::milliseconds::rep>(leeway.count(), 0)));
      kevent64(kq, &tev, 1, nullptr, 0, 0, nullptr);
    }
    struct kevent out{};
    int r;
    do {
      r = kevent(kq, nullptr, 0, &out, 1, nullptr);
    } while (r < 0 && errno == EINTR);
    struct kevent del{};
    EV_SET(&del, timer_id, EVFILT_TIMER, EV_DELETE, 0, 0, nullptr);
    kevent(kq, &del, 1, nullptr, 0, nullptr);
    if (r > 0 && out.filter == EVFILT_TIMER)
      return Wake::Timer;
    return Wake::Signal;
  }

  void wake() override {
    if (kq < 0) {
      woken.test_and_set();
      return;
    }
    struct kevent ev{};
    EV_SET(&ev, user_id, EVFILT_USER, 0, NOTE_TRIGGER, 0, nullptr);
    kevent(kq, &ev, 1, nullptr, 0, nullptr);
  }
};
} // namespace

std::unique_ptr<PollWaiter> PollWaiter::create() {
  return std::make_unique<MacWaiter>();
}

#else
#include <condition_variable>
#include <mutex>

namespace {
class FallbackWaiter final : public PollWaiter {
private:
  std::mutex mutex;
  std::condition_variable cv;
  bool signaled = false;

public:
  Wake wait(std::optional<std::chrono::milliseconds> timeout,
            [[maybe_unused]] std::chrono::milliseconds) override {
    std::unique_lock lock(mutex);
    if (timeout) {
      cv.wait_for(lock, *timeout, [this] { return signaled; });
      if (signaled) {
        signaled = false;
        return Wake::Signal;
      }
      return Wake::Timer;
    }
    cv.wait(lock, [this] { return signaled; });
    signaled = false;
    return Wake::Signal;
  }

  void wake() override {
    {
      std::scoped_lock lock(mutex);
      signaled = true;
    }
    cv.notify_one();
  }
};
} // namespace

std::unique_ptr<PollWaiter> PollWaiter::create() {
  return std::make_unique<FallbackWaiter>();
}

#endif
