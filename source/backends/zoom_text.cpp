// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#ifdef _WIN32
#include <windows.h>
#include "moderncom/com_ptr.h"
#include "moderncom/interfaces.h"
#include "raw/zt.h"
#include <algorithm>
#include <ranges>
#include <tchar.h>

class ZoomTextBackend final : public TextToSpeechBackend {
private:
  belt::com::com_ptr<IZoomText2> controller;
  belt::com::com_ptr<ISpeech2> speech;

public:
  ~ZoomTextBackend() override {}

  std::string_view get_name() const override { return "ZoomText"; }

  BackendResult<> initialize() override {
    if (controller || speech)
      return std::unexpected(BackendError::AlreadyInitialized);
    if (!FindWindow(_T("ZXSPEECHWNDCLASS"), _T("ZoomText Speech Processor"))) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    switch (controller.CoCreateInstance(CLSID_ZoomText)) {
    case S_OK:
      return {};
    case REGDB_E_CLASSNOTREG:
    case E_NOINTERFACE:
      return std::unexpected(BackendError::BackendNotAvailable);
    default:
      return std::unexpected(BackendError::Unknown);
    }
    if (FAILED(controller->get_Speech(speech.put()))) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!controller || !speech)
      return std::unexpected(BackendError::NotInitialized);
    belt::com::com_ptr<IVoice> voice;
    if (FAILED(speech->get_CurrentVoice(voice.put())))
      return std::unexpected(BackendError::InternalBackendError);
    // Don't ask me why we have to do this, but apparently we do?
    if (interrupt) {
      if (FAILED(voice->put_AllowInterrupt(VARIANT_TRUE))) {
        return std::unexpected(BackendError::InternalBackendError);
      }
    }
    const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
    auto bstr = SysAllocStringLen(nullptr, static_cast<UINT>(len));
    if (!bstr)
      return std::unexpected(BackendError::MemoryFailure);
    if (const auto res = simdutf::convert_utf8_to_utf16le(
            text.data(), text.size(), reinterpret_cast<char16_t *>(bstr));
        res == 0) {
      SysFreeString(bstr);
      return std::unexpected(BackendError::InvalidUtf8);
    }
    if (FAILED(voice->Speak(bstr))) {
      SysFreeString(bstr);
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
    if (!controller || !speech)
      return std::unexpected(BackendError::NotInitialized);
    belt::com::com_ptr<IVoice> voice;
    if (FAILED(speech->get_CurrentVoice(voice.put())))
      return std::unexpected(BackendError::InternalBackendError);
    VARIANT_BOOL result = VARIANT_FALSE;
    if (FAILED(voice->get_Speaking(&result))) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    return result;
  }

  BackendResult<> stop() override {
    if (!controller || !speech)
      return std::unexpected(BackendError::NotInitialized);
    belt::com::com_ptr<IVoice> voice;
    if (FAILED(speech->get_CurrentVoice(voice.put())))
      return std::unexpected(BackendError::InternalBackendError);
    if (FAILED(voice->Stop())) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(ZoomTextBackend, Backends::ZoomText, "ZoomText", 101);
#endif