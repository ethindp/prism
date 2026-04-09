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
#ifdef PRISM_ENABLE_SYSTEM_ACCESS_LEGACY_BACKEND
#include "raw/sa.h"

class SystemAccessBackend final : public TextToSpeechBackend {
public:
  [[nodiscard]] std::string_view get_name() const override {
    return "SystemAccess";
  }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    if (SA_IsRunning() != 0) {
      features |= IS_SUPPORTED_AT_RUNTIME;
    }
    features |=
        SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_BRAILLE | SUPPORTS_STOP;
    return features;
  }

  BackendResult<> initialize() override {
    if (SA_IsRunning() == 0) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (SA_IsRunning() == 0) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
    std::wstring wstr;
    wstr.resize(len);
    if (const auto res = simdutf::convert_utf8_to_utf16le(
            text.data(), text.size(),
            reinterpret_cast<char16_t *>(wstr.data()));
        res == 0)
      return std::unexpected(BackendError::InvalidUtf8);
    if (interrupt) {
      if (SA_StopAudio() == 0) {
        return std::unexpected(BackendError::InternalBackendError);
      }
    }
    if (SA_SayW(wstr.c_str()) == 0) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }

  BackendResult<> braille(std::string_view text) override {
    if (SA_IsRunning() == 0) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
    std::wstring wstr;
    wstr.resize(len);
    if (const auto res = simdutf::convert_utf8_to_utf16le(
            text.data(), text.size(),
            reinterpret_cast<char16_t *>(wstr.data()));
        res == 0)
      return std::unexpected(BackendError::InvalidUtf8);
    if (SA_BrlShowTextW(wstr.c_str()) == 0) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    if (const auto res = speak(text, interrupt); !res) {
      return res;
    }
    if (const auto res = braille(text); !res) {
      return res;
    }
    return {};
  }

  BackendResult<> stop() override {
    if (SA_IsRunning() == 0) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    if (SA_StopAudio() == 0) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(SystemAccessBackend, Backends::SystemAccess,
                         "SystemAccess", 100);
#endif
#endif
#endif
#endif