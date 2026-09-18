// SPDX-License-Identifier: MPL-2.0
#define SRAL_EXPORT
#include "SRAL.h"
#include "../common/lock.h"
#include "../common/shim_common.h"

#include <limits.h>
#include <prism.h>
#include <stdlib.h>
#include <string.h>

typedef struct SralSlot {
  PrismBackendId prism_id;
  PrismBackend *backend;
  int legacy_id;
  SRAL_EngineCategory category;
  bool auto_eligible;
  float rate_min;
  float rate_max;
  float volume_min;
  float volume_max;
} SralSlot;

typedef enum SralOp {
  SRAL_OP_SPEAK,
  SRAL_OP_BRAILLE,
  SRAL_OP_OUTPUT,
  SRAL_OP_STOP,
  SRAL_OP_PAUSE,
  SRAL_OP_RESUME
} SralOp;

static SralSlot sral_slots[] = {
    {PRISM_BACKEND_NVDA, PRISM_SHIM_NULL, SRAL_ENGINE_NVDA,
     SRAL_ENGINE_CATEGORY_SCREEN_READER, true, -100.0F, 100.0F, 0.0F, 100.0F},
    {PRISM_BACKEND_JAWS, PRISM_SHIM_NULL, SRAL_ENGINE_JAWS,
     SRAL_ENGINE_CATEGORY_SCREEN_READER, true, -100.0F, 100.0F, 0.0F, 100.0F},
    {PRISM_BACKEND_ZDSR, PRISM_SHIM_NULL, SRAL_ENGINE_ZDSR,
     SRAL_ENGINE_CATEGORY_SCREEN_READER, true, -100.0F, 100.0F, 0.0F, 100.0F},
    {PRISM_BACKEND_INVALID, PRISM_SHIM_NULL, SRAL_ENGINE_NARRATOR,
     SRAL_ENGINE_CATEGORY_SCREEN_READER, false, -100.0F, 100.0F, 0.0F,
     100.0F},
    {PRISM_BACKEND_UIA, PRISM_SHIM_NULL, SRAL_ENGINE_UIA,
     SRAL_ENGINE_CATEGORY_ACCESSIBILITY_PROVIDER, true, -100.0F, 100.0F, 0.0F,
     100.0F},
    {PRISM_BACKEND_SAPI, PRISM_SHIM_NULL, SRAL_ENGINE_SAPI,
     SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE, true, -10.0F, 10.0F, 0.0F,
     100.0F},
    {PRISM_BACKEND_SPEECH_DISPATCHER, PRISM_SHIM_NULL,
     SRAL_ENGINE_SPEECH_DISPATCHER, SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE,
     true, -100.0F, 100.0F, -100.0F, 100.0F},
    {PRISM_BACKEND_VOICE_OVER, PRISM_SHIM_NULL, SRAL_ENGINE_VOICE_OVER,
     SRAL_ENGINE_CATEGORY_SCREEN_READER, true, -100.0F, 100.0F, 0.0F, 100.0F},
    {PRISM_BACKEND_INVALID, PRISM_SHIM_NULL, SRAL_ENGINE_NS_SPEECH,
     SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE, false, -100.0F, 100.0F, 0.0F,
     100.0F},
    {PRISM_BACKEND_AV_SPEECH, PRISM_SHIM_NULL, SRAL_ENGINE_AV_SPEECH,
     SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE, true, -100.0F, 100.0F, 0.0F,
     100.0F},
    {PRISM_BACKEND_ANDROID_SCREEN_READER, PRISM_SHIM_NULL,
     SRAL_ENGINE_ANDROID_ACCESSIBILITY_MANAGER,
     SRAL_ENGINE_CATEGORY_ACCESSIBILITY_PROVIDER, true, -100.0F, 100.0F, 0.0F,
     100.0F},
    {PRISM_BACKEND_ANDROID_TTS, PRISM_SHIM_NULL,
     SRAL_ENGINE_ANDROID_TEXT_TO_SPEECH,
     SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE, true, -100.0F, 100.0F, 0.0F,
     100.0F},
};

