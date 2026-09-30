// SPDX-License-Identifier: MPL-2.0

#ifdef _WIN32
#include "../backend.h"
#include "../backend_catalog.h"
#include "../logging.h"
#include "../optional_library.h"
#include <atomic>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <raw/pc_talker.h>
#include <simdutf.h>
#include <tchar.h>
#include <type_traits>
#include <windows.h>

namespace {
constexpr const TCHAR *pc_talker_dll = _T("PCTKUSR.dll");

struct PcTalker {
  SharedLibrary library;
  decltype(&::PCTKStatus) PCTKStatus;
  decltype(&::PCTKPReadW) PCTKPReadW;
  decltype(&::PCTKVReset) PCTKVReset;
  decltype(&::PCTKGetVStatus) PCTKGetVStatus;
  decltype(&::PCTKPinStatus) PCTKPinStatus;
  decltype(&::PCTKPinFocusW) PCTKPinFocusW;
  decltype(&::PCTKPinIsFocus) PCTKPinIsFocus;
  decltype(&::PCTKPinWriteW) PCTKPinWriteW;
};

std::optional<PcTalker> load_pc_talker() {
  static const LogSource log{"PCTalker"};
  PcTalker api{.library = open_optional_library(pc_talker_dll, nullptr)};
  if (!api.library) {
    log.debug(_T("{} could not be loaded"), pc_talker_dll);
    return std::nullopt;
  }
  const SharedLibrary &library = api.library;
  const bool complete = library.bind(api.PCTKStatus, "PCTKStatus") &&
                        library.bind(api.PCTKPReadW, "PCTKPReadW") &&
                        library.bind(api.PCTKVReset, "PCTKVReset") &&
                        library.bind(api.PCTKGetVStatus, "PCTKGetVStatus") &&
                        library.bind(api.PCTKPinStatus, "PCTKPinStatus") &&
                        library.bind(api.PCTKPinFocusW, "PCTKPinFocusW") &&
                        library.bind(api.PCTKPinIsFocus, "PCTKPinIsFocus") &&
                        library.bind(api.PCTKPinWriteW, "PCTKPinWriteW");
  if (!complete) {
    log.debug(_T("{} is missing a function prism needs"), pc_talker_dll);
    return std::nullopt;
  }
  return api;
}

const PcTalker *pc_talker() {
  static const std::optional<PcTalker> api = load_pc_talker();
  return api ? &*api : nullptr;
}
} // namespace

class BrailleMarshaller {
private:
  HANDLE request{};
  HANDLE done{};
  HANDLE thread{};
  CRITICAL_SECTION lock{};
  std::function<void()> work;
  std::atomic_flag quit;
  bool lock_initialized = false;
  const PcTalker *api = nullptr;

  static DWORD WINAPI ThreadProc(void *self) {
    return static_cast<BrailleMarshaller *>(self)->Run();
  }

  DWORD Run() {
    api->PCTKPinStatus();
    while (true) {
      if (WaitForSingleObject(request, INFINITE) != WAIT_OBJECT_0)
        return 0;
      if (quit.test(std::memory_order_acquire))
        return 0;
      if (work)
        work();
      SetEvent(done);
    }
  }

public:
  bool init(const PcTalker *library) {
    shutdown();
    api = library;
    quit.clear(std::memory_order_release);
    request = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    done = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (request == nullptr || done == nullptr) {
      shutdown();
      return false;
    }
    InitializeCriticalSection(&lock);
    lock_initialized = true;
    thread = CreateThread(nullptr, 0, ThreadProc, this, 0, nullptr);
    if (thread == nullptr) {
      shutdown();
      return false;
    }
    return true;
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
    if (thread == nullptr && request == nullptr && done == nullptr &&
        !lock_initialized)
      return;
    quit.test_and_set(std::memory_order_release);
    if (request != nullptr)
      SetEvent(request);
    if (thread != nullptr) {
      // Vendor calls must return before their events and lock are destroyed.
      WaitForSingleObject(thread, INFINITE);
      CloseHandle(thread);
      thread = nullptr;
    }
    work = nullptr;
    if (request != nullptr) {
      CloseHandle(request);
      request = nullptr;
    }
    if (done != nullptr) {
      CloseHandle(done);
      done = nullptr;
    }
    if (lock_initialized) {
      DeleteCriticalSection(&lock);
      lock_initialized = false;
    }
  }

  ~BrailleMarshaller() { shutdown(); }
};

class PCTalkerBackend final : public TextToSpeechBackend {
private:
  std::atomic_unsigned_lock_free braille_context;
  std::atomic_flag initialized;
  BrailleMarshaller braille_marshaller;
  const PcTalker *api = nullptr;

public:
  ~PCTalkerBackend() override = default;
  PCTalkerBackend() = default;

  [[nodiscard]] std::string_view get_name() const override {
    return "PCTalker";
  }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    if (const auto *library = pc_talker();
        library != nullptr && library->PCTKStatus() != 0) {
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
    api = pc_talker();
    if (api == nullptr || api->PCTKStatus() == 0) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    if (!braille_marshaller.init(api)) {
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
    const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
    std::wstring wstr;
    wstr.resize(len);
    if (const auto res = simdutf::convert_valid_utf8_to_utf16(
            text.data(), text.size(),
            reinterpret_cast<char16_t *>(wstr.data()));
        res == 0)
      return std::unexpected(BackendError::InvalidUtf8);
    if (api->PCTKPReadW(wstr.c_str(),
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
    if (braille_marshaller.call([this] { return api->PCTKPinStatus(); }) == 0) {
      return std::unexpected(BackendError::InvalidOperation);
    }
    const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
    std::wstring wstr;
    wstr.resize(len);
    if (const auto res = simdutf::convert_valid_utf8_to_utf16(
            text.data(), text.size(),
            reinterpret_cast<char16_t *>(wstr.data()));
        res == 0)
      return std::unexpected(BackendError::InvalidUtf8);
    const auto ctx_val = braille_context.load();
    return braille_marshaller.call([&]() -> BackendResult<> {
      if (ctx_val != 0 &&
          api->PCTKPinIsFocus(static_cast<LONG_PTR>(ctx_val)) != 0) {
        if (api->PCTKPinWriteW(wstr.c_str(), 0, 0) == 0) {
          return std::unexpected(BackendError::InternalBackendError);
        }
        return {};
      } else {
        ULONGLONG it;
        QueryUnbiasedInterruptTime(&it);
        braille_context.store(it);
        if (api->PCTKPinFocusW(static_cast<LONG_PTR>(it), wstr.c_str(),
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
    return api->PCTKGetVStatus() != 0;
  }

  BackendResult<> stop() override {
    if (!initialized.test()) {
      return std::unexpected(BackendError::NotInitialized);
    }
    api->PCTKVReset();
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    if (const auto res = speak(text, interrupt); !res) {
      return std::unexpected(res.error());
    }
    if (braille_marshaller.call([this] { return api->PCTKPinStatus(); }) != 0) {
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
