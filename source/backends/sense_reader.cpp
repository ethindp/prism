// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#include <bitset>
#ifdef _WIN32
#include "raw/sense_reader.h"
#include <atlbase.h>
#include <atomic>
#include <tchar.h>
#include <windows.h>

class SenseReaderBackend final : public TextToSpeechBackend {
private:
  CComPtr<IXVApplication> application;
  // Tracks the XVSRD HWND that `application` was bound to so the proxy can be
  // refreshed when Sense Reader is restarted with a new window.
  HWND target_hwnd{nullptr};
  std::atomic_flag initialized;

  bool ensure_connected() {
    HWND current = FindWindow(_T("XVSRD"), nullptr);
    if (current == nullptr) {
      return false;
    }
    if (current == target_hwnd && application != nullptr) {
      return true;
    }
    application.Release();
    if (FAILED(
            application.CoCreateInstance(__uuidof(SenseReaderApplication)))) {
      target_hwnd = nullptr;
      return false;
    }
    target_hwnd = current;
    return true;
  }

public:
  ~SenseReaderBackend() override = default;

  [[nodiscard]] std::string_view get_name() const override {
    return "SenseReader";
  }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    if (FindWindow(_T("XVSRD"), nullptr) != nullptr) {
      IClassFactory *factory = nullptr;
      HRESULT hr = CoGetClassObject(__uuidof(SenseReaderApplication),
                                    CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
                                    nullptr, IID_IClassFactory,
                                    reinterpret_cast<void **>(&factory));
      if (SUCCEEDED(hr) && factory != nullptr) {
        factory->Release();
        features |= IS_SUPPORTED_AT_RUNTIME;
      }
    }
    features |= SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_STOP;
    return features;
  }

  BackendResult<> initialize() override {
    if (initialized.test()) {
      return std::unexpected(BackendError::AlreadyInitialized);
    }
    if (!ensure_connected()) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    initialized.test_and_set();
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!initialized.test()) {
      return std::unexpected(BackendError::NotInitialized);
    }
    if (!ensure_connected()) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    if (interrupt) {
      if (FAILED(application->StopSpeaking())) {
        return std::unexpected(BackendError::InternalBackendError);
      }
    }
    const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
    auto *bstr = SysAllocStringLen(nullptr, static_cast<UINT>(len));
    if (bstr == nullptr) {
      return std::unexpected(BackendError::MemoryFailure);
    }
    if (const auto res = simdutf::convert_utf8_to_utf16le(
            text.data(), text.size(), reinterpret_cast<char16_t *>(bstr));
        res == 0) {
      SysFreeString(bstr);
      return std::unexpected(BackendError::InvalidUtf8);
    }
    const bool succeeded = SUCCEEDED(application->Speak(bstr));
    SysFreeString(bstr);
    if (!succeeded) {
      return std::unexpected(BackendError::SpeakFailure);
    }
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<> stop() override {
    if (!initialized.test()) {
      return std::unexpected(BackendError::NotInitialized);
    }
    if (!ensure_connected()) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    if (FAILED(application->StopSpeaking())) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(SenseReaderBackend, Backends::SenseReader,
                         "SenseReader", 100);
#endif