enum { SRAL_SLOT_COUNT = (int)(sizeof(sral_slots) / sizeof(sral_slots[0])) };

static fast_lock sral_lock = FAST_LOCK_INIT;
static ShimFlag sral_stale;
static PrismContext *sral_ctx;
static SralSlot *sral_current;
static ShimStrings sral_strings;
static int sral_excluded;
static bool sral_initialized;

static SralSlot *sral_slot(int engine) {
  for (int i = 0; i < SRAL_SLOT_COUNT; ++i) {
    if (sral_slots[i].legacy_id == engine) {
      return sral_slots[i].prism_id == PRISM_BACKEND_INVALID
                 ? PRISM_SHIM_NULL
                 : &sral_slots[i];
    }
  }
  return PRISM_SHIM_NULL;
}

static void sral_select_locked(void) {
  sral_current = PRISM_SHIM_NULL;
  if (sral_ctx == PRISM_SHIM_NULL) {
    return;
  }
  for (int i = 0; i < SRAL_SLOT_COUNT; ++i) {
    SralSlot *slot = &sral_slots[i];
    if (slot->prism_id != PRISM_BACKEND_INVALID && slot->auto_eligible &&
        (sral_excluded & slot->legacy_id) == 0 &&
        shim_slot_live(sral_ctx, slot->prism_id, &slot->backend)) {
      sral_current = slot;
      return;
    }
  }
}

static SralSlot *sral_resolve_locked(int engine) {
  if (engine != SRAL_ENGINE_NONE) {
    return sral_slot(engine);
  }
  if (shim_flag_take(&sral_stale)) {
    sral_select_locked();
  }
  return sral_current;
}

static PrismBackend *sral_backend_locked(SralSlot *slot) {
  if (slot == PRISM_SHIM_NULL || sral_ctx == PRISM_SHIM_NULL) {
    return PRISM_SHIM_NULL;
  }
  return shim_slot_get(sral_ctx, slot->prism_id, &slot->backend);
}

static PrismError sral_apply(PrismBackend *backend, SralOp op,
                             const char *text, bool interrupt) {
  if (backend == PRISM_SHIM_NULL) {
    return PRISM_ERROR_BACKEND_NOT_AVAILABLE;
  }
  switch (op) {
  case SRAL_OP_SPEAK:
    return prism_backend_speak(backend, text, interrupt);
  case SRAL_OP_BRAILLE:
    return prism_backend_braille(backend, text);
  case SRAL_OP_OUTPUT:
    return prism_backend_output(backend, text, interrupt);
  case SRAL_OP_STOP:
    return prism_backend_stop(backend);
  case SRAL_OP_PAUSE:
    return prism_backend_pause(backend);
  case SRAL_OP_RESUME:
    return prism_backend_resume(backend);
  }
  return PRISM_ERROR_INVALID_PARAM;
}

static bool sral_run(int engine, SralOp op, const char *text, bool interrupt) {
  const bool needs_text =
      op == SRAL_OP_SPEAK || op == SRAL_OP_BRAILLE || op == SRAL_OP_OUTPUT;
  if (needs_text && text == PRISM_SHIM_NULL) {
    return false;
  }
  fast_lock_acquire(&sral_lock);
  PrismError error = PRISM_ERROR_NOT_INITIALIZED;
  if (sral_initialized) {
    SralSlot *slot = sral_resolve_locked(engine);
    PrismBackend *backend = sral_backend_locked(slot);
    error = sral_apply(backend, op, text, interrupt);
    if (backend != PRISM_SHIM_NULL && shim_error_means_lost(error)) {
      shim_slot_drop(&slot->backend);
      if (engine == SRAL_ENGINE_NONE) {
        shim_flag_raise(&sral_stale);
        slot = sral_resolve_locked(engine);
      }
      error = sral_apply(sral_backend_locked(slot), op, text, interrupt);
    }
  }
  fast_lock_release(&sral_lock);
  return error == PRISM_OK;
}

