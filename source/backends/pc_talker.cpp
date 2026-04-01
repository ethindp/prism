// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#ifdef _WIN32
#include "raw/pc_talker.h"
#include <atomic>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <windows.h>

class BrailleMarshaller {
private:
  HANDLE request{};
  HANDLE done{};
  HANDLE thread{};
  CRITICAL_SECTION lock{};
  std::function<void()> work;
  std::atomic_flag quit;

  static DWORD WINAPI ThreadProc(void *self) {
    return static_cast<BrailleMarshaller *>(self)->Run();
  }

  DWORD Run() {
    PCTKPinStatus();
    while (!quit.test(std::memory_order_relaxed)) {
      if (WaitForSingleObject(request, 100) == WAIT_OBJECT_0) {
        if (work)
          work();
        SetEvent(done);
      }
    }
    return 0;
  }

public:
  bool init() {
    shutdown();
    request = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    done = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    InitializeCriticalSection(&lock);
    if (request == INVALID_HANDLE_VALUE || done == INVALID_HANDLE_VALUE ||
        request == nullptr || done == nullptr)
      return false;
    thread = CreateThread(nullptr, 0, ThreadProc, this, 0, nullptr);
    return thread != nullptr;
  }

  template <typename F> auto call(F &&fn) -> decltype(fn()) {
    using R = decltype(fn());
    EnterCriticalSection(&lock); // serialize callers
    if constexpr (std::is_void_v<R>) {
      work = [&] { fn(); };
      SetEvent(request);
      WaitForSingleObject(done, INFINITE);
      work = nullptr;
      LeaveCriticalSection(&lock);
    } else {
      R result{};
      work = [&] { result = fn(); };
      SetEvent(request);
      WaitForSingleObject(done, INFINITE);
      work = nullptr;
      LeaveCriticalSection(&lock);
      return result;
    }
  }

  void shutdown() {
    if (thread == nullptr && request == nullptr && done == nullptr)
      return;
    quit.test_and_set(std::memory_order_relaxed);
    WaitForSingleObject(thread, 5000);
    CloseHandle(thread);
    CloseHandle(request);
    CloseHandle(done);
    DeleteCriticalSection(&lock);
    thread = request = done = nullptr;
  }

  ~BrailleMarshaller() { shutdown(); }
};

class PCTalkerBackend final : public TextToSpeechBackend {
private:
  std::atomic_uint64_t braille_context;
  std::atomic_flag initialized;
  BrailleMarshaller braille_marshaller;

public:
  ~PCTalkerBackend() override = default;
  PCTalkerBackend() = default;

  [[nodiscard]] std::string_view get_name() const override {
    return "PCTalker";
  }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    if (PCTKStatus() != 0) {
      features |= IS_SUPPORTED_AT_RUNTIME;
    }
    features |= SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_BRAILLE |
                SUPPORTS_IS_SPEAKING | SUPPORTS_STOP;
    return features;
  }

  BackendResult<> initialize() override {
    if (initialized.test()) {
      return std::unexpected(BackendError::AlreadyInitialized);
    }
    if (PCTKStatus() == 0) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    if (!braille_marshaller.init()) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    ULONGLONG it;
    QueryUnbiasedInterruptTime(&it);
    braille_context.store(it);
    initialized.test_and_set();
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!initialized.test()) {
      return std::unexpected(BackendError::NotInitialized);
    }
    if (PCTKStatus() == 0) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
    std::wstring wstr;
    wstr.resize(len);
    if (const auto res = simdutf::convert_utf8_to_utf16le(
            text.data(), text.size(),
            reinterpret_cast<char16_t *>(wstr.data()));
        res == 0)
      return std::unexpected(BackendError::InvalidUtf8);
    if (PCTKPReadW(wstr.c_str(),
                   interrupt ? PCTK_PRIORITY_OVERRIDE : PCTK_PRIORITY_LOW,
                   TRUE) == 0) {
      return std::unexpected(BackendError::SpeakFailure);
    }
    return {};
  }

  BackendResult<> braille(std::string_view text) override {
    if (!initialized.test()) {
      return std::unexpected(BackendError::NotInitialized);
    }
    if (PCTKStatus() == 0) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    if (braille_marshaller.call([] { return PCTKPinStatus(); }) == 0) {
      return std::unexpected(BackendError::InvalidOperation);
    }
    const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
    std::wstring wstr;
    wstr.resize(len);
    if (const auto res = simdutf::convert_utf8_to_utf16le(
            text.data(), text.size(),
            reinterpret_cast<char16_t *>(wstr.data()));
        res == 0)
      return std::unexpected(BackendError::InvalidUtf8);
    const auto ctx_val = braille_context.load();
    return braille_marshaller.call([&]() -> BackendResult<> {
      if (ctx_val != 0 && PCTKPinIsFocus(static_cast<LONG_PTR>(ctx_val)) != 0) {
        if (PCTKPinWriteW(wstr.c_str(), 0, 0) == 0) {
          return std::unexpected(BackendError::InternalBackendError);
        }
        return {};
      } else {
        ULONGLONG it;
        QueryUnbiasedInterruptTime(&it);
        braille_context.store(it);
        if (PCTKPinFocusW(static_cast<LONG_PTR>(it), wstr.c_str(),
                          PCTK_PIN_MODE_DEFAULT, nullptr, 0) == 0) {
          return std::unexpected(BackendError::InternalBackendError);
        }
        return {};
      }
      return {};
    });
  }

  BackendResult<bool> is_speaking() override {
    if (!initialized.test()) {
      return std::unexpected(BackendError::NotInitialized);
    }
    if (PCTKStatus() == 0) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    return PCTKGetVStatus() != 0;
  }

  BackendResult<> stop() override {
    if (!initialized.test()) {
      return std::unexpected(BackendError::NotInitialized);
    }
    if (PCTKStatus() == 0) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    PCTKVReset();
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    if (const auto res = speak(text, interrupt); !res) {
      return std::unexpected(res.error());
    }
    if (braille_marshaller.call([] { return PCTKPinStatus(); }) != 0) {
      if (const auto res = braille(text); !res) {
        return std::unexpected(res.error());
      }
      return {};
    }
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(PCTalkerBackend, Backends::PCTalker, "PCTalker", 101);
#endif