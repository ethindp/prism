// SPDX-License-Identifier: MPL-2.0

#include "prism.h"
#include "backends/backend_registry.h"
#include <limits>
#include <new>
#include <string>

struct PrismContext {
  BackendRegistry &registry;

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
  if (!b)
    return nullptr;
  b->impl = std::move(impl);
  return b;
}

extern "C" {
PRISM_API PrismContext *PRISM_CALL prism_init(void) {
  return new (std::nothrow) PrismContext(BackendRegistry::instance());
}

PRISM_API void PRISM_CALL prism_shutdown(PrismContext *ctx) {
  if (!ctx)
    return;
  delete ctx;
}

PRISM_API size_t PRISM_CALL prism_registry_count(PrismContext *ctx) {
  if (!ctx)
    return std::numeric_limits<std::size_t>::max();
  return ctx->registry.list().size();
}

PRISM_API PrismBackendId PRISM_CALL prism_registry_id_at(PrismContext *ctx,
                                                         size_t index) {
  if (!ctx)
    return PRISM_BACKEND_INVALID;
  const auto list = ctx->registry.list();
  if (index >= list.size())
    return 0;
  return to_prism_id(list[index]);
}

PRISM_API PrismBackendId PRISM_CALL
prism_registry_id(PrismContext *ctx, const char *PRISM_RESTRICT name) {
  if (!ctx || !name)
    return PRISM_BACKEND_INVALID;
  return to_prism_id(ctx->registry.id(name));
}

PRISM_API const char *PRISM_CALL prism_registry_name(PrismContext *ctx,
                                                     PrismBackendId id) {
  if (!ctx)
    return nullptr;
  const auto sv = ctx->registry.name(to_backend_id(id));
  return sv.empty() ? nullptr : sv.data();
}

PRISM_API int PRISM_CALL prism_registry_priority(PrismContext *ctx,
                                                 PrismBackendId id) {
  if (!ctx)
    return -1;
  return ctx->registry.priority(to_backend_id(id));
}

PRISM_API bool PRISM_CALL prism_registry_exists(PrismContext *ctx,
                                                PrismBackendId id) {
  if (!ctx)
    return false;
  return ctx->registry.has(to_backend_id(id));
}

PRISM_API PrismBackend *PRISM_CALL prism_registry_get(PrismContext *ctx,
                                                      PrismBackendId id) {
  if (!ctx)
    return nullptr;
  return wrap_backend(ctx->registry.get(to_backend_id(id)));
}

PRISM_API PrismBackend *PRISM_CALL prism_registry_create(PrismContext *ctx,
                                                         PrismBackendId id) {
  if (!ctx)
    return nullptr;
  return wrap_backend(ctx->registry.create(to_backend_id(id)));
}

PRISM_API PrismBackend *PRISM_CALL
prism_registry_create_best(PrismContext *ctx) {
  if (!ctx)
    return nullptr;
  return wrap_backend(ctx->registry.create_best());
}

PRISM_API PrismBackend *PRISM_CALL prism_registry_acquire(PrismContext *ctx,
                                                          PrismBackendId id) {
  if (!ctx)
    return nullptr;
  return wrap_backend(ctx->registry.acquire(to_backend_id(id)));
}

PRISM_API PrismBackend *PRISM_CALL
prism_registry_acquire_best(PrismContext *ctx) {
  if (!ctx)
    return nullptr;
  return wrap_backend(ctx->registry.acquire_best());
}

PRISM_API void PRISM_CALL prism_backend_free(PrismBackend *backend) {
  if (!backend)
    return;
  delete backend;
}

PRISM_API const char *PRISM_CALL prism_backend_name(PrismBackend *backend) {
  if (!backend)
    return nullptr;
  return backend->impl->get_name().data();
}

PRISM_API PrismError PRISM_CALL
prism_backend_initialize(PrismBackend *backend) {
  if (!backend)
    return PRISM_ERROR_INVALID_PARAM;
  const auto r = backend->impl->initialize();
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_speak(
    PrismBackend *backend, const char *PRISM_RESTRICT text, bool interrupt) {
  if (!backend || !text)
    return PRISM_ERROR_INVALID_PARAM;
  const auto r = backend->impl->speak(text, interrupt);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_speak_to_memory(
    PrismBackend *backend, const char *PRISM_RESTRICT text,
    PrismAudioCallback callback, void *userdata) {
  if (!backend || !callback || !text)
    return PRISM_ERROR_INVALID_PARAM;
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
  if (!backend || !text)
    return PRISM_ERROR_INVALID_PARAM;
  const auto r = backend->impl->braille(text);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_output(
    PrismBackend *backend, const char *PRISM_RESTRICT text, bool interrupt) {
  if (!backend || !text)
    return PRISM_ERROR_INVALID_PARAM;
  const auto r = backend->impl->output(text, interrupt);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_stop(PrismBackend *backend) {
  if (!backend)
    return PRISM_ERROR_INVALID_PARAM;
  const auto r = backend->impl->stop();
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_pause(PrismBackend *backend) {
  if (!backend)
    return PRISM_ERROR_INVALID_PARAM;
  const auto r = backend->impl->pause();
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_resume(PrismBackend *backend) {
  if (!backend)
    return PRISM_ERROR_INVALID_PARAM;
  const auto r = backend->impl->resume();
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_is_speaking(
    PrismBackend *backend, bool *PRISM_RESTRICT out_speaking) {
  if (!backend || !out_speaking)
    return PRISM_ERROR_INVALID_PARAM;
  const auto r = backend->impl->is_speaking();
  if (!r)
    return to_prism_error(r.error());
  *out_speaking = *r;
  return PRISM_OK;
}

PRISM_API PrismError PRISM_CALL prism_backend_set_volume(PrismBackend *backend,
                                                         float volume) {
  if (!backend)
    return PRISM_ERROR_INVALID_PARAM;
  const auto r = backend->impl->set_volume(volume);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_set_rate(PrismBackend *backend,
                                                       float rate) {
  if (!backend)
    return PRISM_ERROR_INVALID_PARAM;
  const auto r = backend->impl->set_rate(rate);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_set_pitch(PrismBackend *backend,
                                                        float pitch) {
  if (!backend)
    return PRISM_ERROR_INVALID_PARAM;
  const auto r = backend->impl->set_pitch(pitch);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_get_volume(
    PrismBackend *backend, float *PRISM_RESTRICT out_volume) {
  if (!backend || !out_volume)
    return PRISM_ERROR_INVALID_PARAM;
  const auto r = backend->impl->get_volume();
  if (!r)
    return to_prism_error(r.error());
  *out_volume = *r;
  return PRISM_OK;
}

PRISM_API PrismError PRISM_CALL
prism_backend_get_rate(PrismBackend *backend, float *PRISM_RESTRICT out_rate) {
  if (!backend || !out_rate)
    return PRISM_ERROR_INVALID_PARAM;
  const auto r = backend->impl->get_rate();
  if (!r)
    return to_prism_error(r.error());
  *out_rate = *r;
  return PRISM_OK;
}

PRISM_API PrismError PRISM_CALL prism_backend_get_pitch(
    PrismBackend *backend, float *PRISM_RESTRICT out_pitch) {
  if (!backend || !out_pitch)
    return PRISM_ERROR_INVALID_PARAM;
  const auto r = backend->impl->get_pitch();
  if (!r)
    return to_prism_error(r.error());
  *out_pitch = *r;
  return PRISM_OK;
}

PRISM_API PrismError PRISM_CALL
prism_backend_refresh_voices(PrismBackend *backend) {
  if (!backend)
    return PRISM_ERROR_INVALID_PARAM;
  const auto r = backend->impl->refresh_voices();
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_count_voices(
    PrismBackend *backend, size_t *PRISM_RESTRICT out_count) {
  if (!backend || !out_count)
    return PRISM_ERROR_INVALID_PARAM;
  const auto r = backend->impl->count_voices();
  if (!r)
    return to_prism_error(r.error());
  *out_count = *r;
  return PRISM_OK;
}

PRISM_API PrismError PRISM_CALL
prism_backend_get_voice_name(PrismBackend *backend, size_t voice_id,
                             const char **PRISM_RESTRICT out_name) {
  if (!backend || !out_name)
    return PRISM_ERROR_INVALID_PARAM;
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
  if (!backend || !out_language)
    return PRISM_ERROR_INVALID_PARAM;
  auto r = backend->impl->get_voice_language(voice_id);
  if (!r)
    return to_prism_error(r.error());
  backend->voice_lang = std::move(*r);
  *out_language = backend->voice_lang.c_str();
  return PRISM_OK;
}

PRISM_API PrismError PRISM_CALL prism_backend_set_voice(PrismBackend *backend,
                                                        size_t voice_id) {
  if (!backend)
    return PRISM_ERROR_INVALID_PARAM;
  const auto r = backend->impl->set_voice(voice_id);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PrismError PRISM_CALL prism_backend_get_voice(
    PrismBackend *backend, size_t *PRISM_RESTRICT out_voice_id) {
  if (!backend || !out_voice_id)
    return PRISM_ERROR_INVALID_PARAM;
  const auto r = backend->impl->get_voice();
  if (!r)
    return to_prism_error(r.error());
  *out_voice_id = *r;
  return PRISM_OK;
}

PRISM_API PrismError PRISM_CALL prism_backend_get_channels(
    PrismBackend *backend, size_t *PRISM_RESTRICT out_channels) {
  if (!backend || !out_channels)
    return PRISM_ERROR_INVALID_PARAM;
  const auto r = backend->impl->get_channels();
  if (!r)
    return to_prism_error(r.error());
  *out_channels = *r;
  return PRISM_OK;
}

PRISM_API PrismError PRISM_CALL prism_backend_get_sample_rate(
    PrismBackend *backend, size_t *PRISM_RESTRICT out_sample_rate) {
  if (!backend || !out_sample_rate)
    return PRISM_ERROR_INVALID_PARAM;
  const auto r = backend->impl->get_sample_rate();
  if (!r)
    return to_prism_error(r.error());
  *out_sample_rate = *r;
  return PRISM_OK;
}

PRISM_API PrismError PRISM_CALL prism_backend_get_bit_depth(
    PrismBackend *backend, size_t *PRISM_RESTRICT out_bit_depth) {
  if (!backend || !out_bit_depth)
    return PRISM_ERROR_INVALID_PARAM;
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
                                        "Unknown error"};
  static_assert(std::size(strings) == PRISM_ERROR_COUNT,
                "Error string table size mismatches error count");
  if (error >= PRISM_ERROR_COUNT)
    return "Unknown error";
  return strings[error];
}
}