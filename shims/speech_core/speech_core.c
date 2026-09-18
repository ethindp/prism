// SPDX-License-Identifier: MPL-2.0
#define SPEECH_CORE_BUILDING
#include "../common/lock.h"
#include "../common/shim_common.h"
#include "SpeechCore.h"

#include <limits.h>
#include <prism.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define SC_VOLUME_MIN 0.0F
#define SC_VOLUME_MAX 100.0F
#define SC_RATE_MIN (-10.0F)
#define SC_RATE_MAX 10.0F
#else
#define SC_VOLUME_MIN 0.0F
#define SC_VOLUME_MAX 1.0F
#define SC_RATE_MIN 0.0F
#define SC_RATE_MAX 1.0F
#endif

typedef enum ScOp {
  SC_OP_SPEAK,
  SC_OP_OUTPUT,
  SC_OP_BRAILLE,
  SC_OP_STOP,
  SC_OP_PAUSE,
  SC_OP_RESUME
} ScOp;

static fast_lock sc_lock = FAST_LOCK_INIT;
static ShimFlag sc_stale;
static PrismContext *sc_ctx;
static ShimSelection sc_auto;
static PrismBackend *sc_native;
static ShimStrings sc_strings;
static bool sc_pinned;
static bool sc_loaded;
static bool sc_prefer_native;

static bool sc_ensure_context_locked(void) {
  if (sc_ctx == PRISM_SHIM_NULL) {
    sc_ctx = shim_context_open(&sc_stale);
  }
  return sc_ctx != PRISM_SHIM_NULL;
}

static void sc_release_context_if_idle_locked(void) {
  if (sc_loaded || sc_native != PRISM_SHIM_NULL || sc_ctx == PRISM_SHIM_NULL) {
    return;
  }
  prism_shutdown(sc_ctx);
  sc_ctx = PRISM_SHIM_NULL;
  (void)shim_flag_take(&sc_stale);
  shim_strings_free(&sc_strings);
}

static PrismBackend *sc_active_locked(void) {
  if (!sc_loaded) {
    return PRISM_SHIM_NULL;
  }
#ifdef _WIN32
  if (sc_prefer_native) {
    PrismBackend *native =
        shim_slot_get(sc_ctx, PRISM_BACKEND_SAPI, &sc_native);
    if (native != PRISM_SHIM_NULL) {
      return native;
    }
  }
#endif
  if (!sc_pinned && shim_flag_take(&sc_stale)) {
    shim_select(sc_ctx, &sc_auto, PRISM_SHIM_NULL, PRISM_SHIM_NULL);
  }
  if (sc_auto.backend != PRISM_SHIM_NULL) {
    return sc_auto.backend;
  }
#ifdef _WIN32
  return shim_slot_get(sc_ctx, PRISM_BACKEND_SAPI, &sc_native);
#else
  return PRISM_SHIM_NULL;
#endif
}

static PrismError sc_apply(PrismBackend *backend, ScOp op, const char *text,
                           bool interrupt) {
  if (backend == PRISM_SHIM_NULL) {
    return PRISM_ERROR_BACKEND_NOT_AVAILABLE;
  }
  switch (op) {
  case SC_OP_SPEAK:
    return prism_backend_speak(backend, text, interrupt);
  case SC_OP_OUTPUT:
    return prism_backend_output(backend, text, interrupt);
  case SC_OP_BRAILLE:
    return prism_backend_braille(backend, text);
  case SC_OP_STOP:
    return prism_backend_stop(backend);
  case SC_OP_PAUSE:
    return prism_backend_pause(backend);
  case SC_OP_RESUME:
    return prism_backend_resume(backend);
  }
  return PRISM_ERROR_INVALID_PARAM;
}

