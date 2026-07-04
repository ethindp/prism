// SPDX-License-Identifier: MPL-2.0

#include "poll_waiter.h"
#include <algorithm>
#include <array>
#include <cstdint>
#ifdef _WIN32
#include <windows.h>

namespace {
class WindowsWaiter final : public PollWaiter {
private:
  HANDLE timer = nullptr;
  HANDLE event = nullptr;

public:
  WindowsWaiter() {
    timer = CreateWaitableTimerEx(nullptr, nullptr,
                                  CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                  TIMER_ALL_ACCESS);
    if (timer == nullptr)
      timer = CreateWaitableTimer(nullptr, FALSE, nullptr);
    event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  }

  ~WindowsWaiter() override {
    if (timer != nullptr)
      CloseHandle(timer);
    if (event != nullptr)
      CloseHandle(event);
  }

  Wake wait(std::optional<std::chrono::milliseconds> timeout,
            std::chrono::milliseconds leeway) override {
    if (!timeout) {
      WaitForSingleObject(event, INFINITE);
      return Wake::Signal;
    }
    LARGE_INTEGER due;
    due.QuadPart = -static_cast<LONGLONG>(timeout->count()) * 10000;
    const auto tolerable =
        static_cast<ULONG>(std::max<ULONG>(leeway.count(), 0));
    SetWaitableTimerEx(timer, &due, 0, nullptr, nullptr, nullptr, tolerable);
    const auto handles = std::to_array<HANDLE>({timer, event});
    const auto r =
        WaitForMultipleObjects(handles.size(), handles.data(), FALSE, INFINITE);
    CancelWaitableTimer(timer);
    return (r == WAIT_OBJECT_0) ? Wake::Timer : Wake::Signal;
  }

  void wake() override { SetEvent(event); }
};
} // namespace

std::unique_ptr<PollWaiter> PollWaiter::create() {
  return std::make_unique<WindowsWaiter>();
}

#elifdef __linux__
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/prctl.h>
#include <unistd.h>

namespace {
class LinuxWaiter final : public PollWaiter {
private:
  int efd = -1;

public:
  LinuxWaiter() { efd = eventfd(0, EFD_CLOEXEC); }

  ~LinuxWaiter() override {
    if (efd >= 0)
      close(efd);
  }

  Wake wait(std::optional<std::chrono::milliseconds> timeout,
            std::chrono::milliseconds leeway) override {
    if (efd < 0)
      return Wake::Timer;
    const unsigned long slack_ns =
        leeway.count() > 0
            ? static_cast<unsigned long>(leeway.count()) * 1000000
            : 1;
    prctl(PR_SET_TIMERSLACK, slack_ns, 0, 0, 0);
    pollfd pfd{};
    pfd.fd = efd;
    pfd.events = POLLIN;
    timespec ts;
    timespec *pts = nullptr;
    if (timeout) {
      ts.tv_sec = static_cast<time_t>(timeout->count() / 1000);
      ts.tv_nsec = static_cast<long>((timeout->count() % 1000) * 1000000);
      pts = &ts;
    }
    const int r = ppoll(&pfd, 1, pts, nullptr);
    if (r < 0)
      return Wake::Signal;
    if (r > 0 && (pfd.revents & POLLIN) != 0) {
      std::uint64_t v = 0;
      [[maybe_unused]] ssize_t n = read(efd, &v, sizeof(v));
      return Wake::Signal;
    }
    return Wake::Timer;
  }

  void wake() override {
    if (efd < 0)
      return;
    std::uint64_t one = 1;
    [[maybe_unused]] ssize_t n = write(efd, &one, sizeof(one));
  }
};
} // namespace

std::unique_ptr<PollWaiter> PollWaiter::create() {
  return std::make_unique<LinuxWaiter>();
}

#elifdef __APPLE__
#include <sys/event.h>
#include <sys/types.h>
#include <unistd.h>

namespace {
constexpr std::uintptr_t timer_id = 1;
constexpr std::uintptr_t user_id = 2;

class MacWaiter final : public PollWaiter {
private:
  int kq = -1;

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
      return Wake::Timer;
    if (timeout) {
      struct kevent tev{};
      EV_SET(&tev, timer_id, EVFILT_TIMER, EV_ADD | EV_ONESHOT, NOTE_LEEWAY,
             static_cast<std::intptr_t>(timeout->count()), nullptr);
      tev.ext[1] = static_cast<std::uint64_t>(std::max(leeway.count(), 0));
      kevent(kq, &tev, 1, nullptr, 0, nullptr);
    }
    struct kevent out{};
    const int r = kevent(kq, nullptr, 0, &out, 1, nullptr);
    struct kevent del{};
    EV_SET(&del, timer_id, EVFILT_TIMER, EV_DELETE, 0, 0, nullptr);
    kevent(kq, &del, 1, nullptr, 0, nullptr);
    if (r > 0 && out.filter == EVFILT_TIMER)
      return Wake::Timer;
    return Wake::Signal;
  }

  void wake() override {
    if (kq < 0)
      return;
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
      std::lock_guard lock(mutex);
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
