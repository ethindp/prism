// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#ifdef _WIN32
#include <nvdaController.h>

class NvdaBackend final : public TextToSpeechBackend {
public:
  ~NvdaBackend() override = default;

  [[nodiscard]] std::string_view get_name() const override { return "NVDA"; }

  BackendResult<> initialize() override {
    if (nvdaController_testIfRunning() != ERROR_SUCCESS)
      return std::unexpected(BackendError::BackendNotAvailable);
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (nvdaController_testIfRunning() != ERROR_SUCCESS)
      return std::unexpected(BackendError::BackendNotAvailable);
    if (interrupt) {
      if (nvdaController_cancelSpeech() != ERROR_SUCCESS)
        return std::unexpected(BackendError::InternalBackendError);
    }
    const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
    std::wstring wstr;
    wstr.resize(len);
    if (const auto res = simdutf::convert_utf8_to_utf16le(
            text.data(), text.size(),
            reinterpret_cast<char16_t *>(wstr.data()));
        res == 0)
      return std::unexpected(BackendError::InvalidUtf8);
    if (nvdaController_speakText(wstr.c_str()) != ERROR_SUCCESS)
      return std::unexpected(BackendError::InternalBackendError);
    return {};
  }

  BackendResult<> braille(std::string_view text) override {
    if (nvdaController_testIfRunning() != ERROR_SUCCESS)
      return std::unexpected(BackendError::BackendNotAvailable);
    const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
    std::wstring wstr;
    wstr.resize(len);
    if (const auto res = simdutf::convert_utf8_to_utf16le(
            text.data(), text.size(),
            reinterpret_cast<char16_t *>(wstr.data()));
        res == 0)
      return std::unexpected(BackendError::InvalidUtf8);
    if (nvdaController_brailleMessage(wstr.c_str()) != ERROR_SUCCESS)
      return std::unexpected(BackendError::InternalBackendError);
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
    if (nvdaController_testIfRunning() != ERROR_SUCCESS)
      return std::unexpected(BackendError::BackendNotAvailable);
    if (nvdaController_cancelSpeech() != ERROR_SUCCESS)
      return std::unexpected(BackendError::InternalBackendError);
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(NvdaBackend, Backends::NVDA, "NVDA", 100);
#endif