static bool sc_run(ScOp op, const wchar_t *text, bool interrupt) {
  char *utf8 = PRISM_SHIM_NULL;
  if (op == SC_OP_SPEAK || op == SC_OP_OUTPUT || op == SC_OP_BRAILLE) {
    if (text == PRISM_SHIM_NULL) {
      return false;
    }
    utf8 = shim_wchar_to_utf8(text);
    if (utf8 == PRISM_SHIM_NULL) {
      return false;
    }
  }
  fast_lock_acquire(&sc_lock);
  PrismBackend *backend = sc_active_locked();
  PrismError error = sc_apply(backend, op, utf8, interrupt);
  if (backend != PRISM_SHIM_NULL && shim_error_means_lost(error)) {
    if (backend == sc_native) {
      shim_slot_drop(&sc_native);
    } else {
      shim_selection_clear(&sc_auto);
      sc_pinned = false;
    }
    shim_flag_raise(&sc_stale);
    error = sc_apply(sc_active_locked(), op, utf8, interrupt);
  }
  fast_lock_release(&sc_lock);
  free(utf8);
  return error == PRISM_OK;
}

static uint32_t sc_flags(PrismBackend *backend) {
  if (backend == PRISM_SHIM_NULL) {
    return 0;
  }
  const uint64_t features = prism_backend_get_features(backend);
  uint32_t flags = 0;
  if (features & PRISM_BACKEND_SUPPORTS_SPEAK)
    flags |= SC_HAS_SPEECH;
  if (features & PRISM_BACKEND_SUPPORTS_BRAILLE)
    flags |= SC_HAS_BRAILLE;
  if (features & PRISM_BACKEND_SUPPORTS_IS_SPEAKING)
    flags |= SC_HAS_SPEECH_STATE;
  if (features & (PRISM_BACKEND_SUPPORTS_STOP | PRISM_BACKEND_SUPPORTS_PAUSE |
                  PRISM_BACKEND_SUPPORTS_RESUME))
    flags |= SC_SPEECH_FLOW_CONTROL;
  if (features &
      (PRISM_BACKEND_SUPPORTS_SET_VOLUME | PRISM_BACKEND_SUPPORTS_SET_RATE |
       PRISM_BACKEND_SUPPORTS_SET_PITCH))
    flags |= SC_SPEECH_PARAMETER_CONTROL;
  if (features &
      (PRISM_BACKEND_SUPPORTS_COUNT_VOICES | PRISM_BACKEND_SUPPORTS_SET_VOICE))
    flags |= SC_VOICE_CONFIG;
  if (features & PRISM_BACKEND_SUPPORTS_SPEAK_TO_MEMORY)
    flags |= SC_FILE_OUTPUT;
  return flags;
}

static const wchar_t *sc_voice_name_locked(PrismBackend *backend, int index) {
  const char *name = PRISM_SHIM_NULL;
  if (backend == PRISM_SHIM_NULL || index < 0 ||
      prism_backend_get_voice_name(backend, (size_t)index, &name) != PRISM_OK) {
    return PRISM_SHIM_NULL;
  }
  return shim_intern_wide(&sc_strings, name);
}

static const wchar_t *sc_current_voice_locked(PrismBackend *backend) {
  size_t index = 0;
  if (backend == PRISM_SHIM_NULL ||
      prism_backend_get_voice(backend, &index) != PRISM_OK ||
      index > (size_t)INT_MAX) {
    return PRISM_SHIM_NULL;
  }
  return sc_voice_name_locked(backend, (int)index);
}

static void sc_output_file_locked(PrismBackend *backend, const char *path,
                                  const wchar_t *text) {
  if (backend == PRISM_SHIM_NULL || path == PRISM_SHIM_NULL ||
      text == PRISM_SHIM_NULL) {
    return;
  }
  char *utf8 = shim_wchar_to_utf8(text);
  ShimAudioBuffer audio;
  shim_audio_buffer_init(&audio);
  if (utf8 != PRISM_SHIM_NULL && shim_synthesize_audio(backend, utf8, &audio)) {
    (void)shim_write_wav(path, &audio);
  }
  shim_audio_buffer_clear(&audio);
  free(utf8);
}

