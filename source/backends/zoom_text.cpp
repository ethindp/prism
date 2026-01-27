// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#ifdef _WIN32
#include "raw/zt.h"
#include <algorithm>
#include <atlbase.h>
#include <atomic>
#include <ranges>
#include <tchar.h>
#include <windows.h>

class ZoomTextBackend final : public TextToSpeechBackend {
private:
  CComPtr<IZoomText2> controller{nullptr};
  CComPtr<ISpeech2> speech{nullptr};
  std::atomic_flag initialized;

public:
  ~ZoomTextBackend() override = default;

  [[nodiscard]] std::string_view get_name() const override {
    return "ZoomText";
  }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    IClassFactory *factory = nullptr;
    HRESULT hr = CoGetClassObject(
        CLSID_ZoomText, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, nullptr,
        IID_IClassFactory, reinterpret_cast<void **>(&factory));
    if (SUCCEEDED(hr) && factory != nullptr) {
      factory->Release();
      if (FindWindow(_T("ZXSPEECHWNDCLASS"), _T("ZoomText Speech Processor")) !=
          nullptr)
        features |= IS_SUPPORTED_AT_RUNTIME;
    }
    features |=
        SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_IS_SPEAKING | SUPPORTS_STOP;
    return features;
  }

  BackendResult<> initialize() override {
    if (initialized.test())
      return std::unexpected(BackendError::AlreadyInitialized);
    if (FindWindow(_T("ZXSPEECHWNDCLASS"), _T("ZoomText Speech Processor")) ==
        nullptr) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    switch (controller.CoCreateInstance(CLSID_ZoomText)) {
    case S_OK: {
      if (FAILED(controller->get_Speech(&speech))) {
        return std::unexpected(BackendError::BackendNotAvailable);
      }
      initialized.test_and_set();
      return {};
    }
    case REGDB_E_CLASSNOTREG:
    case E_NOINTERFACE:
      return std::unexpected(BackendError::BackendNotAvailable);
    default:
      return std::unexpected(BackendError::Unknown);
    }
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    CComPtr<IVoice> voice;
    if (FAILED(speech->get_CurrentVoice(&voice)))
      return std::unexpected(BackendError::InternalBackendError);
    // Don't ask me why we have to do this, but apparently we do?
    if (interrupt) {
      if (FAILED(voice->put_AllowInterrupt(VARIANT_TRUE))) {
        return std::unexpected(BackendError::InternalBackendError);
      }
    }
    const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
    auto *bstr = SysAllocStringLen(nullptr, static_cast<UINT>(len));
    if (bstr == nullptr) {
      if (interrupt) {
        if (FAILED(voice->put_AllowInterrupt(VARIANT_FALSE))) {
          return std::unexpected(BackendError::InternalBackendError);
        }
      }
      return std::unexpected(BackendError::MemoryFailure);
    }
    if (const auto res = simdutf::convert_utf8_to_utf16le(
            text.data(), text.size(), reinterpret_cast<char16_t *>(bstr));
        res == 0) {
      SysFreeString(bstr);
      if (interrupt) {
        if (FAILED(voice->put_AllowInterrupt(VARIANT_FALSE))) {
          return std::unexpected(BackendError::InternalBackendError);
        }
      }
      return std::unexpected(BackendError::InvalidUtf8);
    }
    if (FAILED(voice->Speak(bstr))) {
      SysFreeString(bstr);
      if (interrupt) {
        if (FAILED(voice->put_AllowInterrupt(VARIANT_FALSE))) {
          return std::unexpected(BackendError::InternalBackendError);
        }
      }
      return std::unexpected(BackendError::SpeakFailure);
    }
    SysFreeString(bstr);
    if (interrupt) {
      if (FAILED(voice->put_AllowInterrupt(VARIANT_FALSE))) {
        return std::unexpected(BackendError::InternalBackendError);
      }
    }
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<bool> is_speaking() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    CComPtr<IVoice> voice;
    if (FAILED(speech->get_CurrentVoice(&voice)))
      return std::unexpected(BackendError::InternalBackendError);
    VARIANT_BOOL result = VARIANT_FALSE;
    if (FAILED(voice->get_Speaking(&result))) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    return result;
  }

  BackendResult<> stop() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    CComPtr<IVoice> voice;
    if (FAILED(speech->get_CurrentVoice(&voice)))
      return std::unexpected(BackendError::InternalBackendError);
    if (FAILED(voice->Stop())) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(ZoomTextBackend, Backends::ZoomText, "ZoomText", 100);
#endif