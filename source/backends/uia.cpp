// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#ifdef _WIN32
#include <windows.h>
#include "moderncom/com_ptr.h"
#include "moderncom/interfaces.h"
#include <UIAutomation.h>
#include <UIAutomationCore.h>
#include <UIAutomationCoreApi.h>
#include <atomic>
#include <string>
#include <thread>
#include <tchar.h>

using namespace belt::com;

constexpr auto UIA_WINDOW_CLASS = _T("PrismUIANotificationWindow");
constexpr auto WM_UIA_SPEAK = WM_USER + 1;
constexpr auto WM_UIA_STOP = WM_USER + 2;
constexpr auto WM_UIA_SHUTDOWN = WM_USER + 3;
class UiaNotificationProvider
    : public object<UiaNotificationProvider, IRawElementProviderSimple> {
private:
  HWND hwnd{};

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
      pRetVal->boolVal = VARIANT_FALSE;
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
    if (FAILED(hr)) {
      SysFreeString(bstr_text);
      SysFreeString(bstr_activity);
    }
    return hr;
  }
};

class UiaBackend final : public TextToSpeechBackend {
private:
  std::thread thread;
  HWND hwnd{};
  DWORD thread_id{};
  std::atomic_flag initialized{};
  std::atomic_flag ready{};
  UiaNotificationProvider *provider{nullptr};

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
          self->provider->RaiseNotification(data->first, data->second);
          delete data;
        }
      }
      return 0;

    case WM_UIA_STOP:
      if (self && self->provider) {
        self->provider->RaiseNotification(_T(""), true);
      }
      return 0;

    case WM_UIA_SHUTDOWN:
      PostQuitMessage(0);
      return 0;

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
  }

  void thread_proc() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = UIA_WINDOW_CLASS;
    RegisterClassEx(&wc);
    hwnd = CreateWindowEx(0, UIA_WINDOW_CLASS, _T("Prism UIA Notification"), 0,
                          0, 0, 0, 0, HWND_MESSAGE, nullptr,
                          GetModuleHandle(nullptr), nullptr);
    if (!hwnd) {
      CoUninitialize();
      return;
    }
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    auto holder = UiaNotificationProvider::create_instance(hwnd);
    provider = holder.obj();
    holder.release();
    provider->AddRef();
    thread_id = GetCurrentThreadId();
    ready.test_and_set();
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    if (provider) {
      provider->Release();
      provider = nullptr;
    }
    if (hwnd) {
      DestroyWindow(hwnd);
      hwnd = nullptr;
    }
    UnregisterClass(UIA_WINDOW_CLASS, GetModuleHandle(nullptr));
    CoUninitialize();
  }

public:
  ~UiaBackend() override {
    if (initialized.test() && thread_id) {
      PostThreadMessage(thread_id, WM_UIA_SHUTDOWN, 0, 0);
      if (thread.joinable())
        thread.join();
    }
  }

  std::string_view get_name() const override { return "UIA"; }

  BackendResult<> initialize() override {
    if (initialized.test())
      return std::unexpected(BackendError::AlreadyInitialized);
    thread = std::thread(&UiaBackend::thread_proc, this);
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
    if (!PostThreadMessage(thread_id, WM_UIA_SPEAK, 0,
                           reinterpret_cast<LPARAM>(data))) {
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
    PostThreadMessage(thread_id, WM_UIA_STOP, 0, 0);
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(UiaBackend, Backends::UIA, "UIA", 97);
#endif