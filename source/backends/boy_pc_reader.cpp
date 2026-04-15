// SPDX-License-Identifier: MPL-2.0
// Todo: this file is full of workarounds. Remove them when they are no longer
// needed.

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#ifdef _WIN32
#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64__) ||          \
    defined(__amd64) || defined(_M_X64) || defined(_M_IX86) ||                 \
    defined(__i386__)
#include "raw/boy_pc_reader.h"
#include <array>
#include <atomic>
#include <bitset>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <string_view>
#include <tchar.h>
#include <thread>
#include <tlhelp32.h>
#include <utility>
#include <windows.h>

// Whoever designed this screen reader API needs to learn how to properly design
// C callbacks...
class BoyPCReaderBackend;
template <int Slot> struct CallbackSlot {
  static BoyPCReaderBackend *instance;
  static void __stdcall callback(int reason);
};

template <int Slot> BoyPCReaderBackend *CallbackSlot<Slot>::instance = nullptr;

template <std::size_t... Is> struct SlotTableImpl {
  static inline std::mutex mtx;
  struct Entry {
    BoyPCReaderBackend **instance;
    BoyCtrlSpeakCompleteFunc func;
  };

  static constexpr std::array entries = {
      Entry{.instance = &CallbackSlot<Is>::instance,
            .func = &CallbackSlot<Is>::callback}...};

  static BoyCtrlSpeakCompleteFunc acquire(BoyPCReaderBackend *obj) {
    std::lock_guard lock(mtx);
    for (auto &e : entries) {
      if (*e.instance == nullptr) {
        *e.instance = obj;
        return e.func;
      }
    }
    return nullptr;
  }

  static void release(BoyPCReaderBackend *obj) {
    assert(obj != nullptr);
    std::lock_guard lock(mtx);
    for (auto &e : entries) {
      if (*e.instance == obj) {
        *e.instance = nullptr;
        return;
      }
    }
  }
};

template <std::size_t N, typename = std::make_index_sequence<N>>
struct MakeSlotTable;

template <std::size_t N, std::size_t... Is>
struct MakeSlotTable<N, std::index_sequence<Is...>> {
  using type = SlotTableImpl<Is...>;
};

template <std::size_t N> using SlotTable = typename MakeSlotTable<N>::type;

class BoyPCReaderBackend final : public TextToSpeechBackend {
private:
  // This limit is most likely overkill, but this is deliberately so
  // The objective is to have a limit so high that you are in practice never
  // going to hit it unless you are deliberately trying to do so
  using Slots = SlotTable<128>;
  std::atomic_flag initialized;
  std::atomic_flag speaking;
  BoyCtrlSpeakCompleteFunc complete_callback{nullptr};
  mutable std::shared_mutex lifecycle_mtx;
  std::thread watchdog_thread;
  std::mutex watchdog_wake_mtx;
  std::condition_variable watchdog_wake;
  std::atomic_flag watchdog_running;

  bool try_reinit() {
    BoyCtrlUninitialize();
    initialized.clear();
    if (BoyCtrlInitializeU8(nullptr) != e_bcerr_success)
      return false;
    if (!BoyCtrlIsReaderRunning()) {
      BoyCtrlUninitialize();
      return false;
    }
    if (!BoyCtrlSetAnyKeyStopSpeaking(false)) {
      BoyCtrlUninitialize();
      return false;
    }
    initialized.test_and_set();
    return true;
  }

  bool attempt_recovery() {
    std::unique_lock lock(lifecycle_mtx);
    if (initialized.test() && BoyCtrlIsReaderRunning())
      return true;
    return try_reinit();
  }

  static bool is_recoverable(BoyCtrlError err) {
    return err == e_bcerr_fail || err == e_bcerr_unavailable;
  }

  static BackendError map_error(BoyCtrlError err) {
    switch (err) {
    case e_bcerr_fail:
    case e_bcerr_arg:
      return BackendError::InternalBackendError;
    case e_bcerr_unavailable:
      return BackendError::BackendNotAvailable;
    default:
      return BackendError::Unknown;
    }
  }

  void watchdog_loop() {
    constexpr auto healthy_interval = std::chrono::seconds(2);
    constexpr auto retry_interval = std::chrono::milliseconds(500);
    constexpr std::uint16_t max_burst = 6;
    constexpr auto backoff_interval = std::chrono::seconds(5);
    std::uint16_t consecutive_failures = 0;
    while (watchdog_running.test(std::memory_order_acquire)) {
      std::chrono::milliseconds interval = healthy_interval;
      if (consecutive_failures > 0)
        interval = consecutive_failures < max_burst ? retry_interval
                                                    : backoff_interval;
      {
        std::unique_lock lock(watchdog_wake_mtx);
        watchdog_wake.wait_for(lock, interval, [this] {
          return !watchdog_running.test(std::memory_order_acquire);
        });
      }
      if (!watchdog_running.test(std::memory_order_acquire))
        break;
      if (!initialized.test())
        continue;
      {
        std::shared_lock lock(lifecycle_mtx);
        if (BoyCtrlIsReaderRunning()) {
          consecutive_failures = 0;
          continue;
        }
      }
      std::unique_lock lock(lifecycle_mtx);
      if (initialized.test() && BoyCtrlIsReaderRunning()) {
        consecutive_failures = 0;
        continue;
      }
      speaking.clear();
      if (try_reinit())
        consecutive_failures = 0;
      else
        ++consecutive_failures;
    }
  }

public:
  void handle_speak_complete([[maybe_unused]] int reason) { speaking.clear(); }

