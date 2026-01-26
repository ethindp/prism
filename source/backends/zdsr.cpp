// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#ifdef _WIN32
#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64__) ||          \
    defined(__amd64) || defined(_M_X64)
#include "raw/zdsr.h"
#include <array>
#include <string_view>
#include <tchar.h>
#include <tlhelp32.h>
#include <windows.h>

class ZdsrBackend final : public TextToSpeechBackend {
public:
  ~ZdsrBackend() override = default;

  [[nodiscard]] std::string_view get_name() const override { return "Zhengdu"; }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    constexpr auto zdsr_processes = std::to_array<std::wstring_view>(
        {_T("ZDSRDaemon.exe"), _T("ZDSRMain.exe"), _T("ZDSRMain_x64.exe"),
         _T("ZDSRMainService.exe"), _T("ZDSRMultiEngine.exe"),
         _T("ZDSRService.exe"), _T("ZDSRStart.exe")});
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
      PROCESSENTRY32 entry{};
      entry.dwSize = sizeof(entry);
      if (Process32First(snapshot, &entry)) {
        do {
          if (std::ranges::any_of(zdsr_processes, [entry](const auto &p) {
                return std::wstring_view{entry.szExeFile} == p;
              })) {
            features |= IS_SUPPORTED_AT_RUNTIME;
            break;
          }
        } while (Process32Next(snapshot, &entry));
      }
      CloseHandle(snapshot);
    }
    features |=
        SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_IS_SPEAKING | SUPPORTS_STOP;
    return features;
  }

  BackendResult<> initialize() override {
    if (const auto res = InitTTS(0, nullptr, TRUE); res > 0)
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
    if (const auto res = Speak(wstr.c_str(), static_cast<BOOL>(interrupt));
        res > 0)
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
    default:
      return false;
    }
  }
};

REGISTER_BACKEND_WITH_ID(ZdsrBackend, Backends::ZDSR, "Zhengdu", 101);
#endif
#endif