void Speech_Init(void) {
  fast_lock_acquire(&sc_lock);
  if (!sc_loaded && sc_ensure_context_locked()) {
    sc_loaded = true;
    shim_select(sc_ctx, &sc_auto, PRISM_SHIM_NULL, PRISM_SHIM_NULL);
#ifdef _WIN32
    (void)shim_slot_get(sc_ctx, PRISM_BACKEND_SAPI, &sc_native);
#endif
  }
  fast_lock_release(&sc_lock);
}

void Speech_Free(void) {
  fast_lock_acquire(&sc_lock);
  shim_selection_clear(&sc_auto);
  shim_slot_drop(&sc_native);
  sc_pinned = false;
  sc_loaded = false;
  sc_prefer_native = false;
  sc_release_context_if_idle_locked();
  fast_lock_release(&sc_lock);
}

void Speech_Detect_Driver(void) {
  fast_lock_acquire(&sc_lock);
  if (sc_loaded) {
    sc_pinned = false;
    shim_select(sc_ctx, &sc_auto, PRISM_SHIM_NULL, PRISM_SHIM_NULL);
  }
  fast_lock_release(&sc_lock);
}

const wchar_t *Speech_Current_Driver(void) {
  fast_lock_acquire(&sc_lock);
  PrismBackend *backend = sc_active_locked();
  const wchar_t *result =
      backend == PRISM_SHIM_NULL
          ? PRISM_SHIM_NULL
          : shim_intern_wide(&sc_strings, prism_backend_name(backend));
  fast_lock_release(&sc_lock);
  return result == PRISM_SHIM_NULL ? L"" : result;
}

const wchar_t *Speech_Get_Driver(int index) {
  fast_lock_acquire(&sc_lock);
  const wchar_t *result = PRISM_SHIM_NULL;
  if (sc_ctx != PRISM_SHIM_NULL && index >= 0 &&
      (size_t)index < prism_registry_count(sc_ctx)) {
    const PrismBackendId id = prism_registry_id_at(sc_ctx, (size_t)index);
    result = shim_intern_wide(&sc_strings, prism_registry_name(sc_ctx, id));
  }
  fast_lock_release(&sc_lock);
  return result == PRISM_SHIM_NULL ? L"" : result;
}

void Speech_Set_Driver(int index) {
  fast_lock_acquire(&sc_lock);
  if (sc_loaded && index >= 0 && (size_t)index < prism_registry_count(sc_ctx)) {
    const PrismBackendId id = prism_registry_id_at(sc_ctx, (size_t)index);
    PrismBackend *replacement = shim_create_live(sc_ctx, id);
    if (replacement != PRISM_SHIM_NULL) {
      shim_selection_clear(&sc_auto);
      sc_auto.backend = replacement;
      sc_auto.id = id;
      sc_pinned = true;
      sc_prefer_native = false;
    }
  }
  fast_lock_release(&sc_lock);
}

int Speech_Get_Drivers(void) {
  fast_lock_acquire(&sc_lock);
  const size_t count =
      sc_ctx == PRISM_SHIM_NULL ? 0 : prism_registry_count(sc_ctx);
  fast_lock_release(&sc_lock);
  return count > (size_t)INT_MAX ? INT_MAX : (int)count;
}

uint32_t Speech_Get_Flags(void) {
  fast_lock_acquire(&sc_lock);
  const uint32_t result = sc_flags(sc_active_locked());
  fast_lock_release(&sc_lock);
  return result;
}

bool Speech_Is_Loaded(void) {
  fast_lock_acquire(&sc_lock);
  const bool result = sc_loaded;
  fast_lock_release(&sc_lock);
  return result;
}

