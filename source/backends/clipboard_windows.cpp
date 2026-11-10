// SPDX-License-Identifier: MPL-2.0

#ifdef _WIN32
#include "../backend.h"
#include "../backend_catalog.h"
#include <bitset>
#include <chrono>
#include <memory>
#include <simdutf.h>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <windows.h>

class ClipboardBackend final : public TextToSpeechBackend {
private:
  struct DestroyWindowDeleter {
    void operator()(HWND window) const noexcept { DestroyWindow(window); }
  };
  struct GlobalFreeDeleter {
    void operator()(HGLOBAL memory) const noexcept { GlobalFree(memory); }
  };
  using Window =
      std::unique_ptr<std::remove_pointer_t<HWND>, DestroyWindowDeleter>;
  using GlobalMemory =
      std::unique_ptr<std::remove_pointer_t<HGLOBAL>, GlobalFreeDeleter>;

  static constexpr int open_attempts = 10;
  static constexpr auto open_retry_delay = std::chrono::milliseconds(10);

  bool initialized = false;

  static bool open_clipboard(HWND owner) {
    for (int attempt = 0; attempt < open_attempts; ++attempt) {
      if (attempt != 0)
        std::this_thread::sleep_for(open_retry_delay);
      if (OpenClipboard(owner) != FALSE)
        return true;
    }
    return false;
  }

public:
  static constexpr bool requires_explicit_selection = true;

  [[nodiscard]] std::string_view get_name() const override {
    return "Clipboard";
  }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    return IS_SUPPORTED_AT_RUNTIME | SUPPORTS_SPEAK | SUPPORTS_OUTPUT |
           REQUIRES_EXPLICIT_SELECTION;
  }

  BackendResult<> initialize() override {
    if (initialized)
      return std::unexpected(BackendError::AlreadyInitialized);
    initialized = true;
    return {};
  }

  BackendResult<> speak(std::string_view text,
                        [[maybe_unused]] bool interrupt) override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    if (!simdutf::validate_utf8(text.data(), text.size()))
      return std::unexpected(BackendError::InvalidUtf8);
    const auto length =
        simdutf::utf16_length_from_utf8(text.data(), text.size());
    GlobalMemory memory{
        GlobalAlloc(GMEM_MOVEABLE, (length + 1) * sizeof(char16_t))};
    if (!memory)
      return std::unexpected(BackendError::MemoryFailure);
    auto *const destination = static_cast<char16_t *>(GlobalLock(memory.get()));
    if (destination == nullptr)
      return std::unexpected(BackendError::MemoryFailure);
    simdutf::convert_valid_utf8_to_utf16(text.data(), text.size(), destination);
    destination[length] = u'\0';
    GlobalUnlock(memory.get());
    // SetClipboardData needs an owner window. It is destroyed before returning,
    // since nothing here pumps its messages.
    const Window owner{CreateWindowExW(0, L"STATIC", nullptr, 0, 0, 0, 0, 0,
                                       HWND_MESSAGE, nullptr,
                                       GetModuleHandleW(nullptr), nullptr)};
    if (!owner)
      return std::unexpected(BackendError::InternalBackendError);
    if (!open_clipboard(owner.get()))
      return std::unexpected(BackendError::SpeakFailure);
    const bool placed =
        EmptyClipboard() != FALSE &&
        SetClipboardData(CF_UNICODETEXT, memory.get()) != nullptr;
    CloseClipboard();
    if (!placed)
      return std::unexpected(BackendError::SpeakFailure);
    std::ignore = memory.release();
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }
};

REGISTER_BACKEND_WITH_ID(ClipboardBackend, Backends::Clipboard, "Clipboard", 0);
#endif
