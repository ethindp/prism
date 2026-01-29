// SPDX-License-Identifier: MPL-2.0

#include "prism.h"
#include "backends/backend_registry.h"
#include <cstdint>
#include <limits>
#include <new>
#include <string>
#ifdef __ANDROID__
#include <jni.h>
#endif

struct PrismContext {
  BackendRegistry &registry;
  bool com_initialized;

  explicit PrismContext(BackendRegistry &r) : registry(r) {}
};

struct PrismBackend {
  std::shared_ptr<TextToSpeechBackend> impl;
  std::string voice_name;
  std::string voice_lang;
};

static inline PrismError to_prism_error(BackendError e) {
  return static_cast<PrismError>(static_cast<uint8_t>(e));
}

static inline BackendId to_backend_id(PrismBackendId id) {
  return static_cast<BackendId>(id);
}

static inline PrismBackendId to_prism_id(BackendId id) {
  return static_cast<PrismBackendId>(id);
}

static PrismBackend *wrap_backend(std::shared_ptr<TextToSpeechBackend> impl) {
  if (!impl)
    return nullptr;
  auto *b = new (std::nothrow) PrismBackend;
  if (b == nullptr)
    return nullptr;
  b->impl = std::move(impl);
  return b;
}

extern "C" {

PRISM_API PRISM_NODISCARD PrismConfig PRISM_CALL prism_config_init(void) {
  PrismConfig cfg;
  cfg.version = PRISM_CONFIG_VERSION;
#ifdef __ANDROID__
  cfg.jni_env = nullptr;
#endif
#ifdef _WIN32
  cfg.hwnd = static_cast<HWND>(INVALID_HANDLE_VALUE);
#endif
  return cfg;
}

PRISM_API PrismContext *PRISM_CALL prism_init(PrismConfig *cfg) {
#ifdef _WIN32
  bool owns_com = false;
  switch (CoInitializeEx(nullptr,
                         COINIT_APARTMENTTHREADED | COINIT_SPEED_OVER_MEMORY)) {
  case E_INVALIDARG:
  case E_OUTOFMEMORY:
  case E_UNEXPECTED:
    return nullptr;
  case RPC_E_CHANGED_MODE:
    owns_com = false;
    break;
  default:
    owns_com = true;
    break;
  }
#endif
  if (cfg != nullptr) {
    if (cfg->version != PRISM_CONFIG_VERSION) {
#ifdef _WIN32
      if (owns_com)
        CoUninitialize();
#endif
      return nullptr;
    }
    auto *ctx = new (std::nothrow) PrismContext(BackendRegistry::instance());
    if (ctx == nullptr) {
#ifdef _WIN32
      if (owns_com)
        CoUninitialize();
#endif
      return nullptr;
    }
#ifdef __ANDROID__
    if (cfg->jni_env) {
      JavaVM *vm = nullptr;
      if (cfg->jni_env->GetJavaVM(&vm) != 0)
        return nullptr;
      if (!vm)
        return nullptr;
      ctx->registry.set_java_vm(vm);
    }
#endif
#ifdef _WIN32
    ctx->registry.set_hwnd(cfg->hwnd);
    ctx->com_initialized = owns_com;
#endif
    return ctx;
  }
  auto *ctx = new (std::nothrow) PrismContext(BackendRegistry::instance());
  if (ctx == nullptr) {
#ifdef _WIN32
    if (owns_com)
      CoUninitialize();
#endif
    return nullptr;
  }
#ifdef _WIN32
  ctx->com_initialized = owns_com;
#endif
  return ctx;
}

PRISM_API void PRISM_CALL prism_shutdown(PrismContext *ctx) {
  if (ctx == nullptr)
    return;
#ifdef _WIN32
  if (ctx->com_initialized)
    CoUninitialize();
#endif
  delete ctx;
}

PRISM_API size_t PRISM_CALL prism_registry_count(PrismContext *ctx) {
  return ctx->registry.list().size();
}

PRISM_API PrismBackendId PRISM_CALL prism_registry_id_at(PrismContext *ctx,
                                                         size_t index) {
  const auto list = ctx->registry.list();
  if (index >= list.size())
    return std::numeric_limits<std::size_t>::max();
  return to_prism_id(list[index]);
}

PRISM_API PrismBackendId PRISM_CALL
prism_registry_id(PrismContext *ctx, const char *PRISM_RESTRICT name) {
  return to_prism_id(ctx->registry.id(name));
}

PRISM_API const char *PRISM_CALL prism_registry_name(PrismContext *ctx,
                                                     PrismBackendId id) {
  const auto sv = ctx->registry.name(to_backend_id(id));
  return sv.empty() ? nullptr : sv.data();
}

PRISM_API int PRISM_CALL prism_registry_priority(PrismContext *ctx,
                                                 PrismBackendId id) {
  return ctx->registry.priority(to_backend_id(id));
}

PRISM_API bool PRISM_CALL prism_registry_exists(PrismContext *ctx,
                                                PrismBackendId id) {
  return ctx->registry.has(to_backend_id(id));
}

PRISM_API PrismBackend *PRISM_CALL prism_registry_get(PrismContext *ctx,
                                                      PrismBackendId id) {
  return wrap_backend(ctx->registry.get(to_backend_id(id)));
}

PRISM_API PrismBackend *PRISM_CALL prism_registry_create(PrismContext *ctx,
                                                         PrismBackendId id) {
  return wrap_backend(ctx->registry.create(to_backend_id(id)));
}

PRISM_API PrismBackend *PRISM_CALL
prism_registry_create_best(PrismContext *ctx) {
  return wrap_backend(ctx->registry.create_best());
}

PRISM_API PrismBackend *PRISM_CALL prism_registry_acquire(PrismContext *ctx,
                                                          PrismBackendId id) {
  return wrap_backend(ctx->registry.acquire(to_backend_id(id)));
}

PRISM_API PrismBackend *PRISM_CALL
prism_registry_acquire_best(PrismContext *ctx) {
  return wrap_backend(ctx->registry.acquire_best());
}

PRISM_API void PRISM_CALL prism_backend_free(PrismBackend *backend) {
  if (backend == nullptr)
    return;
  delete backend;
}

PRISM_API const char *PRISM_CALL prism_backend_name(PrismBackend *backend) {
  return backend->impl->get_name().data();
}

PRISM_API std::uint64_t PRISM_CALL
prism_backend_get_features(PrismBackend *backend) {
  return backend->impl->get_features().to_ullong();
}

PRISM_API PrismError PRISM_CALL
prism_backend_initialize(PrismBackend *backend) {
  const auto r = backend->impl->initialize();
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_speak(
    PrismBackend *backend, const char *PRISM_RESTRICT text, bool interrupt) {
  const auto r = backend->impl->speak(text, interrupt);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_speak_to_memory(
    PrismBackend *backend, const char *PRISM_RESTRICT text,
    PrismAudioCallback callback, void *userdata) {
  const auto r = backend->impl->speak_to_memory(
      text,
      [callback, userdata](void *, const float *samples, size_t count,
                           size_t ch, size_t sr) {
        callback(userdata, samples, count, ch, sr);
      },
      nullptr);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL
prism_backend_braille(PrismBackend *backend, const char *PRISM_RESTRICT text) {
  const auto r = backend->impl->braille(text);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_output(
    PrismBackend *backend, const char *PRISM_RESTRICT text, bool interrupt) {
  const auto r = backend->impl->output(text, interrupt);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_stop(PrismBackend *backend) {
  const auto r = backend->impl->stop();
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_pause(PrismBackend *backend) {
  const auto r = backend->impl->pause();
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_resume(PrismBackend *backend) {
  const auto r = backend->impl->resume();
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_is_speaking(
    PrismBackend *backend, bool *PRISM_RESTRICT out_speaking) {
  const auto r = backend->impl->is_speaking();
  if (!r)
    return to_prism_error(r.error());
  *out_speaking = *r;
  return PRISM_OK;
}

PRISM_API PrismError PRISM_CALL prism_backend_set_volume(PrismBackend *backend,
                                                         float volume) {
  const auto r = backend->impl->set_volume(volume);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_set_rate(PrismBackend *backend,
                                                       float rate) {
  const auto r = backend->impl->set_rate(rate);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_set_pitch(PrismBackend *backend,
                                                        float pitch) {
  const auto r = backend->impl->set_pitch(pitch);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_get_volume(
    PrismBackend *backend, float *PRISM_RESTRICT out_volume) {
  const auto r = backend->impl->get_volume();
  if (!r)
    return to_prism_error(r.error());
  *out_volume = *r;
  return PRISM_OK;
}

PRISM_API PrismError PRISM_CALL
prism_backend_get_rate(PrismBackend *backend, float *PRISM_RESTRICT out_rate) {
  const auto r = backend->impl->get_rate();
  if (!r)
    return to_prism_error(r.error());
  *out_rate = *r;
  return PRISM_OK;
}

PRISM_API PrismError PRISM_CALL prism_backend_get_pitch(
    PrismBackend *backend, float *PRISM_RESTRICT out_pitch) {
  const auto r = backend->impl->get_pitch();
  if (!r)
    return to_prism_error(r.error());
  *out_pitch = *r;
  return PRISM_OK;
}

PRISM_API PrismError PRISM_CALL
prism_backend_refresh_voices(PrismBackend *backend) {
  const auto r = backend->impl->refresh_voices();
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_count_voices(
    PrismBackend *backend, size_t *PRISM_RESTRICT out_count) {
  const auto r = backend->impl->count_voices();
  if (!r)
    return to_prism_error(r.error());
  *out_count = *r;
  return PRISM_OK;
}

PRISM_API PrismError PRISM_CALL
prism_backend_get_voice_name(PrismBackend *backend, size_t voice_id,
                             const char **PRISM_RESTRICT out_name) {
  auto r = backend->impl->get_voice_name(voice_id);
  if (!r)
    return to_prism_error(r.error());
  backend->voice_name = std::move(*r);
  *out_name = backend->voice_name.c_str();
  return PRISM_OK;
}

PRISM_API PrismError PRISM_CALL
prism_backend_get_voice_language(PrismBackend *backend, size_t voice_id,
                                 const char **PRISM_RESTRICT out_language) {
  auto r = backend->impl->get_voice_language(voice_id);
  if (!r)
    return to_prism_error(r.error());
  backend->voice_lang = std::move(*r);
  *out_language = backend->voice_lang.c_str();
  return PRISM_OK;
}

PRISM_API PrismError PRISM_CALL prism_backend_set_voice(PrismBackend *backend,
                                                        size_t voice_id) {
  const auto r = backend->impl->set_voice(voice_id);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_get_voice(
    PrismBackend *backend, size_t *PRISM_RESTRICT out_voice_id) {
  const auto r = backend->impl->get_voice();
  if (!r)
    return to_prism_error(r.error());
  *out_voice_id = *r;
  return PRISM_OK;
}

PRISM_API PrismError PRISM_CALL prism_backend_get_channels(
    PrismBackend *backend, size_t *PRISM_RESTRICT out_channels) {
  const auto r = backend->impl->get_channels();
  if (!r)
    return to_prism_error(r.error());
  *out_channels = *r;
  return PRISM_OK;
}

PRISM_API PrismError PRISM_CALL prism_backend_get_sample_rate(
    PrismBackend *backend, size_t *PRISM_RESTRICT out_sample_rate) {
  const auto r = backend->impl->get_sample_rate();
  if (!r)
    return to_prism_error(r.error());
  *out_sample_rate = *r;
  return PRISM_OK;
}

PRISM_API PrismError PRISM_CALL prism_backend_get_bit_depth(
    PrismBackend *backend, size_t *PRISM_RESTRICT out_bit_depth) {
  const auto r = backend->impl->get_bit_depth();
  if (!r)
    return to_prism_error(r.error());
  *out_bit_depth = *r;
  return PRISM_OK;
}

PRISM_API const char *PRISM_CALL prism_error_string(PrismError error) {
  static const char *const strings[] = {"Success",
                                        "Not initialized",
                                        "Invalid parameter",
                                        "Not implemented",
                                        "No voices available",
                                        "Voice not found",
                                        "Speak failure",
                                        "Memory failure",
                                        "Range out of bounds",
                                        "Internal backend error",
                                        "Not speaking",
                                        "Not paused",
                                        "Already paused",
                                        "Invalid UTF-8",
                                        "Invalid operation",
                                        "Already initialized",
                                        "Backend not available",
                                        "Unknown error",
                                        "Invalid audio format"};
  static_assert(std::size(strings) == PRISM_ERROR_COUNT,
                "Error string table size mismatches error count");
  if (error >= PRISM_ERROR_COUNT)
    return "Unknown error";
  return strings[error];
}
}