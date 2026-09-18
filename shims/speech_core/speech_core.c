// SPDX-License-Identifier: MPL-2.0
#define SPEECH_CORE_BUILDING
#include "../common/lock.h"
#include "../common/shim_common.h"
#include "SpeechCore.h"

#include <limits.h>
#include <prism.h>
#include <stdlib.h>
#include <string.h>

static fast_lock sc_lock = FAST_LOCK_INIT;
static PrismContext *sc_ctx;
static PrismBackend *sc_detected;
static PrismBackend *sc_native;
static PrismBackend *sc_active;
static bool sc_loaded;
static bool sc_prefer_native;
static _Thread_local wchar_t *sc_wide_result;

static void sc_set_wide_result(const char *text) {
  free(sc_wide_result);
  sc_wide_result =
      text == PRISM_SHIM_NULL ? PRISM_SHIM_NULL : shim_utf8_to_wchar(text);
}

static PrismBackend *sc_create_best(void) {
  PrismBackend *backend = prism_registry_create_best(sc_ctx);
  if (backend == PRISM_SHIM_NULL) {
    return PRISM_SHIM_NULL;
  }
  const PrismError error = prism_backend_initialize(backend);
  if (error != PRISM_OK && error != PRISM_ERROR_ALREADY_INITIALIZED) {
    prism_backend_free(backend);
    return PRISM_SHIM_NULL;
  }
  return backend;
}

static void sc_detect_locked(void) {
  if (sc_detected != PRISM_SHIM_NULL) {
    prism_backend_free(sc_detected);
    sc_detected = PRISM_SHIM_NULL;
  }
  sc_detected = sc_ctx == PRISM_SHIM_NULL ? PRISM_SHIM_NULL : sc_create_best();
  sc_active = sc_prefer_native && sc_native != PRISM_SHIM_NULL ? sc_native
                                                               : sc_detected;
  if (sc_active == PRISM_SHIM_NULL && sc_native != PRISM_SHIM_NULL) {
    sc_active = sc_native;
  }
}

static bool sc_speak_backend(PrismBackend *backend, const wchar_t *text,
                             bool interrupt) {
  if (backend == PRISM_SHIM_NULL || text == PRISM_SHIM_NULL) {
    return false;
  }
  char *utf8 = shim_wchar_to_utf8(text);
  if (utf8 == PRISM_SHIM_NULL) {
    return false;
  }
  const bool ok = prism_backend_speak(backend, utf8, interrupt) == PRISM_OK;
  free(utf8);
  return ok;
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
  if (features & PRISM_BACKEND_SUPPORTS_SPEAK_SSML)
    flags |= SC_SSML_SUPPORT;
  return flags;
}

void Speech_Init(void) {
  fast_lock_acquire(&sc_lock);
  if (!sc_loaded) {
    PrismConfig config = prism_config_init();
    sc_ctx = prism_init(&config);
    if (sc_ctx != PRISM_SHIM_NULL) {
      sc_native = shim_create_initialized(sc_ctx, shim_native_tts_id());
      sc_detect_locked();
      sc_loaded = sc_active != PRISM_SHIM_NULL;
    }
  }
  fast_lock_release(&sc_lock);
}

void Speech_Free(void) {
  fast_lock_acquire(&sc_lock);
  if (sc_detected != PRISM_SHIM_NULL)
    prism_backend_free(sc_detected);
  if (sc_native != PRISM_SHIM_NULL)
    prism_backend_free(sc_native);
  sc_detected = PRISM_SHIM_NULL;
  sc_native = PRISM_SHIM_NULL;
  sc_active = PRISM_SHIM_NULL;
  if (sc_ctx != PRISM_SHIM_NULL)
    prism_shutdown(sc_ctx);
  sc_ctx = PRISM_SHIM_NULL;
  sc_loaded = false;
  sc_prefer_native = false;
  fast_lock_release(&sc_lock);
}

void Speech_Detect_Driver(void) {
  fast_lock_acquire(&sc_lock);
  if (sc_ctx != PRISM_SHIM_NULL)
    sc_detect_locked();
  sc_loaded = sc_active != PRISM_SHIM_NULL;
  fast_lock_release(&sc_lock);
}
const wchar_t *Speech_Current_Driver(void) {
  fast_lock_acquire(&sc_lock);
  sc_set_wide_result(
      sc_active == PRISM_SHIM_NULL ? "" : prism_backend_name(sc_active));
  const wchar_t *result =
      sc_wide_result == PRISM_SHIM_NULL ? L"" : sc_wide_result;
  fast_lock_release(&sc_lock);
  return result;
}