bool Speech_Is_Speaking(void) {
  fast_lock_acquire(&sc_lock);
  PrismBackend *backend = sc_active_locked();
  bool speaking = false;
  PrismError error = PRISM_ERROR_BACKEND_NOT_AVAILABLE;
  if (backend != PRISM_SHIM_NULL)
    error = prism_backend_is_speaking(backend, &speaking);
  fast_lock_release(&sc_lock);
  if (error != PRISM_OK)
    return false;
  return speaking;
}

bool Speech_Output(const wchar_t *text, bool interrupt) {
  return sc_run(SC_OP_SPEAK, text, interrupt);
}

bool Speech_Output_Text(const wchar_t *text, bool interrupt, bool with_ssml) {
  if (with_ssml) {
    return false;
  }
  return sc_run(SC_OP_OUTPUT, text, interrupt);
}

bool Speech_Braille(const wchar_t *text) {
  return sc_run(SC_OP_BRAILLE, text, false);
}

bool Speech_Stop(void) { return sc_run(SC_OP_STOP, PRISM_SHIM_NULL, false); }

float Speech_Get_Volume(void) {
  fast_lock_acquire(&sc_lock);
  PrismBackend *backend = sc_active_locked();
  float value = 0.0F;
  PrismError error = PRISM_ERROR_BACKEND_NOT_AVAILABLE;
  if (backend != PRISM_SHIM_NULL)
    error = prism_backend_get_volume(backend, &value);
  fast_lock_release(&sc_lock);
  if (error != PRISM_OK)
    return -1.0F;
  return shim_from_unit(value, SC_VOLUME_MIN, SC_VOLUME_MAX);
}

void Speech_Set_Volume(float offset) {
  fast_lock_acquire(&sc_lock);
  PrismBackend *backend = sc_active_locked();
  if (backend != PRISM_SHIM_NULL)
    shim_ignore_error(prism_backend_set_volume(
        backend, shim_to_unit(offset, SC_VOLUME_MIN, SC_VOLUME_MAX)));
  fast_lock_release(&sc_lock);
}

float Speech_Get_Rate(void) {
  fast_lock_acquire(&sc_lock);
  PrismBackend *backend = sc_active_locked();
  float value = 0.0F;
  PrismError error = PRISM_ERROR_BACKEND_NOT_AVAILABLE;
  if (backend != PRISM_SHIM_NULL)
    error = prism_backend_get_rate(backend, &value);
  fast_lock_release(&sc_lock);
  if (error != PRISM_OK)
    return -1.0F;
  return shim_from_unit(value, SC_RATE_MIN, SC_RATE_MAX);
}

void Speech_Set_Rate(float offset) {
  fast_lock_acquire(&sc_lock);
  PrismBackend *backend = sc_active_locked();
  if (backend != PRISM_SHIM_NULL)
    shim_ignore_error(prism_backend_set_rate(
        backend, shim_to_unit(offset, SC_RATE_MIN, SC_RATE_MAX)));
  fast_lock_release(&sc_lock);
}

float Speech_Get_Pitch(void) {
  fast_lock_acquire(&sc_lock);
  PrismBackend *backend = sc_active_locked();
  float value = 0.0F;
  PrismError error = PRISM_ERROR_BACKEND_NOT_AVAILABLE;
  if (backend != PRISM_SHIM_NULL)
    error = prism_backend_get_pitch(backend, &value);
  fast_lock_release(&sc_lock);
  if (error != PRISM_OK)
    return -1.0F;
  return value;
}

void Speech_Set_Pitch(float offset) {
  fast_lock_acquire(&sc_lock);
  PrismBackend *backend = sc_active_locked();
  if (backend != PRISM_SHIM_NULL)
    shim_ignore_error(prism_backend_set_pitch(backend, shim_clamp01(offset)));
  fast_lock_release(&sc_lock);
}

int Speech_Get_Voices(void) {
  fast_lock_acquire(&sc_lock);
  PrismBackend *backend = sc_active_locked();
  size_t count = 0;
  if (backend != PRISM_SHIM_NULL)
    shim_ignore_error(prism_backend_count_voices(backend, &count));
  fast_lock_release(&sc_lock);
  return count > (size_t)INT_MAX ? INT_MAX : (int)count;
}

