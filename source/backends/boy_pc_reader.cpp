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
#include <cassert>
#include <mutex>
#include <string_view>
#include <tchar.h>
#include <tlhelp32.h>
#include <utility>
#include <windows.h>

// Whoever designed this screen reader API needs to learn how to properly design
// C callbacks...
class BoyPCReaderBackend;
template <int Slot> struct CallbackSlot {
  static BoyPCReaderBackend *instance;
  static void __stdcall callback(int reason);
};

template <int Slot> BoyPCReaderBackend *CallbackSlot<Slot>::instance = nullptr;

template <std::size_t... Is> struct SlotTableImpl {
  static inline std::mutex mtx;
  struct Entry {
    BoyPCReaderBackend **instance;
    BoyCtrlSpeakCompleteFunc func;
  };

  static constexpr std::array entries = {
      Entry{.instance = &CallbackSlot<Is>::instance,
            .func = &CallbackSlot<Is>::callback}...};

  static BoyCtrlSpeakCompleteFunc acquire(BoyPCReaderBackend *obj) {
    std::lock_guard lock(mtx);
    for (auto &e : entries) {
      if (*e.instance == nullptr) {
        *e.instance = obj;
        return e.func;
      }
    }
    return nullptr;
  }

  static void release(BoyPCReaderBackend *obj) {
    assert(obj != nullptr);
    std::lock_guard lock(mtx);
    for (auto &e : entries) {
      if (*e.instance == obj) {
        *e.instance = nullptr;
        return;
      }
    }
  }
};

template <std::size_t N, typename = std::make_index_sequence<N>>
struct MakeSlotTable;

template <std::size_t N, std::size_t... Is>
struct MakeSlotTable<N, std::index_sequence<Is...>> {
  using type = SlotTableImpl<Is...>;
};

template <std::size_t N> using SlotTable = typename MakeSlotTable<N>::type;

class BoyPCReaderBackend final : public TextToSpeechBackend {
private:
  // This limit is most likely overkill, but this is deliberately so
  // The objective is to have a limit so high that you are in practice never
  // going to hit it unless you are deliberately trying to do so
  using Slots = SlotTable<128>;
  std::atomic_flag initialized;
  std::atomic_flag speaking;
  BoyCtrlSpeakCompleteFunc complete_callback{nullptr};

public:
  void handle_speak_complete([[maybe_unused]] int reason) { speaking.clear(); }

  ~BoyPCReaderBackend() override {
    if (complete_callback != nullptr) {
      Slots::release(this);
      complete_callback = nullptr;
    }
    if (initialized.test()) {
      BoyCtrlUninitialize();
      initialized.clear();
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
        SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_STOP | SUPPORTS_IS_SPEAKING;
    return features;
  }

  BackendResult<> initialize() override {
    if (initialized.test()) {
      return std::unexpected(BackendError::AlreadyInitialized);
    }
    if (complete_callback != nullptr) {
      complete_callback = Slots::acquire(this);
      if (complete_callback != nullptr) {
        return std::unexpected(BackendError::InternalBackendLimitExceeded);
      }
    }
    if (const auto res = BoyCtrlInitializeU8(nullptr); res != e_bcerr_success) {
      switch (res) {
      case e_bcerr_fail:
      case e_bcerr_arg:
        return std::unexpected(BackendError::InternalBackendError);
      case e_bcerr_unavailable:
        return std::unexpected(BackendError::BackendNotAvailable);
      default:
        return std::unexpected(BackendError::Unknown);
      }
    }
    if (!BoyCtrlIsReaderRunning()) {
      BoyCtrlUninitialize();
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    initialized.test_and_set();
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!initialized.test()) {
      return std::unexpected(BackendError::NotInitialized);
    }
    const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
    std::wstring wstr;
    wstr.resize(len);
    if (const auto res = simdutf::convert_utf8_to_utf16le(
            text.data(), text.size(),
            reinterpret_cast<char16_t *>(wstr.data()));
        res == 0)
      return std::unexpected(BackendError::InvalidUtf8);
    if (const auto res = BoyCtrlSpeak(wstr.c_str(), false, !interrupt, true,
                                      complete_callback);
        res != e_bcerr_success) {
      switch (res) {
      case e_bcerr_fail:
      case e_bcerr_arg:
        return std::unexpected(BackendError::InternalBackendError);
      case e_bcerr_unavailable:
        return std::unexpected(BackendError::BackendNotAvailable);
      default:
        return std::unexpected(BackendError::Unknown);
      }
    }
    speaking.test_and_set();
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<bool> is_speaking() override {
    if (!initialized.test()) {
      return std::unexpected(BackendError::NotInitialized);
    }
    return speaking.test();
  }

  BackendResult<> stop() override {
    if (!initialized.test()) {
      return std::unexpected(BackendError::NotInitialized);
    }
    if (const auto res = BoyCtrlStopSpeaking(false); res != e_bcerr_success) {
      switch (res) {
      case e_bcerr_fail:
      case e_bcerr_arg:
        return std::unexpected(BackendError::InternalBackendError);
      case e_bcerr_unavailable:
        return std::unexpected(BackendError::BackendNotAvailable);
      default:
        return std::unexpected(BackendError::Unknown);
      }
    }
    return {};
  }
};

template <int Slot> void __stdcall CallbackSlot<Slot>::callback(int reason) {
  if (instance != nullptr)
    instance->handle_speak_complete(reason);
}

REGISTER_BACKEND_WITH_ID(BoyPCReaderBackend, Backends::BoyPCReader,
                         "BoyPCReader", 101);
#endif
#endif