static int sral_features_locked(SralSlot *slot) {
  if (slot == PRISM_SHIM_NULL || sral_ctx == PRISM_SHIM_NULL ||
      !prism_registry_exists(sral_ctx, slot->prism_id)) {
    return -1;
  }
  PrismBackend *probe = slot->backend;
  if (probe == PRISM_SHIM_NULL) {
    probe = prism_registry_create(sral_ctx, slot->prism_id);
    if (probe == PRISM_SHIM_NULL) {
      return -1;
    }
  }
  const uint64_t prism = prism_backend_get_features(probe);
  if (probe != slot->backend) {
    prism_backend_free(probe);
  }
  int result = 0;
  if (prism & PRISM_BACKEND_SUPPORTS_SPEAK)
    result |= SRAL_SUPPORTS_SPEECH;
  if (prism & PRISM_BACKEND_SUPPORTS_BRAILLE)
    result |= SRAL_SUPPORTS_BRAILLE;
  if (prism &
      (PRISM_BACKEND_SUPPORTS_SET_RATE | PRISM_BACKEND_SUPPORTS_GET_RATE))
    result |= SRAL_SUPPORTS_SPEECH_RATE;
  if (prism &
      (PRISM_BACKEND_SUPPORTS_SET_VOLUME | PRISM_BACKEND_SUPPORTS_GET_VOLUME))
    result |= SRAL_SUPPORTS_SPEECH_VOLUME;
  if (prism &
      (PRISM_BACKEND_SUPPORTS_SET_VOICE | PRISM_BACKEND_SUPPORTS_GET_VOICE))
    result |= SRAL_SUPPORTS_SELECT_VOICE;
  if (prism & (PRISM_BACKEND_SUPPORTS_PAUSE | PRISM_BACKEND_SUPPORTS_RESUME))
    result |= SRAL_SUPPORTS_PAUSE_SPEECH;
  if (prism & PRISM_BACKEND_SUPPORTS_SPEAK_TO_MEMORY)
    result |= SRAL_SUPPORTS_SPEAK_TO_MEMORY;
  return result;
}

static int sral_to_int(float unit, float min, float max) {
  const float value = shim_from_unit(unit, min, max);
  return (int)(value >= 0.0F ? value + 0.5F : value - 0.5F);
}

void *SRAL_malloc(size_t size) { return malloc(size); }
void SRAL_free(void *memory) { free(memory); }

bool SRAL_Initialize(int engines_exclude) {
  fast_lock_acquire(&sral_lock);
  if (sral_initialized) {
    fast_lock_release(&sral_lock);
    return true;
  }
  sral_ctx = shim_context_open(&sral_stale);
  if (sral_ctx == PRISM_SHIM_NULL) {
    fast_lock_release(&sral_lock);
    return false;
  }
  bool any = false;
  for (int i = 0; i < SRAL_SLOT_COUNT; ++i) {
    SralSlot *slot = &sral_slots[i];
    if (slot->prism_id != PRISM_BACKEND_INVALID &&
        shim_slot_live(sral_ctx, slot->prism_id, &slot->backend)) {
      any = true;
    }
  }
  if (!any) {
    prism_shutdown(sral_ctx);
    sral_ctx = PRISM_SHIM_NULL;
    (void)shim_flag_take(&sral_stale);
    fast_lock_release(&sral_lock);
    return false;
  }
  sral_excluded = engines_exclude;
  sral_initialized = true;
  sral_select_locked();
  fast_lock_release(&sral_lock);
  return true;
}

