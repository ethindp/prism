// SPDX-License-Identifier: MPL-2.0

#ifdef __EMSCRIPTEN__
#include "../backend.h"
#include "../backend_catalog.h"
#include "../logging.h"
#include <atomic>
#include <bitset>
#include <emscripten/bind.h>
#include <emscripten/proxying.h>
#include <emscripten/threading.h>
#include <emscripten/val.h>
#include <string>
#include <string_view>
#include <utility>

using emscripten::val;

namespace {
void report_refused_clipboard_write(const val &reason) {
  static const LogSource log{"Clipboard"};
  log.warn("the browser refused to write to the clipboard: {}",
           val::global("String")(reason).as<std::string>());
}
} // namespace

EMSCRIPTEN_BINDINGS(prism_clipboard) {
  emscripten::function("prism_clipboard_write_refused",
                       &report_refused_clipboard_write);
}

class ClipboardBackend final : public TextToSpeechBackend {
private:
  std::atomic_flag initialized;

  template <class Function> static auto on_main_thread(Function function) {
    if (emscripten_is_main_runtime_thread())
      return function();
    static emscripten::ProxyingQueue queue;
    decltype(function()) result{};
    queue.proxySync(emscripten_main_runtime_thread_id(),
                    [&] { result = function(); });
    return result;
  }

  static val clipboard_api() {
    const val secure = val::global("isSecureContext");
    if (!secure.isUndefined() && !secure.as<bool>())
      return val::undefined();
    const val navigator = val::global("navigator");
    if (navigator.isUndefined())
      return val::undefined();
    val clipboard = navigator["clipboard"];
    if (clipboard.isUndefined() || clipboard.isNull() ||
        clipboard["writeText"].typeOf().as<std::string>() != "function")
      return val::undefined();
    return clipboard;
  }

  static bool clipboard_available() {
    return on_main_thread([] { return !clipboard_api().isUndefined(); });
  }

public:
  static constexpr bool requires_explicit_selection = true;

  ~ClipboardBackend() override = default;

  [[nodiscard]] std::string_view get_name() const override {
    return "Clipboard";
  }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features =
        SUPPORTS_SPEAK | SUPPORTS_OUTPUT | REQUIRES_EXPLICIT_SELECTION;
    if (clipboard_available())
      features |= IS_SUPPORTED_AT_RUNTIME;
    return features;
  }

  BackendResult<> initialize() override {
    if (initialized.test(std::memory_order_acquire))
      return std::unexpected(BackendError::AlreadyInitialized);
    if (!clipboard_available())
      return std::unexpected(BackendError::BackendNotAvailable);
    initialized.test_and_set(std::memory_order_release);
    return {};
  }

  BackendResult<> speak(std::string_view text,
                        [[maybe_unused]] bool interrupt) override {
    if (!initialized.test(std::memory_order_acquire))
      return std::unexpected(BackendError::NotInitialized);
    return on_main_thread([value = std::string{text}]() -> BackendResult<> {
      const val clipboard = clipboard_api();
      if (clipboard.isUndefined())
        return std::unexpected(BackendError::BackendNotAvailable);
      const val document = val::global("document");
      if (!document.isUndefined() &&
          document["hasFocus"].typeOf().as<std::string>() == "function" &&
          !document.call<bool>("hasFocus"))
        return std::unexpected(BackendError::SpeakFailure);
      // A later refusal can only be logged.
      clipboard.call<val>("writeText", value)
          .call<void>("catch",
                      val::module_property("prism_clipboard_write_refused"));
      return {};
    });
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }
};

REGISTER_BACKEND_WITH_ID(ClipboardBackend, Backends::Clipboard, "Clipboard", 0);
#endif
