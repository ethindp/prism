// SPDX-License-Identifier: MPL-2.0

#include "../backend.h"
#include "../backend_catalog.h"
#include "../utils.h"
#include <simdutf.h>
#if (defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) ||      \
     defined(__OpenBSD__) || defined(__DragonFly__)) &&                        \
    !defined(__ANDROID__)
#ifdef PRISM_HAVE_ORCA
#define PRISM_ORCA_THROUGH_BRIDGE
#include <bridge.h>
#endif
#elifdef _WIN32
#define PRISM_ORCA_THROUGH_BRIDGE
#include <raw/prism_orca_bridge.h>
#include <tchar.h>
#include <windows.h>
#endif
#ifdef PRISM_ORCA_THROUGH_BRIDGE
#include <atomic>

namespace {
// On Windows the bridge is a Wine library, so it only answers under Wine.
bool bridge_reachable() {
#ifdef _WIN32
  auto *const ntdll = GetModuleHandle(_T("ntdll.dll"));
  return ntdll != nullptr &&
         GetProcAddress(ntdll, "wine_get_version") != nullptr;
#else
  return true;
#endif
}
} // namespace

class OrcaBackend final : public TextToSpeechBackend {
private:
  std::atomic<PrismOrcaDBusInstance *> instance{nullptr};

public:
  ~OrcaBackend() override {
    if (instance != nullptr) {
      prism_orca_destroy(instance);
      instance = nullptr;
    }
  }

  [[nodiscard]] std::string_view get_name() const override { return "Orca"; }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    if (bridge_reachable() && prism_orca_available()) {
      features |= IS_SUPPORTED_AT_RUNTIME;
    }
    features |= SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_STOP;
    return features;
  }

  BackendResult<> initialize() override {
    if (instance != nullptr) {
      return std::unexpected(BackendError::AlreadyInitialized);
    }
    if (!bridge_reachable() || !prism_orca_available()) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    PrismOrcaDBusInstance *h = nullptr;
    if (!prism_orca_create(&h)) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    if (h == nullptr) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    instance.store(h);
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (instance == nullptr) {
      return std::unexpected(BackendError::NotInitialized);
    }
    if (interrupt)
      if (const auto res = stop(); !res)
        return res;
    if (const auto res = prism_orca_speak(instance, text.data()); !res) {
      return std::unexpected(BackendError::SpeakFailure);
    }
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<> stop() override {
    if (instance == nullptr) {
      return std::unexpected(BackendError::NotInitialized);
    }
    if (const auto res = prism_orca_stop(instance); !res) {
      return std::unexpected(BackendError::InternalBackendError);
    }
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(OrcaBackend, Backends::Orca, "Orca", 100);
#endif
