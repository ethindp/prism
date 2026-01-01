// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#ifdef _WIN32
#include <nvdaController.h>

class NvdaBackend final : public Backend {
public:
  ~NvdaBackend() {}

  std::string_view get_name() const override { return "NVDA"; }

  BackendResult<> initialize() override { return {}; }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (nvdaController_testIfRunning() == 0)
      return BackendError::NotInitialized;
    if (interrupt) {
      if (!nvdaController_cancelSpeech())
        return BackendError::InternalBackendError;
    }
    const auto str = std::string(text);
    const auto len = simdutf::utf16_length_from_utf8(str.c_str(), str.size());
    std::wstring wstr;
    wstr.resize(len);
    if (const auto res = simdutf::convert_utf8_to_utf16le(
            text.c_str(), text.size(),
            reinterpret_cast<char16_t *>(wstr.data()));
        res == 0)
      return BackendError::InvalidUtf8;
    if (!nvdaController_speakText(wstr.c_str()))
      return BackendError::InternalBackendError;
    return {};
  }

  BackendResult<> braille(std::string_view text) override {
    if (!nvdaController_testIfRunning())
      return BackendError::NotInitialized;
    const auto str = std::string(text);
    const auto len = simdutf::utf16_length_from_utf8(str.c_str(), str.size());
    std::wstring wstr;
    wstr.resize(len);
    if (const auto res = simdutf::convert_utf8_to_utf16le(
            text.c_str(), text.size(),
            reinterpret_cast<char16_t *>(wstr.data()));
        res == 0)
      return BackendError::InvalidUtf8;
    if (!nvdaController_brailleMessage(wstr.c_str()))
      return BackendError::InternalBackendError;
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    if (const auto res = speak(text, interrupt); !res)
      return res;
    if (const auto res = braille(text); !res)
      return res;
    return {};
  }

  BackendResult<> stop() override {
    if (!nvdaController_testIfRunning())
      return BackendError::NotInitialized;
    if (!nvdaController_cancelSpeech())
      return BackendError::InternalBackendError;
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(NvdaBackend, Backends::NVDA, "NVDA", 0);
#endif
