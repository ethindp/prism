// SPDX-License-Identifier: MPL-2.0

#include "prism.h"
#include "backend_enumerator.h"
#include "frozen_registry.h"
#include "logging.h"
#include "power_notifier.h"
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <simdutf/simdutf.h>
#include <string>
#ifdef __ANDROID__
#include <jni.h>
#endif
#if (defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) ||      \
     defined(__OpenBSD__) || defined(__DragonFly__)) &&                        \
    !defined(__ANDROID__)
#ifndef NO_ORCA
#include <giomm/init.h>
#endif
#endif

struct PrismContext {
  FrozenRegistry *registry;
  std::unique_ptr<BackendEnumerator> enumerator;
  bool com_initialized = false;

  explicit PrismContext(FrozenRegistry *registry) : registry(registry) {
    registry->retain();
  }
  ~PrismContext() {
    enumerator.reset();
    registry->release();
  }
};

struct PrismBackend {
  std::shared_ptr<TextToSpeechBackend> impl;
  std::string voice_name;
  std::string voice_lang;
};

// This below function definition is defined in the custom backend adapter
BackendFactory make_custom_factory(const PrismBackendVTable *vtable,
                                   void *userdata,
                                   void (*userdata_free)(void *),
                                   std::uint64_t features, std::string name);

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
  PrismConfig cfg{};
  cfg.version = PRISM_CONFIG_VERSION;
  return cfg;
}

PRISM_API PRISM_NODISCARD PrismContext *PRISM_CALL
prism_init(PrismConfig *cfg) {
  init_logging_from_env();
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
#if (defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) ||      \
     defined(__OpenBSD__) || defined(__DragonFly__)) &&                        \
    !defined(__ANDROID__)
#ifndef NO_ORCA
  Gio::init();
#endif
#endif
  FrozenRegistry *registry = FrozenRegistry::global();
  if (cfg != nullptr) {
    if (cfg->version > PRISM_CONFIG_VERSION) {
#ifdef _WIN32
      if (owns_com)
        CoUninitialize();
#endif
      return nullptr;
    }
    if (cfg->version >= 3 && cfg->registry != nullptr)
      registry = reinterpret_cast<FrozenRegistry *>(cfg->registry);
  }
  auto *ctx = new (std::nothrow) PrismContext(registry);
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
  if (cfg != nullptr && cfg->version >= 3 &&
      cfg->availability_callback != nullptr) {
    try {
      ctx->enumerator = std::make_unique<BackendEnumerator>(
          registry, cfg->availability_callback, cfg->availability_userdata,
          cfg->availability_poll_interval_ms,
          cfg->availability_debounce_samples, cfg->availability_backoff_max_ms,
          cfg->availability_auto_power_manage);
    } catch (...) {
      prism_log(PRISM_LOG_LEVEL_ERROR, "prism",
                "failed to start backend enumerator");
    }
  }
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

PRISM_API void PRISM_CALL prism_availability_poll_pause(PrismContext *ctx) {
  if (ctx != nullptr && ctx->enumerator)
    ctx->enumerator->pause();
}

PRISM_API void PRISM_CALL prism_availability_poll_resume(PrismContext *ctx) {
  if (ctx != nullptr && ctx->enumerator)
    ctx->enumerator->resume();
}

PRISM_API PRISM_NODISCARD bool PRISM_CALL
prism_availability_auto_power_supported(void) {
  return PowerNotifier::supported();
}

PRISM_API PRISM_NODISCARD size_t PRISM_CALL
prism_registry_count(PrismContext *ctx) {
  return ctx->registry->list().size();
}

PRISM_API PRISM_NODISCARD PrismBackendId PRISM_CALL
prism_registry_id_at(PrismContext *ctx, size_t index) {
  return to_prism_id(ctx->registry->id_at(index));
}

PRISM_API PRISM_NODISCARD PrismBackendId PRISM_CALL
prism_registry_id(PrismContext *ctx, const char *PRISM_RESTRICT name) {
  return to_prism_id(ctx->registry->id(name));
}

PRISM_API PRISM_NODISCARD const char *PRISM_CALL
prism_registry_name(PrismContext *ctx, PrismBackendId id) {
  const auto sv = ctx->registry->name(to_backend_id(id));
  return sv.empty() ? nullptr : sv.data();
}

