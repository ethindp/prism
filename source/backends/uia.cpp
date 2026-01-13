// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#ifdef _WIN32
#include "moderncom/com_ptr.h"
#include "moderncom/interfaces.h"
#include <UIAutomation.h>
#include <UIAutomationCore.h>
#include <UIAutomationCoreApi.h>
#include <atomic>
#include <format>
#include <string>
#include <tchar.h>
#include <thread>
#include <windows.h>

using namespace belt::com;

constexpr auto WM_UIA_SPEAK = WM_USER + 1;
constexpr auto WM_UIA_STOP = WM_USER + 2;
constexpr auto WM_UIA_SHUTDOWN = WM_USER + 3;
class UiaNotificationProvider
    : public object<UiaNotificationProvider, IRawElementProviderSimple> {
private:
  std::atomic<HWND> hwnd{};

public:
  explicit UiaNotificationProvider(HWND hwnd) noexcept : hwnd{hwnd} {}

  HRESULT STDMETHODCALLTYPE
  get_ProviderOptions(ProviderOptions *pRetVal) noexcept override {
    if (!pRetVal)
      return E_POINTER;
    *pRetVal = static_cast<ProviderOptions>(ProviderOptions_ServerSideProvider |
                                            ProviderOptions_UseComThreading);
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  GetPatternProvider(PATTERNID, IUnknown **pRetVal) noexcept override {
    if (!pRetVal)
      return E_POINTER;
    *pRetVal = nullptr;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  GetPropertyValue(PROPERTYID propertyId, VARIANT *pRetVal) noexcept override {
    if (!pRetVal)
      return E_POINTER;
    VariantInit(pRetVal);

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
      if (!pRetVal->bstrVal)
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
      if (!pRetVal->bstrVal)
        return E_OUTOFMEMORY;
      break;
    case UIA_ClassNamePropertyId:
      pRetVal->vt = VT_BSTR;
      pRetVal->bstrVal = SysAllocString(_T("PrismUIAProvider"));
      if (!pRetVal->bstrVal)
        return E_OUTOFMEMORY;
      break;
    case UIA_NativeWindowHandlePropertyId:
      pRetVal->vt = VT_I4;
      pRetVal->lVal = static_cast<LONG>(reinterpret_cast<INT_PTR>(hwnd.load()));
      break;
    }
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(
      IRawElementProviderSimple **pRetVal) noexcept override {
    if (!pRetVal)
      return E_POINTER;
    return UiaHostProviderFromHwnd(hwnd, pRetVal);
  }

  HRESULT RaiseNotification(const std::wstring &text, bool interrupt) noexcept {
    BSTR bstr_text = SysAllocString(text.c_str());
    BSTR bstr_activity = SysAllocString(_T("Prism"));
    if (!bstr_text || !bstr_activity) {
      if (bstr_text)
        SysFreeString(bstr_text);
      if (bstr_activity)
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
  std::atomic<HWND> hwnd{};
  std::atomic<HWND> host{};
  std::atomic<DWORD> thread_id{};
  std::atomic_flag initialized{};
  std::atomic_flag ready{};
  std::atomic<UiaNotificationProvider *> provider{nullptr};
  std::wstring window_class_name;

  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam,
                                     LPARAM lParam) {
    UiaBackend *self =
        reinterpret_cast<UiaBackend *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_GETOBJECT:
      if (static_cast<long>(lParam) == UiaRootObjectId && self &&
          self->provider) {
        return UiaReturnRawElementProvider(
            hwnd, wParam, lParam,
            static_cast<IRawElementProviderSimple *>(self->provider));
      }
      break;
    case WM_UIA_SPEAK:
      if (self && self->provider) {
        auto *data = reinterpret_cast<std::pair<std::wstring, bool> *>(lParam);
        if (data) {
          self->provider.load()->RaiseNotification(data->first, data->second);
          delete data;
        }
      }
      return 0;
    case WM_UIA_STOP:
      if (self && self->provider) {
        self->provider.load()->RaiseNotification(_T(""), true);
      }
      return 0;
    case WM_UIA_SHUTDOWN:
      PostQuitMessage(0);
      return 0;
    case WM_DESTROY:
      return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }

  void thread_proc() {
    const auto coinit_hr = CoInitializeEx(
        nullptr, COINIT_APARTMENTTHREADED | COINIT_SPEED_OVER_MEMORY);
    const bool should_uninit = SUCCEEDED(coinit_hr);
    if (FAILED(coinit_hr) && coinit_hr != RPC_E_CHANGED_MODE) {
      ready.test_and_set();
      return;
    }
    window_class_name = std::format(L"PrismUIANotificationWindow_{}",
                                    reinterpret_cast<uintptr_t>(this));
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = window_class_name.c_str();
    RegisterClassEx(&wc);
    hwnd = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                          window_class_name.c_str(),
                          _T("Prism UIA Notification"), WS_POPUP, 0, 0, 0, 0,
                          host, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!hwnd) {
      if (should_uninit)
        CoUninitialize();
      return;
    }
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    auto holder = UiaNotificationProvider::create_instance(hwnd);
    provider = holder.obj();
    holder.release();
    provider.load()->AddRef();
    thread_id = GetCurrentThreadId();
    ready.test_and_set();
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    UiaDisconnectProvider(provider.load());
    if (provider) {
      provider.load()->Release();
      provider.exchange(nullptr);
    }
    if (HWND h = hwnd.exchange(nullptr)) {
      DestroyWindow(h);
    }
    UnregisterClass(window_class_name.c_str(), GetModuleHandle(nullptr));
    if (should_uninit)
      CoUninitialize();
  }

public:
  ~UiaBackend() override {
    if (thread.joinable()) {
      if (HWND h = hwnd.load()) {
        PostMessage(h, WM_UIA_SHUTDOWN, 0, 0);
      } else if (DWORD tid = thread_id.load()) {
        PostThreadMessage(tid, WM_UIA_SHUTDOWN, 0, 0);
      }
      try {
        thread.join();
      } catch (...) {
      }
    }
  }

  std::string_view get_name() const override { return "UIA"; }

  BackendResult<> initialize() override {
    if (initialized.test())
      return std::unexpected(BackendError::AlreadyInitialized);
    if (!IsWindow(hwnd_in))
      return std::unexpected(BackendError::InvalidParam);
    host = GetAncestor(hwnd_in, GA_ROOTOWNER);
    if (!host)
      host = hwnd_in;
    DWORD pid = 0;
    GetWindowThreadProcessId(host, &pid);
    if (pid != GetCurrentProcessId())
      return std::unexpected(BackendError::InvalidParam);
    thread = std::jthread(&UiaBackend::thread_proc, this);
    int timeout = 100;
    while (!ready.test() && timeout > 0) {
      SleepEx(10, true);
      timeout--;
    }
    if (!ready.test() || !hwnd)
      return std::unexpected(BackendError::InternalBackendError);
    initialized.test_and_set();
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!initialized.test() || !hwnd || !provider)
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
    auto *data = new std::pair<std::wstring, bool>(std::move(wstr), interrupt);
    if (!PostMessage(hwnd, WM_UIA_SPEAK, 0, reinterpret_cast<LPARAM>(data))) {
      delete data;
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<> stop() override {
    if (!initialized.test() || !hwnd)
      return std::unexpected(BackendError::NotInitialized);
    PostMessage(hwnd, WM_UIA_STOP, 0, 0);
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(UiaBackend, Backends::UIA, "UIA", 97);
#endif