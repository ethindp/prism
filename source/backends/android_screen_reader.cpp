// SPDX-License-Identifier: MPL-2.0

#include "backend.h"
#include "backend_registry.h"
#include <atomic>
#include <bitset>
#include <cmath>
#include <limits>
#include <memory>
#ifdef __ANDROID__
#include "java/AudioCallback.hpp"
#include "java/Unit.hpp"
#include "java/jni/AbstractTextToSpeechBackend.hpp"
#include "java/support/jni/djinni_support.hpp"
#include <jni.h>

class AndroidScreenReaderAudioCallbackAdapter
    : public prism::java::AudioCallback {
public:
  using PrismLambda = TextToSpeechBackend::AudioCallback;

private:
  PrismLambda lambda;
  void *userdata;

public:
  AndroidScreenReaderAudioCallbackAdapter(PrismLambda lambda, void *userdata)
      : lambda(std::move(lambda)), userdata(userdata) {}
  void on_audio(std::int64_t java_userdata, const djinni::DataView &samples,
                int64_t sample_count, int64_t channels,
                int64_t sample_rate) override {
    const float *float_samples = reinterpret_cast<const float *>(samples.buf());
    if (lambda) {
      lambda(static_cast<void *>(userdata), float_samples,
             static_cast<std::size_t>(sample_count),
             static_cast<std::size_t>(channels),
             static_cast<std::size_t>(sample_rate));
    }
  }
};

class AndroidScreenReaderBackend final : public TextToSpeechBackend {
private:
  std::shared_ptr<prism::java::AbstractTextToSpeechBackend> backend{nullptr};

public:
  ~AndroidScreenReaderBackend() override = default;