void SRAL_Uninitialize(void) {
  fast_lock_acquire(&sral_lock);
  for (int i = 0; i < SRAL_SLOT_COUNT; ++i) {
    shim_slot_drop(&sral_slots[i].backend);
  }
  sral_current = PRISM_SHIM_NULL;
  if (sral_ctx != PRISM_SHIM_NULL) {
    prism_shutdown(sral_ctx);
  }
  sral_ctx = PRISM_SHIM_NULL;
  (void)shim_flag_take(&sral_stale);
  shim_strings_free(&sral_strings);
  sral_excluded = SRAL_ENGINE_NONE;
  sral_initialized = false;
  fast_lock_release(&sral_lock);
}

bool SRAL_IsInitialized(void) {
  fast_lock_acquire(&sral_lock);
  const bool result = sral_initialized;
  fast_lock_release(&sral_lock);
  return result;
}

bool SRAL_SpeakEx(int engine, const char *text, bool interrupt) {
  return sral_run(engine, SRAL_OP_SPEAK, text, interrupt);
}

bool SRAL_Speak(const char *text, bool interrupt) {
  return sral_run(SRAL_ENGINE_NONE, SRAL_OP_SPEAK, text, interrupt);
}

bool SRAL_SpeakSsmlEx(int engine, const char *ssml, bool interrupt) {
  (void)engine;
  (void)ssml;
  (void)interrupt;
  return false;
}

bool SRAL_SpeakSsml(const char *ssml, bool interrupt) {
  (void)ssml;
  (void)interrupt;
  return false;
}

void *SRAL_SpeakToMemoryEx(int engine, const char *text, uint64_t *buffer_size,
                           int *channels, int *sample_rate,
                           int *bits_per_sample) {
  fast_lock_acquire(&sral_lock);
  PrismBackend *backend = PRISM_SHIM_NULL;
  if (sral_initialized) {
    backend = sral_backend_locked(sral_resolve_locked(engine));
  }
  ShimAudioBuffer audio;
  shim_audio_buffer_init(&audio);
  void *result = PRISM_SHIM_NULL;
  if (backend != PRISM_SHIM_NULL && text != PRISM_SHIM_NULL &&
      shim_synthesize_audio(backend, text, &audio) &&
      audio.channels <= (size_t)INT_MAX && audio.sample_rate <= (size_t)INT_MAX) {
    if (buffer_size != PRISM_SHIM_NULL)
      *buffer_size = (uint64_t)(audio.sample_count * sizeof(float));
    if (channels != PRISM_SHIM_NULL)
      *channels = (int)audio.channels;
    if (sample_rate != PRISM_SHIM_NULL)
      *sample_rate = (int)audio.sample_rate;
    if (bits_per_sample != PRISM_SHIM_NULL)
      *bits_per_sample = 32;
    result = audio.samples;
    audio.samples = PRISM_SHIM_NULL;
  } else {
    if (buffer_size != PRISM_SHIM_NULL)
      *buffer_size = 0;
    if (channels != PRISM_SHIM_NULL)
      *channels = 0;
    if (sample_rate != PRISM_SHIM_NULL)
      *sample_rate = 0;
    if (bits_per_sample != PRISM_SHIM_NULL)
      *bits_per_sample = 0;
  }
  shim_audio_buffer_clear(&audio);
  fast_lock_release(&sral_lock);
  return result;
}

void *SRAL_SpeakToMemory(const char *text, uint64_t *buffer_size, int *channels,
                         int *sample_rate, int *bits_per_sample) {
  return SRAL_SpeakToMemoryEx(SRAL_ENGINE_NONE, text, buffer_size, channels,
                              sample_rate, bits_per_sample);
}

bool SRAL_BrailleEx(int engine, const char *text) {
  return sral_run(engine, SRAL_OP_BRAILLE, text, false);
}

bool SRAL_Braille(const char *text) {
  return sral_run(SRAL_ENGINE_NONE, SRAL_OP_BRAILLE, text, false);
}

