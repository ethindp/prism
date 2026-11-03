#include "prism_android_test_jni/com_github_ethindp_prism_tests_android_simpleapp_PrismNative.h"
#include <cstddef>
#include <cstdint>
#include <jni.h>
#include <prism.h>

template <class T> static inline T *from_handle(jlong h) {
  return reinterpret_cast<T *>(static_cast<intptr_t>(h));
}

template <class T> static inline jlong to_handle(T *p) {
  return static_cast<jlong>(reinterpret_cast<intptr_t>(p));
}

extern "C" {
JNIEXPORT jlong JNICALL
Java_com_github_ethindp_prism_tests_android_simpleapp_PrismNative_prism_1init(
    JNIEnv *env, jclass) {
  auto cfg = prism_config_init();
  auto *ctx = prism_init(&cfg);
  return to_handle(ctx);
}

JNIEXPORT void JNICALL
Java_com_github_ethindp_prism_tests_android_simpleapp_PrismNative_prism_1shutdown(
    JNIEnv *, jclass, jlong ctxHandle) {
  auto *ctx = from_handle<PrismContext>(ctxHandle);
  prism_shutdown(ctx);
}

JNIEXPORT jlong JNICALL
Java_com_github_ethindp_prism_tests_android_simpleapp_PrismNative_prism_1registry_1create_1best(
    JNIEnv *, jclass, jlong ctxHandle) {
  auto *ctx = from_handle<PrismContext>(ctxHandle);
  auto *backend = prism_registry_create_best(ctx);
  return to_handle(backend);
}

JNIEXPORT void JNICALL
Java_com_github_ethindp_prism_tests_android_simpleapp_PrismNative_prism_1backend_1free(
    JNIEnv *, jclass, jlong backendHandle) {
  auto *backend = from_handle<PrismBackend>(backendHandle);
  prism_backend_free(backend);
}

JNIEXPORT jint JNICALL
Java_com_github_ethindp_prism_tests_android_simpleapp_PrismNative_prism_1backend_1speak(
    JNIEnv *env, jclass, jlong backendHandle, jstring text,
    jboolean interrupt) {
  auto *backend = from_handle<PrismBackend>(backendHandle);
  const char *utf8 = env->GetStringUTFChars(text, nullptr);
  auto err = prism_backend_speak(backend, utf8, static_cast<bool>(interrupt));
  env->ReleaseStringUTFChars(text, utf8);
  return static_cast<jint>(err);
}

JNIEXPORT jlong JNICALL
Java_com_github_ethindp_prism_tests_android_simpleapp_PrismNative_prism_1registry_1count(
    JNIEnv *, jclass, jlong ctxHandle) {
  return static_cast<jlong>(
      prism_registry_count(from_handle<PrismContext>(ctxHandle)));
}

JNIEXPORT jint JNICALL
Java_com_github_ethindp_prism_tests_android_simpleapp_PrismNative_classification_1failures(
    JNIEnv *, jclass, jlong ctxHandle) {
  auto *ctx = from_handle<PrismContext>(ctxHandle);
  constexpr std::uint64_t reader = PRISM_BACKEND_IS_SCREEN_READER;
  constexpr std::uint64_t engine = PRISM_BACKEND_IS_SPEECH_ENGINE;
  jint failures = 0;
  const auto count = prism_registry_count(ctx);
  for (std::size_t i = 0; i < count; ++i) {
    auto *backend = prism_registry_create(ctx, prism_registry_id_at(ctx, i));
    if (backend == nullptr) {
      ++failures;
      continue;
    }
    const auto features = prism_backend_get_features(backend);
    if (((features & reader) != 0) == ((features & engine) != 0))
      ++failures;
    prism_backend_free(backend);
  }
  return failures;
}
}