const wchar_t *Speech_Get_Driver(int index) {
  fast_lock_acquire(&sc_lock);
  const char *name = PRISM_SHIM_NULL;
  if (sc_ctx != PRISM_SHIM_NULL && index >= 0 &&
      (size_t)index < prism_registry_count(sc_ctx)) {
    const PrismBackendId id = prism_registry_id_at(sc_ctx, (size_t)index);
    name = prism_registry_name(sc_ctx, id);
  }
  sc_set_wide_result(name == PRISM_SHIM_NULL ? "" : name);
  const wchar_t *result =
      sc_wide_result == PRISM_SHIM_NULL ? L"" : sc_wide_result;
  fast_lock_release(&sc_lock);
  return result;
}

void Speech_Set_Driver(int index) {
  fast_lock_acquire(&sc_lock);
  if (sc_ctx != PRISM_SHIM_NULL && index >= 0 &&
      (size_t)index < prism_registry_count(sc_ctx)) {
    const PrismBackendId id = prism_registry_id_at(sc_ctx, (size_t)index);
    PrismBackend *replacement = shim_create_initialized(sc_ctx, id);
    if (replacement != PRISM_SHIM_NULL) {
      if (sc_detected != PRISM_SHIM_NULL)
        prism_backend_free(sc_detected);
      sc_detected = replacement;
      sc_active = replacement;
      sc_prefer_native = false;
    }
  }
  fast_lock_release(&sc_lock);
}
int Speech_Get_Drivers(void) {
  fast_lock_acquire(&sc_lock);
  const int result =
      sc_ctx == PRISM_SHIM_NULL ? 0 : (int)prism_registry_count(sc_ctx);
  fast_lock_release(&sc_lock);
  return result;
}

uint32_t Speech_Get_Flags(void) {
  fast_lock_acquire(&sc_lock);
  const uint32_t result = sc_flags(sc_active);
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
  bool speaking = false;
  const bool ok =
      (sc_active != PRISM_SHIM_NULL &&
       prism_backend_is_speaking(sc_active, &speaking) == PRISM_OK) != 0;
  fast_lock_release(&sc_lock);
  return (ok && speaking) != 0;
}

bool Speech_Output(const wchar_t *text, bool interrupt) {
  fast_lock_acquire(&sc_lock);
  const bool result = sc_speak_backend(sc_active, text, interrupt);
  fast_lock_release(&sc_lock);
  return result;
}
bool Speech_Output_Text(const wchar_t *text, bool interrupt, bool with_ssml) {
  (void)with_ssml;
  return Speech_Output(text, interrupt);
}

bool Speech_Braille(const wchar_t *text) {
  fast_lock_acquire(&sc_lock);
  char *utf8 = shim_wchar_to_utf8(text);
  const bool result =
      (sc_active != PRISM_SHIM_NULL && utf8 != PRISM_SHIM_NULL &&
       prism_backend_braille(sc_active, utf8) == PRISM_OK) != 0;
  free(utf8);
  fast_lock_release(&sc_lock);
  return result;
}

bool Speech_Stop(void) {
  fast_lock_acquire(&sc_lock);
  const bool result = (sc_active != PRISM_SHIM_NULL &&
                       prism_backend_stop(sc_active) == PRISM_OK) != 0;
  fast_lock_release(&sc_lock);
  return result;
}

float Speech_Get_Volume(void) {
  fast_lock_acquire(&sc_lock);
  float result = -1.0F;
  if (sc_active != PRISM_SHIM_NULL)
    shim_ignore_error(prism_backend_get_volume(sc_active, &result));
  fast_lock_release(&sc_lock);
  return result;
}

void Speech_Set_Volume(float offset) {
  fast_lock_acquire(&sc_lock);
  if (sc_active != PRISM_SHIM_NULL)
    shim_ignore_error(
        prism_backend_set_volume(sc_active, shim_clamp01(offset)));
  fast_lock_release(&sc_lock);
}
float Speech_Get_Rate(void) {
  fast_lock_acquire(&sc_lock);
  float result = -1.0F;
  if (sc_active != PRISM_SHIM_NULL)
    shim_ignore_error(prism_backend_get_rate(sc_active, &result));
  fast_lock_release(&sc_lock);
  return result;
}