bool SRAL_OutputEx(int engine, const char *text, bool interrupt) {
  return sral_run(engine, SRAL_OP_OUTPUT, text, interrupt);
}

bool SRAL_Output(const char *text, bool interrupt) {
  return sral_run(SRAL_ENGINE_NONE, SRAL_OP_OUTPUT, text, interrupt);
}

bool SRAL_StopSpeechEx(int engine) {
  return sral_run(engine, SRAL_OP_STOP, PRISM_SHIM_NULL, false);
}

bool SRAL_StopSpeech(void) {
  return sral_run(SRAL_ENGINE_NONE, SRAL_OP_STOP, PRISM_SHIM_NULL, false);
}

bool SRAL_PauseSpeechEx(int engine) {
  return sral_run(engine, SRAL_OP_PAUSE, PRISM_SHIM_NULL, false);
}

bool SRAL_PauseSpeech(void) {
  return sral_run(SRAL_ENGINE_NONE, SRAL_OP_PAUSE, PRISM_SHIM_NULL, false);
}

bool SRAL_ResumeSpeechEx(int engine) {
  return sral_run(engine, SRAL_OP_RESUME, PRISM_SHIM_NULL, false);
}

bool SRAL_ResumeSpeech(void) {
  return sral_run(SRAL_ENGINE_NONE, SRAL_OP_RESUME, PRISM_SHIM_NULL, false);
}

bool SRAL_IsSpeakingEx(int engine) {
  fast_lock_acquire(&sral_lock);
  bool speaking = false;
  PrismError error = PRISM_ERROR_NOT_INITIALIZED;
  if (sral_initialized) {
    PrismBackend *backend = sral_backend_locked(sral_resolve_locked(engine));
    if (backend != PRISM_SHIM_NULL) {
      error = prism_backend_is_speaking(backend, &speaking);
    }
  }
  fast_lock_release(&sral_lock);
  return error == PRISM_OK && speaking;
}

bool SRAL_IsSpeaking(void) { return SRAL_IsSpeakingEx(SRAL_ENGINE_NONE); }

int SRAL_GetCurrentEngine(void) {
  fast_lock_acquire(&sral_lock);
  const SralSlot *slot = sral_initialized
                             ? sral_resolve_locked(SRAL_ENGINE_NONE)
                             : PRISM_SHIM_NULL;
  const int result =
      slot == PRISM_SHIM_NULL ? SRAL_ENGINE_NONE : slot->legacy_id;
  fast_lock_release(&sral_lock);
  return result;
}

int SRAL_GetEngineFeatures(int engine) {
  fast_lock_acquire(&sral_lock);
  const int result = sral_initialized
                         ? sral_features_locked(sral_resolve_locked(engine))
                         : -1;
  fast_lock_release(&sral_lock);
  return result;
}

static bool sral_set_parameter_locked(SralSlot *slot, int param,
                                      const void *value) {
  PrismBackend *backend = sral_backend_locked(slot);
  if (backend == PRISM_SHIM_NULL || value == PRISM_SHIM_NULL)
    return false;
  switch (param) {
  case SRAL_PARAM_SPEECH_RATE:
    return prism_backend_set_rate(backend,
                                  shim_to_unit((float)*(const int *)value,
                                               slot->rate_min,
                                               slot->rate_max)) == PRISM_OK;
  case SRAL_PARAM_SPEECH_VOLUME:
    return prism_backend_set_volume(backend,
                                    shim_to_unit((float)*(const int *)value,
                                                 slot->volume_min,
                                                 slot->volume_max)) == PRISM_OK;
  case SRAL_PARAM_VOICE_INDEX: {
    const int index = *(const int *)value;
    return index >= 0 &&
           prism_backend_set_voice(backend, (size_t)index) == PRISM_OK;
  }
  default:
    return false;
  }
}

