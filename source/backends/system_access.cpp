// SPDX-License-Identifier: MPL-2.0

#ifdef _WIN32
#ifdef PRISM_ENABLE_LEGACY_BACKENDS
#ifdef PRISM_ENABLE_SYSTEM_ACCESS_LEGACY_BACKEND
#include "backend.h"
#include "backend_registry.h"
#include <atomic>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <tchar.h>
#include <vector>
#include <windows.h>

// To do: make this faster
namespace {
void encode_base128(std::vector<std::byte> &output, std::uint64_t i) {
  if (i == 0) {
    output.emplace_back(static_cast<std::byte>(0));
    return;
  }
  while (i > 0) {
    output.emplace_back(static_cast<std::byte>(i & 0x7F));
    i >>= 7;
  }
}

void encode_uint(std::vector<std::byte> &output, std::uint64_t i) {
  encode_base128(output, i);
  const auto tb = (i > 0x7FFFFFFF) ? 0x85 : 0x81;
  output.emplace_back(static_cast<std::byte>(tb));
}

void encode_byte_string(std::vector<std::byte> &output,
                        std::span<const std::byte> s) {
  encode_base128(output, s.size());
  output.emplace_back(static_cast<std::byte>(0x82));
  output.append_range(s);
}

class List {
  std::size_t count{0};
  std::vector<std::byte> data;

public:
  List() = default;
  void encode(std::vector<std::byte> &output) const {
    encode_base128(output, count);
    output.emplace_back(static_cast<std::byte>(0x80));
    output.append_range(data);
  }

  void push_uint(const std::uint64_t i) {
    count += 1;
    encode_uint(data, i);
  }

  void push_byte_string(std::span<const std::byte> s) {
    count += 1;
    encode_byte_string(data, s);
  }

  void push_str(std::string_view s) {
    push_byte_string(std::as_bytes(std::span{s.data(), s.size()}));
  }

  void push_list(const List &l) {
    count += 1;
    l.encode(data);
  }

  void push_nil() {
    static const List empty_list{};
    push_list(empty_list);
  }
};
} // namespace

class SystemAccessBackend final : public TextToSpeechBackend {
private:
  std::atomic<HWND> window{nullptr};

  static bool send_message(const HWND window, const List &message) {
    List batch;
    batch.push_str("MessageBatch");
    batch.push_list(message);
    std::vector<std::byte> encoded;
    batch.encode(encoded);
    COPYDATASTRUCT cds{.dwData = 0x27BF2,
                       .cbData = static_cast<DWORD>(encoded.size()),
                       .lpData = reinterpret_cast<LPVOID>(encoded.data())};
    return SendMessageTimeout(window, WM_COPYDATA, 0,
                              reinterpret_cast<LPARAM>(std::addressof(cds)),
                              SMTO_BLOCK, 1000, nullptr) != 0;
  }

public:
  [[nodiscard]] std::string_view get_name() const override {
    return "SystemAccess";
  }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    if (FindWindow(_T("FBSAHiddenWindow"), nullptr) != nullptr) {
      features |= IS_SUPPORTED_AT_RUNTIME;
    }
    features |=
        SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_BRAILLE | SUPPORTS_STOP;
    return features;
  }

  BackendResult<> initialize() override {
    auto *const wh = FindWindow(_T("FBSAHiddenWindow"), nullptr);
    if (wh == nullptr) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    window.store(wh, std::memory_order_release);
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    auto *const w = window.load(std::memory_order_acquire);
    if (w == nullptr) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    if (interrupt) {
      if (const auto res = stop(); !res) {
        return res;
      }
    }
    List l{};
    l.push_str("say");
    l.push_str(text);
    if (!send_message(w, l)) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }

  BackendResult<> braille(std::string_view text) override {
    auto *const w = window.load(std::memory_order_acquire);
    if (w == nullptr) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    List l{};
    l.push_str("brl.displayText");
    l.push_str(text);
    l.push_nil();
    l.push_uint(0x7FFFFFFF);
    l.push_nil();
    l.push_nil();
    if (!send_message(w, l)) {
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
    auto *const w = window.load(std::memory_order_acquire);
    if (w == nullptr) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    List l{};
    l.push_str("stopAudio");
    if (!send_message(w, l)) {
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