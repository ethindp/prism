// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#ifdef _WIN32
#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64__) ||          \
    defined(__amd64) || defined(_M_X64) || defined(_M_IX86) ||                 \
    defined(__i386__)
#include "raw/boy_pc_reader.h"
#include <array>
#include <atomic>
#include <bitset>
#include <string_view>
#include <tchar.h>
#include <tlhelp32.h>
#include <windows.h>

class BoyPCReaderBackend final : public TextToSpeechBackend {
private:
  std::atomic_bool initialized{false};
  static constexpr bool use_reader_channel = false;
  static constexpr bool allow_break = true;

  static BackendError map_error(BoyCtrlError error) {
    switch (error) {
    case e_bcerr_fail:
    case e_bcerr_arg:
      return BackendError::InternalBackendError;
    case e_bcerr_unavailable:
      return BackendError::BackendNotAvailable;
    default:
      return BackendError::Unknown;
    }
  }

public:
  ~BoyPCReaderBackend() override {
    if (initialized.load()) {
      BoyCtrlUninitialize();
      initialized.store(false);
    }
  }

  [[nodiscard]] std::string_view get_name() const override {
    return "BoyPCReader";
  }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    constexpr auto boy_pc_reader_processes = std::to_array<std::wstring_view>(
        {_T("BoyService.exe"), _T("BoyHelper.exe"), _T("BoyHlp.exe"),
         _T("BoyPcReader.exe"), _T("BoyPRStart.exe"), _T("BoySpeechSlave.exe"),
         _T("BoyVoiceInput.exe")});
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
      PROCESSENTRY32 entry{};
      entry.dwSize = sizeof(entry);
      if (Process32First(snapshot, &entry)) {
        do {
          if (std::ranges::any_of(
                  boy_pc_reader_processes, [entry](const auto &p) {
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
        SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_STOP;
    return features;
  }

  BackendResult<> initialize() override {
    if (initialized.load()) {
      return std::unexpected(BackendError::AlreadyInitialized);
    }
    if (const auto res = BoyCtrlInitializeU8(nullptr); res != e_bcerr_success) {
      return std::unexpected(map_error(res));
    }
    if (!BoyCtrlIsReaderRunning()) {
      BoyCtrlUninitialize();
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    initialized.store(true);
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!initialized.load()) {
      return std::unexpected(BackendError::NotInitialized);
    }
    if (!BoyCtrlIsReaderRunning()) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    if (interrupt) {
      if (const auto res = stop(); !res) {
        return res;
      }
    }
    const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
    std::wstring wstr;
    wstr.resize(len);
    if (const auto res = simdutf::convert_utf8_to_utf16le(
            text.data(), text.size(),
            reinterpret_cast<char16_t *>(wstr.data()));
        res == 0)
      return std::unexpected(BackendError::InvalidUtf8);
    if (const auto res =
            BoyCtrlSpeak(wstr.c_str(), use_reader_channel, !interrupt,
                         allow_break, nullptr);
        res != e_bcerr_success) {
      return std::unexpected(map_error(res));
    }
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<bool> is_speaking() override {
    return std::unexpected(BackendError::NotImplemented);
  }

  BackendResult<> stop() override {
    if (!initialized.load()) {
      return std::unexpected(BackendError::NotInitialized);
    }
    if (!BoyCtrlIsReaderRunning()) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    if (const auto res = BoyCtrlStopSpeaking(use_reader_channel);
        res != e_bcerr_success) {
      return std::unexpected(map_error(res));
    }
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(BoyPCReaderBackend, Backends::BoyPCReader,
                         "BoyPCReader", 101);
#endif
#endif