bool SRAL_SetEngineParameter(int engine, int param, const void *value) {
  fast_lock_acquire(&sral_lock);
  const bool result =
      sral_initialized &&
      sral_set_parameter_locked(sral_resolve_locked(engine), param, value);
  fast_lock_release(&sral_lock);
  return result;
}

static bool sral_get_parameter_locked(SralSlot *slot, int param, void *value) {
  PrismBackend *backend = sral_backend_locked(slot);
  if (backend == PRISM_SHIM_NULL || value == PRISM_SHIM_NULL)
    return false;
  switch (param) {
  case SRAL_PARAM_SPEECH_RATE: {
    float rate = 0.0F;
    if (prism_backend_get_rate(backend, &rate) != PRISM_OK)
      return false;
    *(int *)value = sral_to_int(rate, slot->rate_min, slot->rate_max);
    return true;
  }
  case SRAL_PARAM_SPEECH_VOLUME: {
    float volume = 0.0F;
    if (prism_backend_get_volume(backend, &volume) != PRISM_OK)
      return false;
    *(int *)value = sral_to_int(volume, slot->volume_min, slot->volume_max);
    return true;
  }
  case SRAL_PARAM_VOICE_INDEX: {
    size_t index = 0;
    if (prism_backend_get_voice(backend, &index) != PRISM_OK ||
        index > (size_t)INT_MAX)
      return false;
    *(int *)value = (int)index;
    return true;
  }
  case SRAL_PARAM_VOICE_COUNT: {
    size_t count = 0;
    if (prism_backend_count_voices(backend, &count) != PRISM_OK)
      return false;
    *(int *)value = count > (size_t)INT_MAX ? INT_MAX : (int)count;
    return true;
  }
  case SRAL_PARAM_VOICE_PROPERTIES: {
    size_t count = 0;
    if (prism_backend_count_voices(backend, &count) != PRISM_OK)
      return false;
    if (count > (size_t)INT_MAX)
      count = (size_t)INT_MAX;
    SRAL_VoiceInfo *voices = value;
    for (size_t i = 0; i < count; ++i) {
      const char *name = PRISM_SHIM_NULL;
      const char *language = PRISM_SHIM_NULL;
      if (prism_backend_get_voice_name(backend, i, &name) == PRISM_OK)
        name = shim_intern_utf8(&sral_strings, name);
      else
        name = PRISM_SHIM_NULL;
      if (prism_backend_get_voice_language(backend, i, &language) == PRISM_OK)
        language = shim_intern_utf8(&sral_strings, language);
      else
        language = PRISM_SHIM_NULL;
      voices[i].index = (int)i;
      voices[i].name = name;
      voices[i].language = language;
      voices[i].gender = "Unknown";
      voices[i].vendor = "Unknown";
    }
    return true;
  }
  default:
    return false;
  }
}

bool SRAL_GetEngineParameter(int engine, int param, void *value) {
  fast_lock_acquire(&sral_lock);
  const bool result =
      sral_initialized &&
      sral_get_parameter_locked(sral_resolve_locked(engine), param, value);
  fast_lock_release(&sral_lock);
  return result;
}

void SRAL_Delay(int time) { (void)time; }

bool SRAL_RegisterKeyboardHooks(void) { return false; }

void SRAL_UnregisterKeyboardHooks(void) {}

int SRAL_GetAvailableEngines(void) {
  fast_lock_acquire(&sral_lock);
  int result = 0;
  if (sral_ctx != PRISM_SHIM_NULL) {
    for (int i = 0; i < SRAL_SLOT_COUNT; ++i) {
      const SralSlot *slot = &sral_slots[i];
      if (slot->prism_id != PRISM_BACKEND_INVALID &&
          prism_registry_exists(sral_ctx, slot->prism_id))
        result |= slot->legacy_id;
    }
  }
  fast_lock_release(&sral_lock);
  return result;
}