  std::string_view get_name() const override { return "Android screen reader"; }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    auto *env = djinni::jniGetThreadEnv();
    if (env != nullptr) {
      auto java_class =
          env->FindClass("com/github/ethindp/prism/AndroidScreenReaderBackend");
      if (java_class != nullptr) {
        features |= IS_SUPPORTED_AT_RUNTIME;
      } else {
        if (env->ExceptionCheck())
          env->ExceptionClear();
      }
    }
    features |= SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_STOP;
    return features;
  }

  BackendResult<> initialize() override {
    auto *jni_env = djinni::jniGetThreadEnv();
    if (jni_env == nullptr)
      return std::unexpected(BackendError::BackendNotAvailable);
    auto java_class = jni_env->FindClass(
        "com/github/ethindp/prism/AndroidScreenReaderBackend");
    if (java_class == nullptr) {
      if (jni_env->ExceptionCheck())
        jni_env->ExceptionClear();
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    jmethodID constructor = jni_env->GetMethodID(java_class, "<init>", "()V");
    if (!constructor) {
      if (jni_env->ExceptionCheck())
        jni_env->ExceptionClear();
      jni_env->DeleteLocalRef(java_class);
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    auto instance = jni_env->NewObject(java_class, constructor);
    if (!instance) {
      if (jni_env->ExceptionCheck())
        jni_env->ExceptionClear();
      jni_env->DeleteLocalRef(java_class);
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    backend = prism::jni::AbstractTextToSpeechBackend::toCpp(jni_env, instance);
    jni_env->DeleteLocalRef(java_class);
    jni_env->DeleteLocalRef(instance);
    if (!backend) {
      if (jni_env->ExceptionCheck())
        jni_env->ExceptionClear();
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    if (const auto res = backend->initialize(); !res)
      return std::unexpected(static_cast<BackendError>(res.error()));
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    djinni::DataView view(reinterpret_cast<const uint8_t *>(text.data()),
                          text.size());
    if (const auto res = backend->speak(view, interrupt); !res)
      return std::unexpected(static_cast<BackendError>(res.error()));
    return {};
  }

  BackendResult<> speak_to_memory(std::string_view text, AudioCallback callback,
                                  void *userdata) override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    djinni::DataView view(reinterpret_cast<const uint8_t *>(text.data()),
                          text.size());
    auto adapter = std::make_shared<AndroidScreenReaderAudioCallbackAdapter>(
        callback, userdata);
    if (const auto res = backend->speak_to_memory(
            view, adapter, reinterpret_cast<std::int64_t>(userdata));
        !res)
      return std::unexpected(static_cast<BackendError>(res.error()));
    return {};
  }

  BackendResult<> braille(std::string_view text) override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    djinni::DataView view(reinterpret_cast<const uint8_t *>(text.data()),
                          text.size());
    if (const auto res = backend->braille(view); !res)
      return std::unexpected(static_cast<BackendError>(res.error()));
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    djinni::DataView view(reinterpret_cast<const uint8_t *>(text.data()),
                          text.size());
    if (const auto res = backend->output(view, interrupt); !res)
      return std::unexpected(static_cast<BackendError>(res.error()));
    return {};
  }

  BackendResult<bool> is_speaking() override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = backend->is_speaking(); res)
      return *res;
    else
      return std::unexpected(static_cast<BackendError>(res.error()));
  }

  BackendResult<> stop() override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = backend->stop(); res)
      return {};
    else
      return std::unexpected(static_cast<BackendError>(res.error()));
  }

  BackendResult<> pause() override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = backend->pause(); res)
      return {};
    else
      return std::unexpected(static_cast<BackendError>(res.error()));
  }

  BackendResult<> resume() override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = backend->resume(); res)
      return {};
    else
      return std::unexpected(static_cast<BackendError>(res.error()));
  }

  BackendResult<> set_volume(float volume) override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = backend->set_volume(volume); res)
      return {};
    else
      return std::unexpected(static_cast<BackendError>(res.error()));
  }

  BackendResult<float> get_volume() override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = backend->get_volume(); res)
      return *res;
    else
      return std::unexpected(static_cast<BackendError>(res.error()));
  }

  BackendResult<> set_rate(float rate) override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = backend->set_rate(rate); res)
      return {};
    else
      return std::unexpected(static_cast<BackendError>(res.error()));
  }

  BackendResult<float> get_rate() override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = backend->get_rate(); res)
      return *res;
    else
      return std::unexpected(static_cast<BackendError>(res.error()));
  }

  BackendResult<> set_pitch(float pitch) override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = backend->set_pitch(pitch); res)
      return {};
    else
      return std::unexpected(static_cast<BackendError>(res.error()));
  }

  BackendResult<float> get_pitch() override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = backend->get_pitch(); res)
      return *res;
    else
      return std::unexpected(static_cast<BackendError>(res.error()));
  }

  BackendResult<> refresh_voices() override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = backend->refresh_voices(); res)
      return {};
    else
      return std::unexpected(static_cast<BackendError>(res.error()));
  }

  BackendResult<std::size_t> count_voices() override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = backend->count_voices(); res) {
      const auto val = *res;
      if (val < 0)
        return std::unexpected(BackendError::InternalBackendError);
      return static_cast<std::size_t>(val);
    } else {
      return std::unexpected(static_cast<BackendError>(res.error()));
    }
  }

  BackendResult<std::string> get_voice_name(std::size_t id) override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (id >= std::numeric_limits<std::int64_t>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    if (const auto res = backend->get_voice_name(static_cast<std::int64_t>(id));
        res)
      return *res;
    else
      return std::unexpected(static_cast<BackendError>(res.error()));
  }

  BackendResult<std::string> get_voice_language(std::size_t id) override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (id >= std::numeric_limits<std::int64_t>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    if (const auto res =
            backend->get_voice_language(static_cast<std::int64_t>(id));
        res)
      return *res;
    else
      return std::unexpected(static_cast<BackendError>(res.error()));
  }

  BackendResult<> set_voice(std::size_t id) override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (id >= std::numeric_limits<std::int64_t>::max())
      return std::unexpected(BackendError::RangeOutOfBounds);
    if (const auto res = backend->set_voice(static_cast<std::int64_t>(id)); res)
      return {};
    else
      return std::unexpected(static_cast<BackendError>(res.error()));
  }

  BackendResult<std::size_t> get_voice() override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = backend->get_voice(); res) {
      const auto val = *res;
      if (val < 0)
        return std::unexpected(BackendError::RangeOutOfBounds);
      return static_cast<std::size_t>(val);
    } else {
      return std::unexpected(static_cast<BackendError>(res.error()));
    }
  }

  BackendResult<std::size_t> get_channels() override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = backend->get_channels(); res) {
      const auto val = *res;
      if (val < 0)
        return std::unexpected(BackendError::RangeOutOfBounds);
      return static_cast<std::size_t>(val);
    } else {
      return std::unexpected(static_cast<BackendError>(res.error()));
    }
  }

  BackendResult<std::size_t> get_sample_rate() override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = backend->get_sample_rate(); res) {
      const auto val = *res;
      if (val < 0)
        return std::unexpected(BackendError::RangeOutOfBounds);
      return static_cast<std::size_t>(val);
    } else {
      return std::unexpected(static_cast<BackendError>(res.error()));
    }
  }

  BackendResult<std::size_t> get_bit_depth() override {
    if (backend == nullptr)
      return std::unexpected(BackendError::NotInitialized);
    if (const auto res = backend->get_bit_depth(); res) {
      const auto val = *res;
      if (val < 0)
        return std::unexpected(BackendError::RangeOutOfBounds);
      return static_cast<std::size_t>(val);
    } else {
      return std::unexpected(static_cast<BackendError>(res.error()));
    }
  }
};

REGISTER_BACKEND_WITH_ID(AndroidScreenReaderBackend,
                         Backends::AndroidScreenReader, "Android Screen Reader",
                         1);
#endif