PRISM_API PRISM_NODISCARD int PRISM_CALL
prism_registry_priority(PrismContext *ctx, PrismBackendId id) {
  return ctx->registry->priority(to_backend_id(id));
}

PRISM_API PRISM_NODISCARD bool PRISM_CALL
prism_registry_exists(PrismContext *ctx, PrismBackendId id) {
  return id != PRISM_BACKEND_INVALID && ctx->registry->has(to_backend_id(id));
}

PRISM_API PRISM_NODISCARD PrismBackend *PRISM_CALL
prism_registry_get(PrismContext *ctx, PrismBackendId id) {
  return wrap_backend(ctx->registry->get(to_backend_id(id)));
}

PRISM_API PRISM_NODISCARD PrismBackend *PRISM_CALL
prism_registry_create(PrismContext *ctx, PrismBackendId id) {
  return wrap_backend(ctx->registry->create(to_backend_id(id)));
}

PRISM_API PRISM_NODISCARD PrismBackend *PRISM_CALL
prism_registry_create_best(PrismContext *ctx) {
  return wrap_backend(ctx->registry->create_best());
}

PRISM_API PRISM_NODISCARD PrismBackend *PRISM_CALL
prism_registry_acquire(PrismContext *ctx, PrismBackendId id) {
  return wrap_backend(ctx->registry->acquire(to_backend_id(id)));
}

PRISM_API PRISM_NODISCARD PrismBackend *PRISM_CALL
prism_registry_acquire_best(PrismContext *ctx) {
  return wrap_backend(ctx->registry->acquire_best());
}

