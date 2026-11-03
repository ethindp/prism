// SPDX-License-Identifier: MPL-2.0

#ifdef _WIN32
#include "../backend.h"
#include "../backend_catalog.h"
#include <algorithm>
#include <atlbase.h>
#include <atomic>
#include <ranges>
#include <raw/fsapi.h>
#include <simdutf.h>
#include <span>
#include <tchar.h>
#include <windows.h>

class JawsBackend final : public ComTextToSpeechBackend {
private:
  CComPtr<IJawsApi> controller;
  std::atomic_flag initialized;

public:
  ~JawsBackend() override = default;

  [[nodiscard]] std::string_view get_name() const override { return "JAWS"; }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    IClassFactory *factory = nullptr;
    const HRESULT hr = CoGetClassObject(
        CLSID_JawsApi, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, nullptr,
        IID_IClassFactory, reinterpret_cast<void **>(&factory));
    if (SUCCEEDED(hr) && factory != nullptr) {
      factory->Release();
      if (IsWindow(FindWindow(_T("JFWUI2"), nullptr)) != FALSE)
        features |= IS_SUPPORTED_AT_RUNTIME;
    }
    features |=
        SUPPORTS_SPEAK | SUPPORTS_BRAILLE | SUPPORTS_OUTPUT | SUPPORTS_STOP;
    return features |= IS_SCREEN_READER;
  }

  BackendResult<> initialize() override {
    if (initialized.test())
      return std::unexpected(BackendError::AlreadyInitialized);
    if (IsWindow(FindWindow(_T("JFWUI2"), nullptr)) == FALSE)
      return std::unexpected(BackendError::BackendNotAvailable);
    switch (controller.CoCreateInstance(CLSID_JawsApi)) {
    case S_OK: {
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
    const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
    auto *bstr = SysAllocStringLen(nullptr, static_cast<UINT>(len));
    if (bstr == nullptr)
      return std::unexpected(BackendError::MemoryFailure);
    if (const auto res = simdutf::convert_valid_utf8_to_utf16(
            text.data(), text.size(), reinterpret_cast<char16_t *>(bstr));
        res == 0) {
      SysFreeString(bstr);
      return std::unexpected(BackendError::InvalidUtf8);
    }
    VARIANT_BOOL result = VARIANT_FALSE;
    const VARIANT_BOOL flush = interrupt ? VARIANT_TRUE : VARIANT_FALSE;
    const bool succeeded =
        SUCCEEDED(controller->SayString(bstr, flush, &result));
    SysFreeString(bstr);
    if (succeeded && result == VARIANT_TRUE)
      return {};
    return std::unexpected(BackendError::InternalBackendError);
  }

  BackendResult<> braille(std::string_view text) override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    constexpr std::wstring_view prefix = _T("BrailleString(\"");
    constexpr std::wstring_view suffix = _T("\")");
    const auto text_len =
        simdutf::utf16_length_from_utf8(text.data(), text.size());
    const auto total_len = prefix.size() + text_len + suffix.size();
    auto *bstr = SysAllocStringLen(nullptr, static_cast<UINT>(total_len));
    if (bstr == nullptr)
      return std::unexpected(BackendError::MemoryFailure);
    const std::span<wchar_t> buffer{bstr, total_len};
    const auto body = buffer.subspan(prefix.size(), text_len);
    std::ranges::copy(prefix, buffer.begin());
    if (text_len > 0 && simdutf::convert_valid_utf8_to_utf16(
                            text.data(), text.size(),
                            reinterpret_cast<char16_t *>(body.data())) == 0) {
      SysFreeString(bstr);
      return std::unexpected(BackendError::InvalidUtf8);
    }
    std::ranges::replace(body, _T('"'), _T('\''));
    std::ranges::copy(suffix, buffer.last(suffix.size()).begin());
    VARIANT_BOOL result = VARIANT_FALSE;
    const bool succeeded = SUCCEEDED(controller->RunFunction(bstr, &result));
    SysFreeString(bstr);
    if (succeeded && result == VARIANT_TRUE)
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
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (SUCCEEDED(controller->StopSpeech()))
      return {};
    return std::unexpected(BackendError::InternalBackendError);
  }
};

REGISTER_BACKEND_WITH_ID(JawsBackend, Backends::JAWS, "JAWS", 100);
#endif