void Speech_Set_Rate(float offset) {
  fast_lock_acquire(&sc_lock);
  if (sc_active != PRISM_SHIM_NULL)
    shim_ignore_error(prism_backend_set_rate(sc_active, shim_clamp01(offset)));
  fast_lock_release(&sc_lock);
}

float Speech_Get_Pitch(void) {
  fast_lock_acquire(&sc_lock);
  float result = -1.0F;
  if (sc_active != PRISM_SHIM_NULL)
    shim_ignore_error(prism_backend_get_pitch(sc_active, &result));
  fast_lock_release(&sc_lock);
  return result;
}

void Speech_Set_Pitch(float offset) {
  fast_lock_acquire(&sc_lock);
  if (sc_active != PRISM_SHIM_NULL)
    shim_ignore_error(prism_backend_set_pitch(sc_active, shim_clamp01(offset)));
  fast_lock_release(&sc_lock);
}

int Speech_Get_Voices(void) {
  fast_lock_acquire(&sc_lock);
  size_t count = 0;
  if (sc_active != PRISM_SHIM_NULL)
    shim_ignore_error(prism_backend_count_voices(sc_active, &count));
  fast_lock_release(&sc_lock);
  return count > (size_t)INT_MAX ? INT_MAX : (int)count;
}
const wchar_t *Speech_Get_Voice(int index) {
  fast_lock_acquire(&sc_lock);
  const char *name = PRISM_SHIM_NULL;
  if (sc_active != PRISM_SHIM_NULL && index >= 0)
    shim_ignore_error(
        prism_backend_get_voice_name(sc_active, (size_t)index, &name));
  sc_set_wide_result(name);
  const wchar_t *result = sc_wide_result;
  fast_lock_release(&sc_lock);
  return result;
}

const wchar_t *Speech_Get_Current_Voice(void) {
  fast_lock_acquire(&sc_lock);
  size_t index = 0;
  const char *name = PRISM_SHIM_NULL;
  if (sc_active != PRISM_SHIM_NULL &&
      prism_backend_get_voice(sc_active, &index) == PRISM_OK)
    shim_ignore_error(prism_backend_get_voice_name(sc_active, index, &name));
  sc_set_wide_result(name);
  const wchar_t *result = sc_wide_result;
  fast_lock_release(&sc_lock);
  return result;
}

void Speech_Set_Voice(int index) {
  fast_lock_acquire(&sc_lock);
  if (sc_active != PRISM_SHIM_NULL && index >= 0)
    shim_ignore_error(prism_backend_set_voice(sc_active, (size_t)index));
  fast_lock_release(&sc_lock);
}

void Speech_Output_File(const char *path, const wchar_t *text) {
  fast_lock_acquire(&sc_lock);
  char *utf8 = shim_wchar_to_utf8(text);
  ShimAudioBuffer audio;
  shim_audio_buffer_init(&audio);
  if (sc_active != PRISM_SHIM_NULL && path != PRISM_SHIM_NULL &&
      utf8 != PRISM_SHIM_NULL && shim_synthesize_audio(sc_active, utf8, &audio))
    (void)shim_write_wav(path, &audio);
  shim_audio_buffer_clear(&audio);
  free(utf8);
  fast_lock_release(&sc_lock);
}

void Speech_Resume(void) {
  fast_lock_acquire(&sc_lock);
  if (sc_active != PRISM_SHIM_NULL)
    shim_ignore_error(prism_backend_resume(sc_active));
  fast_lock_release(&sc_lock);
}

void Speech_Pause(void) {
  fast_lock_acquire(&sc_lock);
  if (sc_active != PRISM_SHIM_NULL)
    shim_ignore_error(prism_backend_pause(sc_active));
  fast_lock_release(&sc_lock);
}

