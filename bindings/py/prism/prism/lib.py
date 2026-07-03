import contextlib
import os
import sys
from pathlib import Path

from cffi import FFI

if sys.platform == "win32":
    _NATIVE_NAME = "prism.dll"
elif sys.platform == "darwin":
    _NATIVE_NAME = "libprism.dylib"
else:
    _NATIVE_NAME = "libprism.so"


def _find_native_dir() -> str:
    local_path = Path(__file__).parent / "_native"
    if local_path.exists() and any(local_path.iterdir()):
        return local_path
    relative_path = Path("prism") / "_native"
    for path in sys.path:
        if not path:
            continue
        candidate = Path(path) / relative_path
        if (candidate / _NATIVE_NAME).exists():
            return candidate.resolve()

    return local_path


dll_home = _find_native_dir()
with contextlib.suppress(AttributeError):
    os.add_dll_directory(str(dll_home))


ffi = FFI()
ffi.cdef(r"""// SPDX-License-Identifier: MPL-2.0

typedef struct PrismContext PrismContext;
typedef struct PrismBackend PrismBackend;
typedef uint64_t PrismBackendId;
typedef struct PrismRegistry PrismRegistry;
typedef struct PrismRegistryBuilder PrismRegistryBuilder;

typedef struct {
  uint8_t version;
  PrismRegistry *registry;
} PrismConfig;

typedef enum PrismError {
  PRISM_OK = 0,
  PRISM_ERROR_NOT_INITIALIZED,
  PRISM_ERROR_INVALID_PARAM,
  PRISM_ERROR_NOT_IMPLEMENTED,
  PRISM_ERROR_NO_VOICES,
  PRISM_ERROR_VOICE_NOT_FOUND,
  PRISM_ERROR_SPEAK_FAILURE,
  PRISM_ERROR_MEMORY_FAILURE,
  PRISM_ERROR_RANGE_OUT_OF_BOUNDS,
  PRISM_ERROR_INTERNAL,
  PRISM_ERROR_NOT_SPEAKING,
  PRISM_ERROR_NOT_PAUSED,
  PRISM_ERROR_ALREADY_PAUSED,
  PRISM_ERROR_INVALID_UTF8,
  PRISM_ERROR_INVALID_OPERATION,
  PRISM_ERROR_ALREADY_INITIALIZED,
  PRISM_ERROR_BACKEND_NOT_AVAILABLE,
  PRISM_ERROR_UNKNOWN,
  PRISM_ERROR_INVALID_AUDIO_FORMAT,
  PRISM_ERROR_INTERNAL_BACKEND_LIMIT_EXCEEDED,
  PRISM_ERROR_BACKEND_ENTERED_UNDEFINED_STATE,
  PRISM_ERROR_COUNT
} PrismError;

typedef void(PrismAudioCallback)(
    void *userdata, const float *samples, size_t sample_count,
    size_t channels, size_t sample_rate);

typedef struct PrismBackendVTable {
  size_t size;
  void *(*create)(void *instance);
  void (*destroy)(void *instance);
  bool (*is_supported)(void *instance);
  PrismError (*initialize)(void *instance);
  PrismError (*speak)(void *instance, const char *text, bool interrupt);
  PrismError (*speak_to_memory)(void *instance, const char *text,
                                PrismAudioCallback *callback, void *callback_userdata);
  PrismError (*braille)(void *instance, const char *text);
  PrismError (*output)(void *instance, const char *text, bool interrupt);
  PrismError (*stop)(void *instance);
  PrismError (*pause)(void *instance);
  PrismError (*resume)(void *instance);
  PrismError (*is_speaking)(void *instance, bool *out_speaking);
  PrismError (*set_volume)(void *instance, float volume);
  PrismError (*get_volume)(void *instance, float *out_volume);
  PrismError (*set_rate)(void *instance, float rate);
  PrismError (*get_rate)(void *instance, float *out_rate);
  PrismError (*set_pitch)(void *instance, float pitch);
  PrismError (*get_pitch)(void *instance, float *out_pitch);
  PrismError (*refresh_voices)(void *instance);
  PrismError (*count_voices)(void *instance, size_t *out_count);
  PrismError (*get_voice_name)(void *instance, size_t voice_id, const char **out_name);
  PrismError (*get_voice_language)(void *instance, size_t voice_id, const char **out_language);
  PrismError (*set_voice)(void *instance, size_t voice_id);
  PrismError (*get_voice)(void *instance, size_t *out_voice_id);
  PrismError (*get_channels)(void *instance, size_t *out_channels);
  PrismError (*get_sample_rate)(void *instance, size_t *out_sample_rate);
  PrismError (*get_bit_depth)(void *instance, size_t *out_bit_depth);
} PrismBackendVTable;

PrismConfig prism_config_init(void);
PrismContext *prism_init(PrismConfig* cfg);
void prism_shutdown(PrismContext *ctx);
size_t prism_registry_count(PrismContext *ctx);
PrismBackendId prism_registry_id_at(PrismContext *ctx, size_t index);
PrismBackendId prism_registry_id(PrismContext *ctx, const char *name);
const char *prism_registry_name(PrismContext *ctx, PrismBackendId id);
int prism_registry_priority(PrismContext *ctx, PrismBackendId id);
bool prism_registry_exists(PrismContext *ctx, PrismBackendId id);
PrismBackend *prism_registry_get(PrismContext *ctx, PrismBackendId id);
PrismBackend *prism_registry_create(PrismContext *ctx, PrismBackendId id);
PrismBackend *prism_registry_create_best(PrismContext *ctx);
PrismBackend *prism_registry_acquire(PrismContext *ctx, PrismBackendId id);
PrismBackend *prism_registry_acquire_best(PrismContext *ctx);
PrismRegistryBuilder *prism_registry_builder_new(void);
PrismError prism_registry_builder_add_backend(
    PrismRegistryBuilder *builder, const char *name, int priority,
    uint64_t features, const PrismBackendVTable *vtable, void *userdata,
    void (*userdata_free)(void *), PrismBackendId *out_id);
PrismRegistry *prism_registry_freeze(PrismRegistryBuilder *builder);
void prism_registry_builder_free(PrismRegistryBuilder *builder);
PrismRegistry *prism_registry_retain(PrismRegistry *registry);
void prism_registry_release(PrismRegistry *registry);
void prism_backend_free(PrismBackend *backend);
uint64_t prism_backend_get_features(PrismBackend *backend);
const char *prism_backend_name(PrismBackend *backend);
PrismError prism_backend_initialize(PrismBackend *backend);
PrismError prism_backend_speak(PrismBackend *backend, const char *text, bool interrupt);
PrismError prism_backend_speak_to_memory(
    PrismBackend *backend,
    const char *text,
    PrismAudioCallback callback,
    void *userdata
);
PrismError prism_backend_braille(PrismBackend *backend, const char *text);
PrismError prism_backend_output(PrismBackend *backend, const char *text, bool interrupt);
PrismError prism_backend_stop(PrismBackend *backend);
PrismError prism_backend_pause(PrismBackend *backend);
PrismError prism_backend_resume(PrismBackend *backend);
PrismError prism_backend_is_speaking(PrismBackend *backend, bool *out_speaking);
PrismError prism_backend_set_volume(PrismBackend *backend, float volume);
PrismError prism_backend_get_volume(PrismBackend *backend, float *out_volume);
PrismError prism_backend_set_rate(PrismBackend *backend, float rate);
PrismError prism_backend_get_rate(PrismBackend *backend, float *out_rate);
PrismError prism_backend_set_pitch(PrismBackend *backend, float pitch);
PrismError prism_backend_get_pitch(PrismBackend *backend, float *out_pitch);
PrismError prism_backend_refresh_voices(PrismBackend *backend);
PrismError prism_backend_count_voices(PrismBackend *backend, size_t *out_count);
PrismError prism_backend_get_voice_name(
    PrismBackend *backend,
    size_t voice_id,
    const char **out_name
);
PrismError prism_backend_get_voice_language(
    PrismBackend *backend,
    size_t voice_id,
    const char **out_language
);
PrismError prism_backend_set_voice(PrismBackend *backend, size_t voice_id);
PrismError prism_backend_get_voice(PrismBackend *backend, size_t *out_voice_id);
PrismError prism_backend_get_channels(PrismBackend *backend, size_t *out_channels);
PrismError prism_backend_get_sample_rate(PrismBackend *backend, size_t *out_sample_rate);
PrismError prism_backend_get_bit_depth(PrismBackend *backend, size_t *out_bit_depth);
const char *prism_error_string(PrismError error);
""")
lib_path = (dll_home / _NATIVE_NAME).resolve()
lib = ffi.dlopen(
    str(lib_path), ffi.RTLD_NOW | ffi.RTLD_DEEPBIND if sys.platform == "linux" else 0
)
