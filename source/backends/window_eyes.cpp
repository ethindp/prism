// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#include <bitset>
#ifdef _WIN32
#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64__) ||          \
    defined(__amd64) || defined(_M_X64) || defined(_M_IX86) ||                 \
    defined(__i386__)
#ifdef PRISM_ENABLE_LEGACY_BACKENDS
#ifdef PRISM_ENABLE_WINDOW_EYES_LEGACY_BACKEND
#include "raw/wineyes.h"
#include <algorithm>
#include <atlbase.h>
#include <atomic>
#include <ranges>
#include <tchar.h>
#include <windows.h>

class WindowEyesBackend final : public TextToSpeechBackend {
private:
  CComPtr<_Application> we_application;
  CComPtr<_Speech> speech_obj;
  CComPtr<_Braille> braille_obj;
  VARIANT var_opt;
  std::atomic_flag initialized;

public:
  [[nodiscard]] std::string_view get_name() const override {
    return "WindowEyes";
  }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    if (auto* const handle =
            FindWindow(_T("GWMExternalControl"), _T("External Control"));
        handle != INVALID_HANDLE_VALUE && handle != nullptr) {
      IClassFactory *factory = nullptr;
      HRESULT hr = CoGetClassObject(
          CLSID_Application, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
          nullptr, IID_IClassFactory, reinterpret_cast<void **>(&factory));
      if (SUCCEEDED(hr) && factory != nullptr) {
        features |= IS_SUPPORTED_AT_RUNTIME;
        factory->Release();
      }
    }
    features |=
        SUPPORTS_SPEAK | SUPPORTS_BRAILLE | SUPPORTS_OUTPUT | SUPPORTS_STOP;
    return features;
  }

  BackendResult<> initialize() override {
    if (we_application != nullptr && speech_obj != nullptr &&
        braille_obj != nullptr && initialized.test()) {
      return std::unexpected(BackendError::AlreadyInitialized);
    }
    if (!FindWindow(_T("GWMExternalControl"), _T("External Control"))) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    switch (we_application.CoCreateInstance(CLSID_Application)) {
    case S_OK: {
      if (SUCCEEDED(we_application->get_Speech(&speech_obj)) &&
          SUCCEEDED(we_application->get_Braille(&braille_obj))) {
        var_opt.vt = VT_ERROR;
        var_opt.scode = DISP_E_PARAMNOTFOUND;
        initialized.test_and_set();
        return {};
      } else {
        return std::unexpected(BackendError::BackendNotAvailable);
      }
    }
    case REGDB_E_CLASSNOTREG:
    case E_NOINTERFACE:
      return std::unexpected(BackendError::BackendNotAvailable);
    default:
      return std::unexpected(BackendError::Unknown);
    }
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (we_application == nullptr || speech_obj == nullptr ||
        braille_obj == nullptr || !initialized.test()) {
      return std::unexpected(BackendError::NotInitialized);
    }
    if (!FindWindow(_T("GWMExternalControl"), _T("External Control"))) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
    auto *bstr = SysAllocStringLen(nullptr, static_cast<UINT>(len));
    if (bstr == nullptr)
      return std::unexpected(BackendError::MemoryFailure);
    if (const auto res = simdutf::convert_utf8_to_utf16le(
            text.data(), text.size(), reinterpret_cast<char16_t *>(bstr));
        res == 0) {
      SysFreeString(bstr);
      return std::unexpected(BackendError::InvalidUtf8);
    }
    if (interrupt) {
      if (const auto res = stop(); !res) {
        return res;
      }
    }
    const bool succeeded = SUCCEEDED(speech_obj->Speak(bstr, var_opt));
    SysFreeString(bstr);
    if (succeeded)
      return {};
    return std::unexpected(BackendError::InternalBackendError);
  }

  BackendResult<> braille(std::string_view text) override {
    if (we_application == nullptr || speech_obj == nullptr ||
        braille_obj == nullptr || !initialized.test()) {
      return std::unexpected(BackendError::NotInitialized);
    }
    if (!FindWindow(_T("GWMExternalControl"), _T("External Control"))) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
    auto *bstr = SysAllocStringLen(nullptr, static_cast<UINT>(len));
    if (bstr == nullptr)
      return std::unexpected(BackendError::MemoryFailure);
    if (const auto res = simdutf::convert_utf8_to_utf16le(
            text.data(), text.size(), reinterpret_cast<char16_t *>(bstr));
        res == 0) {
      SysFreeString(bstr);
      return std::unexpected(BackendError::InvalidUtf8);
    }
    const bool succeeded =
        SUCCEEDED(braille_obj->Display(bstr, var_opt, var_opt));
    SysFreeString(bstr);
    if (succeeded)
      return {};
    return std::unexpected(BackendError::InternalBackendError);
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    if (const auto res = speak(text, interrupt); !res)
      return res;
    if (const auto res = braille(text); !res)
      return res;
    return {};
  }

  BackendResult<> stop() override {
    if (we_application == nullptr || speech_obj == nullptr ||
        braille_obj == nullptr || !initialized.test()) {
      return std::unexpected(BackendError::NotInitialized);
    }
    if (!FindWindow(_T("GWMExternalControl"), _T("External Control"))) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    if (SUCCEEDED(speech_obj->Silence())) {
      return {};
    }
    return std::unexpected(BackendError::InternalBackendError);
  }
};

REGISTER_BACKEND_WITH_ID(WindowEyesBackend, Backends::WindowEyes, "WindowEyes",
                         100);
#endif
#endif
#endif
#endif