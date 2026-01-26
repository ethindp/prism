// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#ifdef _WIN32
#include "concurrentqueue.h"
#include "moderncom/com_ptr.h"
#include "moderncom/interfaces.h"
#include <UIAutomation.h>
#include <UIAutomationCore.h>
#include <UIAutomationCoreApi.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <format>
#include <optional>
#include <stop_token>
#include <string>
#include <tchar.h>
#include <thread>
#include <variant>
#include <windows.h>

using namespace belt::com;

constexpr auto WM_UIA_EXECUTE_COMMAND = WM_USER + 1;
constexpr auto WM_UIA_SHUTDOWN = WM_USER + 2;

template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

struct handle_guard {
  HANDLE h{};
  ~handle_guard() {
    if (h != nullptr && h != INVALID_HANDLE_VALUE)
      CloseHandle(h);
  }
  handle_guard(HANDLE h) : h(h) {}
  handle_guard(const handle_guard &) = delete;
  handle_guard &operator=(const handle_guard &) = delete;
};

struct SpeakCommand {
  std::wstring text;
  bool interrupt;
};

struct StopCommand {};

using Command = std::variant<SpeakCommand, StopCommand>;

class UiaNotificationProvider
    : public object<UiaNotificationProvider, IRawElementProviderSimple> {
private:
  std::atomic<HWND> hwnd;

public:
  explicit UiaNotificationProvider(HWND hwnd) noexcept : hwnd{hwnd} {}

  HRESULT STDMETHODCALLTYPE
  get_ProviderOptions(ProviderOptions *pRetVal) noexcept override {
    if (pRetVal == nullptr)
      return E_POINTER;
    *pRetVal =
        (ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading);
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GetPatternProvider(
      [[maybe_unused]] PATTERNID pid, IUnknown **pRetVal) noexcept override {
    if (pRetVal == nullptr)
      return E_POINTER;
    *pRetVal = nullptr;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  GetPropertyValue(PROPERTYID propertyId, VARIANT *pRetVal) noexcept override {
    if (pRetVal == nullptr)
      return E_POINTER;
    VariantInit(pRetVal);
    auto *const h = hwnd.load(std::memory_order_relaxed);
    switch (propertyId) {
    case UIA_ControlTypePropertyId:
      pRetVal->vt = VT_I4;
      pRetVal->lVal = UIA_CustomControlTypeId;
      break;
    case UIA_IsContentElementPropertyId:
    case UIA_IsControlElementPropertyId:
      pRetVal->vt = VT_BOOL;
      pRetVal->boolVal = VARIANT_TRUE;
      break;
    case UIA_NamePropertyId:
      pRetVal->vt = VT_BSTR;
      pRetVal->bstrVal = SysAllocString(_T("Prism Speech"));
      if (pRetVal->bstrVal == nullptr)
        return E_OUTOFMEMORY;
      break;
    case UIA_LiveSettingPropertyId:
      pRetVal->vt = VT_I4;
      pRetVal->lVal = Assertive;
      break;
    case UIA_IsKeyboardFocusablePropertyId:
      pRetVal->vt = VT_BOOL;
      pRetVal->boolVal = VARIANT_FALSE;
      break;
    case UIA_AutomationIdPropertyId:
      pRetVal->vt = VT_BSTR;
      pRetVal->bstrVal = SysAllocString(_T("PrismNotification"));
      if (pRetVal->bstrVal == nullptr)
        return E_OUTOFMEMORY;
      break;
    case UIA_ClassNamePropertyId:
      pRetVal->vt = VT_BSTR;
      pRetVal->bstrVal = SysAllocString(_T("PrismUIAProvider"));
      if (pRetVal->bstrVal == nullptr)
        return E_OUTOFMEMORY;
      break;
    case UIA_NativeWindowHandlePropertyId:
      pRetVal->vt = VT_I4;
      pRetVal->lVal = static_cast<LONG>(reinterpret_cast<INT_PTR>(h));
      break;
    default:
      break;
    }
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(
      IRawElementProviderSimple **pRetVal) noexcept override {
    if (pRetVal == nullptr)
      return E_POINTER;
    const HWND h = hwnd.load(std::memory_order_relaxed);
    return UiaHostProviderFromHwnd(h, pRetVal);
  }

  HRESULT RaiseNotification(const std::wstring &text, bool interrupt) noexcept {
    BSTR bstr_text = SysAllocString(text.c_str());
    BSTR bstr_activity = SysAllocString(_T("Prism"));
    if (bstr_text == nullptr || bstr_activity == nullptr) {
      if (bstr_text != nullptr)
        SysFreeString(bstr_text);
      if (bstr_activity != nullptr)
        SysFreeString(bstr_activity);
      return E_OUTOFMEMORY;
    }
    const auto processing = interrupt ? NotificationProcessing_ImportantAll
                                      : NotificationProcessing_All;
    const auto hr = UiaRaiseNotificationEvent(
        static_cast<IRawElementProviderSimple *>(this),
        NotificationKind_ActionCompleted, processing, bstr_text, bstr_activity);
    SysFreeString(bstr_text);
    SysFreeString(bstr_activity);
    return hr;
  }
};

class UiaBackend final : public TextToSpeechBackend {
private:
  std::jthread thread;
  std::atomic<HWND> hwnd;
  std::atomic<HWND> host;
  std::atomic_flag initialized;
  std::atomic<UiaNotificationProvider *> provider{nullptr};
  std::wstring window_class_name;
  std::optional<bool> ready{std::nullopt};
  std::mutex ready_mtx;
  std::condition_variable ready_cv;
  moodycamel::ConcurrentQueue<Command> uia_command_queue;
  std::atomic<HDESK> target_desktop{nullptr};

  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam,
                                     LPARAM lParam) {
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    auto *self = reinterpret_cast<UiaBackend *>(GetWindowLongPtr(
        hwnd, GWLP_USERDATA)); // NOLINT(performance-no-int-to-ptr)
    auto *prov = self != nullptr
                     ? self->provider.load(std::memory_order_acquire)
                     : nullptr;
    switch (msg) {
    case WM_GETOBJECT:
      if (static_cast<long>(lParam) == UiaRootObjectId && self != nullptr &&
          prov != nullptr) {
        return UiaReturnRawElementProvider(
            hwnd, wParam, lParam,
            static_cast<IRawElementProviderSimple *>(prov));
      }
      break;
    case WM_UIA_EXECUTE_COMMAND: {
      if (self != nullptr && prov != nullptr) {
        Command command;
        while (self->uia_command_queue.try_dequeue(command)) {
          std::visit(overloaded{[prov](const SpeakCommand &cmd) {
                                  prov->RaiseNotification(cmd.text,
                                                          cmd.interrupt);
                                },
                                [prov](const StopCommand &) {
                                  prov->RaiseNotification(_T(""), true);
                                }},
                     command);
        }
      }
      return 0;
    }
    case WM_UIA_SHUTDOWN:
      PostQuitMessage(0);
      return 0;
    case WM_DESTROY:
      return 0;
    default:
      return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }

  void thread_proc(const std::stop_token &st) {
    HDESK desktop = target_desktop.load(std::memory_order_acquire);
    if (desktop != nullptr) {
      if (SetThreadDesktop(target_desktop) == 0) {
        CloseDesktop(target_desktop);
        target_desktop.store(nullptr, std::memory_order_release);
        {
          std::lock_guard g(ready_mtx);
          ready = false;
        }
        ready_cv.notify_all();
        return;
      }
      CloseDesktop(target_desktop);
      target_desktop.store(nullptr, std::memory_order_release);
    }
    handle_guard stop_event(CreateEvent(nullptr, TRUE, FALSE, nullptr));
    if (stop_event.h == nullptr) {
      {
        std::lock_guard g(ready_mtx);
        ready = false;
      }
      ready_cv.notify_all();
      return;
    }
    {
      std::stop_callback cb(st, [h = stop_event.h] { SetEvent(h); });
      const HRESULT coinit_hr = CoInitializeEx(
          nullptr, COINIT_APARTMENTTHREADED | COINIT_SPEED_OVER_MEMORY);
      const bool should_uninit = SUCCEEDED(coinit_hr);
      if (FAILED(coinit_hr) && coinit_hr != RPC_E_CHANGED_MODE) {
        {
          std::lock_guard g(ready_mtx);
          ready = false;
        }
        ready_cv.notify_all();
        return;
      }
      const HINSTANCE hinst = GetModuleHandle(nullptr);
      window_class_name = std::format(L"PrismUIANotificationWindow_{}",
                                      reinterpret_cast<uintptr_t>(this));
      WNDCLASSEX wc{};
      wc.cbSize = sizeof(wc);
      wc.lpfnWndProc = WindowProc;
      wc.hInstance = hinst;
      wc.lpszClassName = window_class_name.c_str();
      if (!RegisterClassEx(&wc)) {
        if (should_uninit)
          CoUninitialize();
        {
          std::lock_guard g(ready_mtx);
          ready = false;
        }
        ready_cv.notify_all();
        return;
      }
      const HWND parent = host.load(std::memory_order_relaxed);
      const HWND w = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                    window_class_name.c_str(),
                                    _T("Prism UIA Notification"), WS_POPUP, 0,
                                    0, 0, 0, parent, nullptr, hinst, nullptr);
      hwnd.store(w, std::memory_order_relaxed);
      if (w == nullptr) {
        UnregisterClass(window_class_name.c_str(), hinst);
        if (should_uninit)
          CoUninitialize();
        {
          std::lock_guard g(ready_mtx);
          ready = false;
        }
        ready_cv.notify_all();
        return;
      }
      SetWindowLongPtr(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
      auto holder = UiaNotificationProvider::create_instance(w);
      auto *p = holder.obj();
      holder.release();
      p->AddRef();
      provider.store(p, std::memory_order_release);
      {
        std::lock_guard g(ready_mtx);
        ready = true;
      }
      ready_cv.notify_all();
      MSG dummy{};
      PeekMessage(&dummy, nullptr, 0, 0, PM_NOREMOVE);
      bool quit = false;
      while (!quit) {
        DWORD r = MsgWaitForMultipleObjectsEx(1, &stop_event.h, INFINITE,
                                              QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (r == WAIT_OBJECT_0) {
          break;
        }
        if (r == WAIT_FAILED) {
          break;
        }
        MSG msg{};
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
          if (msg.message == WM_QUIT) {
            quit = true;
            break;
          }
          TranslateMessage(&msg);
          DispatchMessage(&msg);
        }
      }
      auto *prov = provider.exchange(nullptr, std::memory_order_acq_rel);
      if (prov != nullptr) {
        UiaDisconnectProvider(prov);
      }
      if (HWND h = hwnd.exchange(nullptr, std::memory_order_acq_rel)) {
        DestroyWindow(h);
      }
      UnregisterClass(window_class_name.c_str(), hinst);
      if (should_uninit)
        CoUninitialize();
    }
  }

public:
  ~UiaBackend() override = default;

  std::string_view get_name() const override { return "UIA"; }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    IUIAutomation *uia = nullptr;
    HRESULT hr =
        CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                         IID_IUIAutomation, reinterpret_cast<void **>(&uia));
    if (SUCCEEDED(hr) && uia != nullptr) {
      uia->Release();
      features |= IS_SUPPORTED_AT_RUNTIME;
    }
    features |= SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_STOP;
    return features;
  }

  BackendResult<> initialize() override {
    std::unique_lock lock(ready_mtx);
    if (initialized.test())
      return std::unexpected(BackendError::AlreadyInitialized);
    ready = std::nullopt;
    if (IsWindow(hwnd_in) == 0)
      return std::unexpected(BackendError::InvalidParam);
    auto *h = GetAncestor(hwnd_in, GA_ROOTOWNER);
    if (h == nullptr)
      h = hwnd_in;
    host.store(h, std::memory_order_relaxed);
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId())
      return std::unexpected(BackendError::InvalidParam);
    HDESK current = GetThreadDesktop(GetCurrentThreadId());
    if (current != nullptr) {
      HDESK dup = nullptr;
      if (DuplicateHandle(GetCurrentProcess(), current, GetCurrentProcess(),
                          reinterpret_cast<LPHANDLE>(&dup), 0, FALSE,
                          DUPLICATE_SAME_ACCESS) != 0) {
        target_desktop.store(dup, std::memory_order_release);
      }
    }
    thread = std::jthread(
        [this](const std::stop_token &st) { this->thread_proc(st); });
    bool success = ready_cv.wait_for(lock, std::chrono::seconds(5),
                                     [this] { return ready.has_value(); });
    const HWND w = hwnd.load(std::memory_order_acquire);
    const auto *p = provider.load(std::memory_order_acquire);
    if (!success || !*ready || w == nullptr || p == nullptr)
      return std::unexpected(BackendError::InternalBackendError);
    initialized.test_and_set();
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    const HWND w = hwnd.load(std::memory_order_acquire);
    const auto *p = provider.load(std::memory_order_acquire);
    if (w == nullptr || p == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (!simdutf::validate_utf8(text.data(), text.size()))
      return std::unexpected(BackendError::InvalidUtf8);
    const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
    std::wstring wstr;
    wstr.resize(len);
    if (simdutf::convert_utf8_to_utf16le(
            text.data(), text.size(),
            reinterpret_cast<char16_t *>(wstr.data())) == 0)
      return std::unexpected(BackendError::InvalidUtf8);
    SpeakCommand command{.text = std::move(wstr), .interrupt = interrupt};
    uia_command_queue.enqueue(command);
    if (!PostMessage(w, WM_UIA_EXECUTE_COMMAND, 0, 0)) {
      const DWORD err = GetLastError();
      if (err == ERROR_INVALID_WINDOW_HANDLE)
        return std::unexpected(BackendError::NotInitialized);
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<> stop() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    const HWND w = hwnd.load(std::memory_order_acquire);
    if (w == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    StopCommand cmd{};
    uia_command_queue.enqueue(cmd);
    if (!PostMessage(w, WM_UIA_EXECUTE_COMMAND, 0, 0)) {
      const DWORD err = GetLastError();
      if (err == ERROR_INVALID_WINDOW_HANDLE)
        return std::unexpected(BackendError::NotInitialized);
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(UiaBackend, Backends::UIA, "UIA", 97);
#endif