  ~BoyPCReaderBackend() override {
    if (watchdog_running.test()) {
      watchdog_running.clear();
      watchdog_wake.notify_all();
      if (watchdog_thread.joinable())
        watchdog_thread.join();
    }
    {
      std::unique_lock lock(lifecycle_mtx);
      if (initialized.test()) {
        BoyCtrlUninitialize();
        initialized.clear();
      }
    }
    if (complete_callback != nullptr) {
      Slots::release(this);
      complete_callback = nullptr;
    }
  }

  [[nodiscard]] std::string_view get_name() const override {
    return "BoyPCReader";
  }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    constexpr auto boy_pc_reader_processes = std::to_array<std::wstring_view>(
        {_T("BoyService.exe"), _T("BoyHelper.exe"), _T("BoyHlp.exe"),
         _T("BoyPcReader.exe"), _T("BoyPRStart.exe"), _T("BoySpeechSlave.exe"),
         _T("BoyVoiceInput.exe")});
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
      PROCESSENTRY32 entry{};
      entry.dwSize = sizeof(entry);
      if (Process32First(snapshot, &entry)) {
        do {
          if (std::ranges::any_of(
                  boy_pc_reader_processes, [entry](const auto &p) {
                    return std::wstring_view{entry.szExeFile} == p;
                  })) {
            features |= IS_SUPPORTED_AT_RUNTIME;
            break;
          }
        } while (Process32Next(snapshot, &entry));
      }
      CloseHandle(snapshot);
    }
    features |=
        SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_STOP | SUPPORTS_IS_SPEAKING;
    return features;
  }

  BackendResult<> initialize() override {
    if (initialized.test())
      return std::unexpected(BackendError::AlreadyInitialized);
    if (complete_callback == nullptr) {
      complete_callback = Slots::acquire(this);
      if (complete_callback == nullptr)
        return std::unexpected(BackendError::InternalBackendLimitExceeded);
    }
    if (const auto res = BoyCtrlInitializeU8(nullptr); res != e_bcerr_success)
      return std::unexpected(map_error(res));
    if (!BoyCtrlIsReaderRunning()) {
      BoyCtrlUninitialize();
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    if (const auto res = BoyCtrlSetAnyKeyStopSpeaking(false); !res) {
      BoyCtrlUninitialize();
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    initialized.test_and_set();
    watchdog_running.test_and_set(std::memory_order_release);
    watchdog_thread = std::thread(&BoyPCReaderBackend::watchdog_loop, this);
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
    std::wstring wstr;
    wstr.resize(len);
    if (const auto res = simdutf::convert_utf8_to_utf16le(
            text.data(), text.size(),
            reinterpret_cast<char16_t *>(wstr.data()));
        res == 0)
      return std::unexpected(BackendError::InvalidUtf8);
    bool needs_recovery = false;
    {
      std::shared_lock lock(lifecycle_mtx);
      if (!BoyCtrlIsReaderRunning()) {
        needs_recovery = true;
      } else {
        // Temporary workaround to ensure interrupt always stops
        // Todo: remove this when fixed upstream
        if (interrupt) {
          if (const auto res = BoyCtrlStopSpeaking(false);
              res != e_bcerr_success) {
            if (!is_recoverable(res))
              return std::unexpected(map_error(res));
            needs_recovery = true;
          }
        }
        if (!needs_recovery) {
          const auto res = BoyCtrlSpeak(wstr.c_str(), false, !interrupt, true,
                                        complete_callback);
          if (res == e_bcerr_success) {
            speaking.test_and_set();
            return {};
          }
          if (!is_recoverable(res))
            return std::unexpected(map_error(res));
          needs_recovery = true;
        }
      }
    }
    if (needs_recovery) {
      if (!attempt_recovery())
        return std::unexpected(BackendError::BackendNotAvailable);
      std::shared_lock lock(lifecycle_mtx);
      if (interrupt)
        BoyCtrlStopSpeaking(false);
      if (const auto res = BoyCtrlSpeak(wstr.c_str(), false, !interrupt, true,
                                        complete_callback);
          res != e_bcerr_success)
        return std::unexpected(map_error(res));
      speaking.test_and_set();
    }
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<bool> is_speaking() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    std::shared_lock lock(lifecycle_mtx);
    if (!BoyCtrlIsReaderRunning())
      return std::unexpected(BackendError::BackendNotAvailable);
    return speaking.test();
  }

  BackendResult<> stop() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    {
      std::shared_lock lock(lifecycle_mtx);
      if (BoyCtrlIsReaderRunning()) {
        const auto res = BoyCtrlStopSpeaking(false);
        if (res == e_bcerr_success) {
          speaking.clear();
          return {};
        }
        if (!is_recoverable(res))
          return std::unexpected(map_error(res));
      }
    }
    if (!attempt_recovery())
      return std::unexpected(BackendError::BackendNotAvailable);
    std::shared_lock lock(lifecycle_mtx);
    if (const auto res = BoyCtrlStopSpeaking(false); res != e_bcerr_success)
      return std::unexpected(map_error(res));
    speaking.clear();
    return {};
  }
};

template <int Slot> void __stdcall CallbackSlot<Slot>::callback(int reason) {
  if (instance != nullptr)
    instance->handle_speak_complete(reason);
}

REGISTER_BACKEND_WITH_ID(BoyPCReaderBackend, Backends::BoyPCReader,
                         "BoyPCReader", 101);
#endif
#endif