const wchar_t *Speech_Get_Voice(int index) {
  fast_lock_acquire(&sc_lock);
  const wchar_t *result = sc_voice_name_locked(sc_active_locked(), index);
  fast_lock_release(&sc_lock);
  return result;
}

const wchar_t *Speech_Get_Current_Voice(void) {
  fast_lock_acquire(&sc_lock);
  const wchar_t *result = sc_current_voice_locked(sc_active_locked());
  fast_lock_release(&sc_lock);
  return result;
}

void Speech_Set_Voice(int index) {
  fast_lock_acquire(&sc_lock);
  PrismBackend *backend = sc_active_locked();
  if (backend != PRISM_SHIM_NULL && index >= 0)
    shim_ignore_error(prism_backend_set_voice(backend, (size_t)index));
  fast_lock_release(&sc_lock);
}

void Speech_Output_File(const char *path, const wchar_t *text) {
  fast_lock_acquire(&sc_lock);
  sc_output_file_locked(sc_active_locked(), path, text);
  fast_lock_release(&sc_lock);
}

void Speech_Resume(void) { (void)sc_run(SC_OP_RESUME, PRISM_SHIM_NULL, false); }

void Speech_Pause(void) { (void)sc_run(SC_OP_PAUSE, PRISM_SHIM_NULL, false); }

#ifdef _WIN32
void Speech_Prefer_Sapi(bool prefer_sapi) {
  fast_lock_acquire(&sc_lock);
  sc_prefer_native = prefer_sapi;
  fast_lock_release(&sc_lock);
}

bool Speech_Sapi_Loaded(void) {
  fast_lock_acquire(&sc_lock);
  const bool result = sc_native != PRISM_SHIM_NULL;
  fast_lock_release(&sc_lock);
  return result;
}

void Sapi_Init(void) {
  fast_lock_acquire(&sc_lock);
  if (sc_ensure_context_locked()) {
    (void)shim_slot_get(sc_ctx, PRISM_BACKEND_SAPI, &sc_native);
    sc_release_context_if_idle_locked();
  }
  fast_lock_release(&sc_lock);
}

void Sapi_Release(void) {
  fast_lock_acquire(&sc_lock);
  shim_slot_drop(&sc_native);
  sc_release_context_if_idle_locked();
  fast_lock_release(&sc_lock);
}

float Sapi_Voice_Get_Volume(void) {
  fast_lock_acquire(&sc_lock);
  float value = 0.0F;
  PrismError error = PRISM_ERROR_BACKEND_NOT_AVAILABLE;
  if (sc_native != PRISM_SHIM_NULL)
    error = prism_backend_get_volume(sc_native, &value);
  fast_lock_release(&sc_lock);
  if (error != PRISM_OK)
    return -1.0F;
  return shim_from_unit(value, 0.0F, 100.0F);
}

void Sapi_Voice_Set_Volume(float volume) {
  fast_lock_acquire(&sc_lock);
  if (sc_native != PRISM_SHIM_NULL)
    shim_ignore_error(prism_backend_set_volume(
        sc_native, shim_to_unit(volume, 0.0F, 100.0F)));
  fast_lock_release(&sc_lock);
}

float Sapi_Voice_Get_Rate(void) {
  fast_lock_acquire(&sc_lock);
  float value = 0.0F;
  PrismError error = PRISM_ERROR_BACKEND_NOT_AVAILABLE;
  if (sc_native != PRISM_SHIM_NULL)
    error = prism_backend_get_rate(sc_native, &value);
  fast_lock_release(&sc_lock);
  if (error != PRISM_OK)
    return -1.0F;
  return shim_from_unit(value, -10.0F, 10.0F);
}