void Speech_Prefer_Sapi(bool prefer_sapi) {
  fast_lock_acquire(&sc_lock);
  sc_prefer_native = prefer_sapi;
  if (sc_loaded)
    sc_active =
        prefer_sapi && sc_native != PRISM_SHIM_NULL ? sc_native : sc_detected;
  if (sc_active == PRISM_SHIM_NULL && sc_native != PRISM_SHIM_NULL)
    sc_active = sc_native;
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
  if (sc_ctx != PRISM_SHIM_NULL && sc_native == PRISM_SHIM_NULL)
    sc_native = shim_create_initialized(sc_ctx, shim_native_tts_id());
  fast_lock_release(&sc_lock);
}

void Sapi_Release(void) {
  fast_lock_acquire(&sc_lock);
  if (sc_active == sc_native)
    sc_active = sc_detected;
  if (sc_native != PRISM_SHIM_NULL)
    prism_backend_free(sc_native);
  sc_native = PRISM_SHIM_NULL;
  fast_lock_release(&sc_lock);
}

static float sc_native_get(bool volume) {
  float value = -1.0F;
  if (sc_native != PRISM_SHIM_NULL) {
    const PrismError error = (int)volume
                                 ? prism_backend_get_volume(sc_native, &value)
                                 : prism_backend_get_rate(sc_native, &value);
    if (error != PRISM_OK)
      value = -1.0F;
  }
  return value;
}

float Sapi_Voice_Get_Volume(void) {
  fast_lock_acquire(&sc_lock);
  const float value = sc_native_get(true);
  fast_lock_release(&sc_lock);
  return value < 0.0F ? -1.0F : value * 100.0F;
}

void Sapi_Voice_Set_Volume(float volume) {
  fast_lock_acquire(&sc_lock);
  if (sc_native != PRISM_SHIM_NULL)
    shim_ignore_error(
        prism_backend_set_volume(sc_native, shim_clamp01(volume / 100.0F)));
  fast_lock_release(&sc_lock);
}

float Sapi_Voice_Get_Rate(void) {
  fast_lock_acquire(&sc_lock);
  const float value = sc_native_get(false);
  fast_lock_release(&sc_lock);
  return value < 0.0F ? -1.0F : (value * 20.0F) - 10.0F;
}

void Sapi_Voice_Set_Rate(float rate) {
  fast_lock_acquire(&sc_lock);
  if (sc_native != PRISM_SHIM_NULL)
    shim_ignore_error(prism_backend_set_rate(
        sc_native, shim_clamp01((rate + 10.0F) / 20.0F)));
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
  const char *name = PRISM_SHIM_NULL;
  if (sc_native != PRISM_SHIM_NULL && index >= 0)
    shim_ignore_error(
        prism_backend_get_voice_name(sc_native, (size_t)index, &name));
  sc_set_wide_result(name);
  const wchar_t *result = sc_wide_result;
  fast_lock_release(&sc_lock);
  return result;
}

const wchar_t *Sapi_Get_Current_Voice(void) {
  fast_lock_acquire(&sc_lock);
  size_t index = 0;
  const char *name = PRISM_SHIM_NULL;
  if (sc_native != PRISM_SHIM_NULL &&
      prism_backend_get_voice(sc_native, &index) == PRISM_OK)
    shim_ignore_error(prism_backend_get_voice_name(sc_native, index, &name));
  sc_set_wide_result(name);
  const wchar_t *result = sc_wide_result;
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
  fast_lock_acquire(&sc_lock);
  char *wanted = shim_wchar_to_utf8(voice);
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
  free(wanted);
  fast_lock_release(&sc_lock);
}

void Sapi_Speak(const wchar_t *text, bool interrupt, bool xml) {
  (void)xml;
  fast_lock_acquire(&sc_lock);
  (void)sc_speak_backend(sc_native, text, interrupt);
  fast_lock_release(&sc_lock);
}

void Sapi_Output_File(const char *filename, const wchar_t *text, bool _xml) {
  (void)_xml;
  fast_lock_acquire(&sc_lock);
  char *utf8 = shim_wchar_to_utf8(text);
  ShimAudioBuffer audio;
  shim_audio_buffer_init(&audio);
  if (sc_native != PRISM_SHIM_NULL && filename != PRISM_SHIM_NULL &&
      utf8 != PRISM_SHIM_NULL && shim_synthesize_audio(sc_native, utf8, &audio))
    (void)shim_write_wav(filename, &audio);
  shim_audio_buffer_clear(&audio);
  free(utf8);
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
