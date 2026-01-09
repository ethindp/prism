#include "prism_android_test_jni/com_github_ethindp_prism_tests_android_simpleapp_PrismNative.h"
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
  cfg.jni_env = env;
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
}
