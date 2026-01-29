import contextlib
import importlib
import os
import sys
from pathlib import Path

from cffi import FFI


def _find_native_dir() -> str:
    local_path = Path(__file__).parent / "_native"
    if local_path.exists() and any(local_path.iterdir()):
        return local_path
    relative_path = Path("prism") / "_native"
    for path in sys.path:
        if not path:
            continue
        candidate = Path(path) / relative_path
        if (candidate / "prism.dll").exists():
            return candidate.resolve()

    return local_path


dll_home = _find_native_dir()
with contextlib.suppress(AttributeError):
    os.add_dll_directory(str(dll_home))


def _is_android() -> bool:
    with contextlib.suppress(ImportError):
        if importlib.util.find_spec("", "android") is not None:
            return True
    if sys.platform == "linux":
        with contextlib.suppress(FileNotFoundError), Path("/system/build.prop").open():
            return True
    return False


ffi = FFI()
if sys.platform == "win32" or _is_android():
    ffi.cdef(r"""// SPDX-License-Identifier: MPL-2.0

typedef struct PrismContext PrismContext;
typedef struct PrismBackend PrismBackend;
typedef uint64_t PrismBackendId;
typedef struct {
  uint8_t version;
  void* platform_data;
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
  PRISM_ERROR_COUNT
} PrismError;

typedef void(PrismAudioCallback)(
    void *userdata, const float *samples, size_t sample_count,
    size_t channels, size_t sample_rate);

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
else:
    ffi.cdef(r"""// SPDX-License-Identifier: MPL-2.0

typedef struct PrismContext PrismContext;
typedef struct PrismBackend PrismBackend;
typedef uint64_t PrismBackendId;
typedef struct {
  uint8_t version;
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
  PRISM_ERROR_COUNT
} PrismError;

typedef void(PrismAudioCallback)(
    void *userdata, const float *samples, size_t sample_count,
    size_t channels, size_t sample_rate);

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
if sys.platform == "win32":
    lib_path = (dll_home / "prism.dll").resolve()
elif sys.platform == "darwin":
    lib_path = (dll_home / "libprism.dylib").resolve()
else:
    lib_path = (dll_home / "libprism.so").resolve()
lib = ffi.dlopen(str(lib_path))