PRISM_API PRISM_NODISCARD PrismRegistryBuilder *PRISM_CALL
prism_registry_builder_new(void) {
  return reinterpret_cast<PrismRegistryBuilder *>(new (std::nothrow)
                                                      RegistryBuilder());
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL
prism_registry_builder_add_backend(PrismRegistryBuilder *builder,
                                   const char *PRISM_RESTRICT name,
                                   int priority, uint64_t features,
                                   const PrismBackendVTable *vtable,
                                   void *userdata,
                                   void(PRISM_CALL *userdata_free)(void *),
                                   PrismBackendId *out_id) {
  if (vtable->size == 0) {
    if (userdata_free != nullptr)
      userdata_free(userdata);
    return PRISM_ERROR_INVALID_PARAM;
  }
  auto factory = make_custom_factory(vtable, userdata, userdata_free, features,
                                     std::string{name});
  if (!factory) {
    if (userdata_free != nullptr)
      userdata_free(userdata);
    return PRISM_ERROR_INVALID_PARAM;
  }
  BackendId id{};
  const auto result = reinterpret_cast<RegistryBuilder *>(builder)->add(
      std::string{name}, priority, std::move(factory), &id);
  switch (result) {
  case BuilderResult::Ok: {
    if (out_id != nullptr)
      *out_id = to_prism_id(id);
    return PRISM_OK;
  }
  case BuilderResult::InvalidUtf8:
    return PRISM_ERROR_INVALID_UTF8;
  case BuilderResult::EmptyName:
  case BuilderResult::NegativePriority:
  case BuilderResult::ReservedId:
    return PRISM_ERROR_INVALID_PARAM;
  case BuilderResult::Spent:
  case BuilderResult::DuplicateName:
  case BuilderResult::DuplicateId:
    return PRISM_ERROR_INVALID_OPERATION;
  }
  return PRISM_ERROR_UNKNOWN;
}

PRISM_API PRISM_NODISCARD PrismRegistry *PRISM_CALL
prism_registry_freeze(PrismRegistryBuilder *builder) {
  return reinterpret_cast<PrismRegistry *>(
      reinterpret_cast<RegistryBuilder *>(builder)->freeze());
}

PRISM_API void PRISM_CALL
prism_registry_builder_free(PrismRegistryBuilder *builder) {
  delete reinterpret_cast<RegistryBuilder *>(builder);
}

PRISM_API PrismRegistry *PRISM_CALL
prism_registry_retain(PrismRegistry *registry) {
  if (registry != nullptr)
    reinterpret_cast<FrozenRegistry *>(registry)->retain();
  return registry;
}

PRISM_API void PRISM_CALL prism_registry_release(PrismRegistry *registry) {
  if (registry != nullptr)
    reinterpret_cast<FrozenRegistry *>(registry)->release();
}

PRISM_API void PRISM_CALL prism_backend_free(PrismBackend *backend) {
  if (backend == nullptr)
    return;
  delete backend;
}

PRISM_API PRISM_NODISCARD const char *PRISM_CALL
prism_backend_name(PrismBackend *backend) {
  return backend->impl->get_name().data();
}

PRISM_API PRISM_NODISCARD std::uint64_t PRISM_CALL
prism_backend_get_features(PrismBackend *backend) {
  return backend->impl->get_features().to_ullong();
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL
prism_backend_initialize(PrismBackend *backend) {
  const auto r = backend->impl->initialize();
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL prism_backend_speak(
    PrismBackend *backend, const char *PRISM_RESTRICT text, bool interrupt) {
  if (!simdutf::validate_utf8(text, std::string_view{text}.size()))
    return PRISM_ERROR_INVALID_UTF8;
  const auto r = backend->impl->speak(text, interrupt);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL prism_backend_speak_to_memory(
    PrismBackend *backend, const char *PRISM_RESTRICT text,
    PrismAudioCallback callback, void *userdata) {
  if (!simdutf::validate_utf8(text, std::string_view{text}.size()))
    return PRISM_ERROR_INVALID_UTF8;
  const auto r = backend->impl->speak_to_memory(
      text,
      [callback, userdata](void *, const float *samples, size_t count,
                           size_t ch, size_t sr) {
        callback(userdata, samples, count, ch, sr);
      },
      nullptr);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL
prism_backend_braille(PrismBackend *backend, const char *PRISM_RESTRICT text) {
  if (!simdutf::validate_utf8(text, std::string_view{text}.size()))
    return PRISM_ERROR_INVALID_UTF8;
  const auto r = backend->impl->braille(text);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL prism_backend_output(
    PrismBackend *backend, const char *PRISM_RESTRICT text, bool interrupt) {
  if (!simdutf::validate_utf8(text, std::string_view{text}.size()))
    return PRISM_ERROR_INVALID_UTF8;
  const auto r = backend->impl->output(text, interrupt);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL
prism_backend_stop(PrismBackend *backend) {
  const auto r = backend->impl->stop();
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL
prism_backend_pause(PrismBackend *backend) {
  const auto r = backend->impl->pause();
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL
prism_backend_resume(PrismBackend *backend) {
  const auto r = backend->impl->resume();
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL prism_backend_is_speaking(
    PrismBackend *backend, bool *PRISM_RESTRICT out_speaking) {
  const auto r = backend->impl->is_speaking();
  if (!r)
    return to_prism_error(r.error());
  *out_speaking = *r;
  return PRISM_OK;
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL
prism_backend_set_volume(PrismBackend *backend, float volume) {
  if (!std::isfinite(volume) || volume < 0.0F || volume > 1.0F)
    return PRISM_ERROR_RANGE_OUT_OF_BOUNDS;
  const auto r = backend->impl->set_volume(volume);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL
prism_backend_set_rate(PrismBackend *backend, float rate) {
  if (!std::isfinite(rate) || rate < 0.0F || rate > 1.0F)
    return PRISM_ERROR_RANGE_OUT_OF_BOUNDS;
  const auto r = backend->impl->set_rate(rate);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL
prism_backend_set_pitch(PrismBackend *backend, float pitch) {
  if (!std::isfinite(pitch) || pitch < 0.0F || pitch > 1.0F)
    return PRISM_ERROR_RANGE_OUT_OF_BOUNDS;
  const auto r = backend->impl->set_pitch(pitch);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL prism_backend_get_volume(
    PrismBackend *backend, float *PRISM_RESTRICT out_volume) {
  const auto r = backend->impl->get_volume();
  if (!r)
    return to_prism_error(r.error());
  *out_volume = *r;
  return PRISM_OK;
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL
prism_backend_get_rate(PrismBackend *backend, float *PRISM_RESTRICT out_rate) {
  const auto r = backend->impl->get_rate();
  if (!r)
    return to_prism_error(r.error());
  *out_rate = *r;
  return PRISM_OK;
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL prism_backend_get_pitch(
    PrismBackend *backend, float *PRISM_RESTRICT out_pitch) {
  const auto r = backend->impl->get_pitch();
  if (!r)
    return to_prism_error(r.error());
  *out_pitch = *r;
  return PRISM_OK;
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL
prism_backend_refresh_voices(PrismBackend *backend) {
  const auto r = backend->impl->refresh_voices();
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL prism_backend_count_voices(
    PrismBackend *backend, size_t *PRISM_RESTRICT out_count) {
  const auto r = backend->impl->count_voices();
  if (!r)
    return to_prism_error(r.error());
  *out_count = *r;
  return PRISM_OK;
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL
prism_backend_get_voice_name(PrismBackend *backend, size_t voice_id,
                             const char **PRISM_RESTRICT out_name) {
  auto r = backend->impl->get_voice_name(voice_id);
  if (!r)
    return to_prism_error(r.error());
  backend->voice_name = std::move(*r);
  *out_name = backend->voice_name.c_str();
  return PRISM_OK;
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL
prism_backend_get_voice_language(PrismBackend *backend, size_t voice_id,
                                 const char **PRISM_RESTRICT out_language) {
  auto r = backend->impl->get_voice_language(voice_id);
  if (!r)
    return to_prism_error(r.error());
  backend->voice_lang = std::move(*r);
  *out_language = backend->voice_lang.c_str();
  return PRISM_OK;
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL
prism_backend_set_voice(PrismBackend *backend, size_t voice_id) {
  const auto r = backend->impl->set_voice(voice_id);
  return r ? PRISM_OK : to_prism_error(r.error());
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL prism_backend_get_voice(
    PrismBackend *backend, size_t *PRISM_RESTRICT out_voice_id) {
  const auto r = backend->impl->get_voice();
  if (!r)
    return to_prism_error(r.error());
  *out_voice_id = *r;
  return PRISM_OK;
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL prism_backend_get_channels(
    PrismBackend *backend, size_t *PRISM_RESTRICT out_channels) {
  const auto r = backend->impl->get_channels();
  if (!r)
    return to_prism_error(r.error());
  *out_channels = *r;
  return PRISM_OK;
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL prism_backend_get_sample_rate(
    PrismBackend *backend, size_t *PRISM_RESTRICT out_sample_rate) {
  const auto r = backend->impl->get_sample_rate();
  if (!r)
    return to_prism_error(r.error());
  *out_sample_rate = *r;
  return PRISM_OK;
}

PRISM_API PRISM_NODISCARD PrismError PRISM_CALL prism_backend_get_bit_depth(
    PrismBackend *backend, size_t *PRISM_RESTRICT out_bit_depth) {
  const auto r = backend->impl->get_bit_depth();
  if (!r)
    return to_prism_error(r.error());
  *out_bit_depth = *r;
  return PRISM_OK;
}

PRISM_API PRISM_NODISCARD const char *PRISM_CALL
prism_error_string(PrismError error) {
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
                                        "Invalid audio format",
                                        "Internal backend limit exceeded",
                                        "Backend entered undefined state"};
  static_assert(std::size(strings) == PRISM_ERROR_COUNT,
                "Error string table size mismatches error count");
  if (static_cast<std::uint32_t>(error) >= PRISM_ERROR_COUNT)
    return "Unknown error";
  return strings[error];
}

PRISM_API PrismLogHandler PRISM_CALL
prism_set_log_handler(PrismLogHandler handler) {
  return logger().set_handler(handler);
}

PRISM_API PrismLogLevel PRISM_CALL prism_set_log_level(PrismLogLevel level) {
  return logger().set_level(level);
}

PRISM_API void PRISM_CALL prism_log(PrismLogLevel level, const char *source,
                                    const char *message) {
  Logger &lg = logger();
  if (!lg.wants(level))
    return;
  lg.submit(level, source, message);
}

PRISM_API void PRISM_CALL prism_log_flush(void) { logger().flush(); }

PRISM_API void PRISM_CALL prism_log_shutdown(void) { logger().shutdown(); }
}