int SRAL_GetActiveEngines(void) {
  fast_lock_acquire(&sral_lock);
  int result = 0;
  if (sral_initialized) {
    for (int i = 0; i < SRAL_SLOT_COUNT; ++i) {
      SralSlot *slot = &sral_slots[i];
      if (slot->prism_id != PRISM_BACKEND_INVALID &&
          shim_slot_live(sral_ctx, slot->prism_id, &slot->backend))
        result |= slot->legacy_id;
    }
  }
  fast_lock_release(&sral_lock);
  return result;
}

SRAL_EngineCategory SRAL_GetEngineCategory(int engine) {
  fast_lock_acquire(&sral_lock);
  const SralSlot *slot = sral_slot(engine);
  const bool present = sral_initialized && slot != PRISM_SHIM_NULL &&
                       prism_registry_exists(sral_ctx, slot->prism_id);
  const SRAL_EngineCategory result =
      present ? slot->category : SRAL_ENGINE_CATEGORY_UNKNOWN;
  fast_lock_release(&sral_lock);
  return result;
}

int SRAL_GetTTSEngines(void) {
  fast_lock_acquire(&sral_lock);
  int result = 0;
  if (sral_initialized) {
    for (int i = 0; i < SRAL_SLOT_COUNT; ++i) {
      const SralSlot *slot = &sral_slots[i];
      if (slot->category == SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE &&
          slot->prism_id != PRISM_BACKEND_INVALID &&
          prism_registry_exists(sral_ctx, slot->prism_id))
        result |= slot->legacy_id;
    }
  }
  fast_lock_release(&sral_lock);
  return result;
}

int SRAL_GetAssistiveTechEngines(void) {
  fast_lock_acquire(&sral_lock);
  int result = 0;
  if (sral_initialized) {
    for (int i = 0; i < SRAL_SLOT_COUNT; ++i) {
      const SralSlot *slot = &sral_slots[i];
      if (slot->category != SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE &&
          slot->prism_id != PRISM_BACKEND_INVALID &&
          prism_registry_exists(sral_ctx, slot->prism_id))
        result |= slot->legacy_id;
    }
  }
  fast_lock_release(&sral_lock);
  return result;
}

const char *SRAL_GetEngineName(int engine) {
  switch (engine) {
  case SRAL_ENGINE_NONE:
    return "None";
  case SRAL_ENGINE_NVDA:
    return "NVDA";
  case SRAL_ENGINE_JAWS:
    return "JAWS";
  case SRAL_ENGINE_ZDSR:
    return "ZDSR";
  case SRAL_ENGINE_NARRATOR:
    return "Narrator";
  case SRAL_ENGINE_UIA:
    return "UIA";
  case SRAL_ENGINE_SAPI:
    return "SAPI";
  case SRAL_ENGINE_SPEECH_DISPATCHER:
    return "Speech Dispatcher";
  case SRAL_ENGINE_VOICE_OVER:
    return "Voice Over";
  case SRAL_ENGINE_NS_SPEECH:
    return "NS Speech";
  case SRAL_ENGINE_AV_SPEECH:
    return "AV Speech";
  case SRAL_ENGINE_ANDROID_ACCESSIBILITY_MANAGER:
    return "Android AccessibilityManager";
  case SRAL_ENGINE_ANDROID_TEXT_TO_SPEECH:
    return "Android TTS";
  default:
    return "Unknown";
  }
}

bool SRAL_SetEnginesExclude(int engines_exclude) {
  fast_lock_acquire(&sral_lock);
  if (!sral_initialized) {
    fast_lock_release(&sral_lock);
    return false;
  }
  sral_excluded = engines_exclude;
  sral_select_locked();
  fast_lock_release(&sral_lock);
  return true;
}

int SRAL_GetEnginesExclude(void) {
  fast_lock_acquire(&sral_lock);
  const int result = sral_initialized ? sral_excluded : -1;
  fast_lock_release(&sral_lock);
  return result;
}
