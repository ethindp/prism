// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#ifdef _WIN32
#include "raw/zdsr.h"

class ZdsrBackend final : public TextToSpeechBackend {
public:
  ~ZdsrBackend() {}

  std::string_view get_name() const override { return "Zhengdu"; }

  BackendResult<> initialize() override {
    if (const auto res = InitTTS(0, nullptr, true); res > 0)
      return std::unexpected(BackendError::BackendNotAvailable);
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (const auto res = GetSpeakState(); res == 1 || res == 2)
      return std::unexpected(BackendError::BackendNotAvailable);
    const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
    std::wstring wstr;
    wstr.resize(len);
    if (const auto res = simdutf::convert_utf8_to_utf16le(
            text.data(), text.size(),
            reinterpret_cast<char16_t *>(wstr.data()));
        res == 0)
      return std::unexpected(BackendError::InvalidUtf8);
    if (const auto res = Speak(wstr.c_str(), interrupt); res > 0)
      return std::unexpected(BackendError::InternalBackendError);
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<> stop() override {
    if (const auto res = GetSpeakState(); res == 1 || res == 2)
      return std::unexpected(BackendError::BackendNotAvailable);
    StopSpeak();
    return {};
  }

  BackendResult<bool> is_speaking() override {
    switch (GetSpeakState()) {
    case 1:
    case 2:
      return std::unexpected(BackendError::BackendNotAvailable);
    case 3:
      return true;
    case 4:
      return false;
    default:
      return false;
    }
  }
};

REGISTER_BACKEND_WITH_ID(ZdsrBackend, Backends::ZDSR, "Zhengdu", 102);
#endif