void Sapi_Voice_Set_Rate(float rate) {
  fast_lock_acquire(&sc_lock);
  if (sc_native != PRISM_SHIM_NULL)
    shim_ignore_error(
        prism_backend_set_rate(sc_native, shim_to_unit(rate, -10.0F, 10.0F)));
  fast_lock_release(&sc_lock);
}

int Sapi_Get_Voices(void) {
  fast_lock_acquire(&sc_lock);
  size_t count = 0;
  if (sc_native != PRISM_SHIM_NULL)
    shim_ignore_error(prism_backend_count_voices(sc_native, &count));
  fast_lock_release(&sc_lock);
  return count > (size_t)INT_MAX ? INT_MAX : (int)count;
}

const wchar_t *Sapi_Get_Voice(int index) {
  fast_lock_acquire(&sc_lock);
  const wchar_t *result = sc_voice_name_locked(sc_native, index);
  fast_lock_release(&sc_lock);
  return result;
}

const wchar_t *Sapi_Get_Current_Voice(void) {
  fast_lock_acquire(&sc_lock);
  const wchar_t *result = sc_current_voice_locked(sc_native);
  fast_lock_release(&sc_lock);
  return result;
}

void Sapi_Set_Voice_By_Index(int index) {
  fast_lock_acquire(&sc_lock);
  if (sc_native != PRISM_SHIM_NULL && index >= 0)
    shim_ignore_error(prism_backend_set_voice(sc_native, (size_t)index));
  fast_lock_release(&sc_lock);
}

void Sapi_Set_Voice(const wchar_t *voice) {
  char *wanted = shim_wchar_to_utf8(voice);
  fast_lock_acquire(&sc_lock);
  size_t count = 0;
  if (sc_native != PRISM_SHIM_NULL && wanted != PRISM_SHIM_NULL &&
      prism_backend_count_voices(sc_native, &count) == PRISM_OK) {
    for (size_t i = 0; i < count; ++i) {
      const char *name = PRISM_SHIM_NULL;
      if (prism_backend_get_voice_name(sc_native, i, &name) == PRISM_OK &&
          name != PRISM_SHIM_NULL && strcmp(name, wanted) == 0) {
        shim_ignore_error(prism_backend_set_voice(sc_native, i));
        break;
      }
    }
  }
  fast_lock_release(&sc_lock);
  free(wanted);
}

void Sapi_Speak(const wchar_t *text, bool interrupt, bool xml) {
  if (xml || text == PRISM_SHIM_NULL) {
    return;
  }
  char *utf8 = shim_wchar_to_utf8(text);
  if (utf8 == PRISM_SHIM_NULL) {
    return;
  }
  fast_lock_acquire(&sc_lock);
  if (sc_native != PRISM_SHIM_NULL)
    shim_ignore_error(prism_backend_speak(sc_native, utf8, interrupt));
  fast_lock_release(&sc_lock);
  free(utf8);
}

void Sapi_Output_File(const char *filename, const wchar_t *text, bool xml) {
  if (xml) {
    return;
  }
  fast_lock_acquire(&sc_lock);
  sc_output_file_locked(sc_native, filename, text);
  fast_lock_release(&sc_lock);
}

void Sapi_Pause(void) {
  fast_lock_acquire(&sc_lock);
  if (sc_native != PRISM_SHIM_NULL)
    shim_ignore_error(prism_backend_pause(sc_native));
  fast_lock_release(&sc_lock);
}

void Sapi_Resume(void) {
  fast_lock_acquire(&sc_lock);
  if (sc_native != PRISM_SHIM_NULL)
    shim_ignore_error(prism_backend_resume(sc_native));
  fast_lock_release(&sc_lock);
}

void Sapi_Stop(void) {
  fast_lock_acquire(&sc_lock);
  if (sc_native != PRISM_SHIM_NULL)
    shim_ignore_error(prism_backend_stop(sc_native));
  fast_lock_release(&sc_lock);
}
#endif
