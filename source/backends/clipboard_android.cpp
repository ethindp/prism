// SPDX-License-Identifier: MPL-2.0

#ifdef __ANDROID__
#include "../backend.h"
#include "../backend_catalog.h"
#include "android/BackendFeatures.hpp"
#include "android/BackendFeaturesHigh.hpp"
#include "android/Unit.hpp"
#include "android/jni/AbstractTextToSpeechBackend.hpp"
#include "android_features.h"
#include <bitset>
#include <djinni/support/jni/djinni_support.hpp>
#include <jni.h>
#include <memory>
#include <string_view>
#include <utility>

class ClipboardBackend final : public TextToSpeechBackend {
private:
  std::shared_ptr<prism::java::AbstractTextToSpeechBackend> backend{nullptr};

  static constexpr std::bitset<64> unavailable{
      BackendFeature::SUPPORTS_SPEAK | BackendFeature::SUPPORTS_OUTPUT |
      BackendFeature::REQUIRES_EXPLICIT_SELECTION};

public:
  static constexpr bool requires_explicit_selection = true;

  ~ClipboardBackend() override = default;

  [[nodiscard]] std::string_view get_name() const override {
    return "Clipboard";
  }

  [[nodiscard]] std::bitset<64> get_features() const override try {
    if (backend) {
      return combined_features(*backend);
    }
    auto *env = djinni::jniGetThreadEnv();
    if (env == nullptr) {
      return unavailable;
    }
    auto *cls = static_cast<jclass>(env->NewLocalRef(
        djinni::jniFindClass("com/github/ethindp/prism/AndroidClipboardBackend")
            .get()));
    if (cls == nullptr) {
      if (env->ExceptionCheck() != 0)
        env->ExceptionClear();
      return unavailable;
    }
    auto *ctor = env->GetMethodID(cls, "<init>", "()V");
    if (ctor == nullptr) {
      if (env->ExceptionCheck() != 0)
        env->ExceptionClear();
      env->DeleteLocalRef(cls);
      return unavailable;
    }
    auto *instance = env->NewObject(cls, ctor);
    if (instance == nullptr) {
      if (env->ExceptionCheck() != 0)
        env->ExceptionClear();
      env->DeleteLocalRef(cls);
      return unavailable;
    }
    auto handle = prism::jni::AbstractTextToSpeechBackend::toCpp(env, instance);
    env->DeleteLocalRef(instance);
    env->DeleteLocalRef(cls);
    if (!handle) {
      return unavailable;
    }
    return combined_features(*handle);
  } catch (...) {
    return unavailable;
  }

  BackendResult<> initialize() override try {
    if (backend != nullptr)
      return std::unexpected(BackendError::AlreadyInitialized);
    auto *jni_env = djinni::jniGetThreadEnv();
    if (jni_env == nullptr)
      return std::unexpected(BackendError::BackendNotAvailable);
    auto *java_class = static_cast<jclass>(jni_env->NewLocalRef(
        djinni::jniFindClass("com/github/ethindp/prism/AndroidClipboardBackend")
            .get()));
    if (java_class == nullptr) {
      if (jni_env->ExceptionCheck() != 0)
        jni_env->ExceptionClear();
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    jmethodID constructor = jni_env->GetMethodID(java_class, "<init>", "()V");
    if (constructor == nullptr) {
      if (jni_env->ExceptionCheck() != 0)
        jni_env->ExceptionClear();
      jni_env->DeleteLocalRef(java_class);
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    auto *instance = jni_env->NewObject(java_class, constructor);
    if (instance == nullptr) {
      if (jni_env->ExceptionCheck() != 0)
        jni_env->ExceptionClear();
      jni_env->DeleteLocalRef(java_class);
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    auto candidate =
        prism::jni::AbstractTextToSpeechBackend::toCpp(jni_env, instance);
    jni_env->DeleteLocalRef(java_class);
    jni_env->DeleteLocalRef(instance);
    if (!candidate) {
      if (jni_env->ExceptionCheck() != 0)
        jni_env->ExceptionClear();
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    if (const auto res = candidate->initialize(); !res)
      return std::unexpected(static_cast<BackendError>(res.error()));
    backend = std::move(candidate);
    return {};
  } catch (...) {
    return std::unexpected(BackendError::BackendNotAvailable);
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!backend)
      return std::unexpected(BackendError::NotInitialized);
    djinni::DataView view(reinterpret_cast<const uint8_t *>(text.data()),
                          text.size());
    if (const auto res = backend->speak(view, interrupt); !res)
      return std::unexpected(static_cast<BackendError>(res.error()));
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    if (!backend)
      return std::unexpected(BackendError::NotInitialized);
    djinni::DataView view(reinterpret_cast<const uint8_t *>(text.data()),
                          text.size());
    if (const auto res = backend->output(view, interrupt); !res)
      return std::unexpected(static_cast<BackendError>(res.error()));
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(ClipboardBackend, Backends::Clipboard, "Clipboard", 0);
#endif
