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
} SralSlot;

static SralSlot sral_slots[] = {
    {.prism_id = PRISM_BACKEND_NVDA,
     .backend = PRISM_SHIM_NULL,
     .legacy_id = SRAL_ENGINE_NVDA,
     .category = SRAL_ENGINE_CATEGORY_SCREEN_READER},
    {.prism_id = PRISM_BACKEND_JAWS,
     .backend = PRISM_SHIM_NULL,
     .legacy_id = SRAL_ENGINE_JAWS,
     .category = SRAL_ENGINE_CATEGORY_SCREEN_READER},
    {.prism_id = PRISM_BACKEND_ZDSR,
     .backend = PRISM_SHIM_NULL,
     .legacy_id = SRAL_ENGINE_ZDSR,
     .category = SRAL_ENGINE_CATEGORY_SCREEN_READER},
    {.prism_id = PRISM_BACKEND_INVALID,
     .backend = PRISM_SHIM_NULL,
     .legacy_id = SRAL_ENGINE_NARRATOR,
     .category = SRAL_ENGINE_CATEGORY_SCREEN_READER},
    {.prism_id = PRISM_BACKEND_UIA,
     .backend = PRISM_SHIM_NULL,
     .legacy_id = SRAL_ENGINE_UIA,
     .category = SRAL_ENGINE_CATEGORY_ACCESSIBILITY_PROVIDER},
    {.prism_id = PRISM_BACKEND_SAPI,
     .backend = PRISM_SHIM_NULL,
     .legacy_id = SRAL_ENGINE_SAPI,
     .category = SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE},
    {.prism_id = PRISM_BACKEND_SPEECH_DISPATCHER,
     .backend = PRISM_SHIM_NULL,
     .legacy_id = SRAL_ENGINE_SPEECH_DISPATCHER,
     .category = SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE},
    {.prism_id = PRISM_BACKEND_VOICE_OVER,
     .backend = PRISM_SHIM_NULL,
     .legacy_id = SRAL_ENGINE_VOICE_OVER,
     .category = SRAL_ENGINE_CATEGORY_SCREEN_READER},
    {.prism_id = PRISM_BACKEND_INVALID,
     .backend = PRISM_SHIM_NULL,
     .legacy_id = SRAL_ENGINE_NS_SPEECH,
     .category = SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE},
    {.prism_id = PRISM_BACKEND_AV_SPEECH,
     .backend = PRISM_SHIM_NULL,
     .legacy_id = SRAL_ENGINE_AV_SPEECH,
     .category = SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE},
    {.prism_id = PRISM_BACKEND_ANDROID_SCREEN_READER,
     .backend = PRISM_SHIM_NULL,
     .legacy_id = SRAL_ENGINE_ANDROID_ACCESSIBILITY_MANAGER,
     .category = SRAL_ENGINE_CATEGORY_ACCESSIBILITY_PROVIDER},
    {.prism_id = PRISM_BACKEND_ANDROID_TTS,
     .backend = PRISM_SHIM_NULL,
     .legacy_id = SRAL_ENGINE_ANDROID_TEXT_TO_SPEECH,
     .category = SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE},
};

static fast_lock sral_lock = FAST_LOCK_INIT;
static PrismContext *sral_ctx;
static SralSlot *sral_current;
static int sral_excluded;
static bool sral_initialized;

static SralSlot *sral_slot(int engine) {
  for (size_t i = 0; i < sizeof(sral_slots) / sizeof(sral_slots[0]); ++i) {
    if (sral_slots[i].legacy_id == engine)
      return &sral_slots[i];
  }
  return PRISM_SHIM_NULL;
}

static SralSlot *sral_slot_by_prism(PrismBackendId id) {
  for (size_t i = 0; i < sizeof(sral_slots) / sizeof(sral_slots[0]); ++i) {
    if (sral_slots[i].prism_id == id)
      return &sral_slots[i];
  }
  return PRISM_SHIM_NULL;
}

static bool sral_slot_active(const SralSlot *slot) {
  if (slot == PRISM_SHIM_NULL || slot->backend == PRISM_SHIM_NULL) {
    return false;
  }
  return (prism_backend_get_features(slot->backend) &
          PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME) != 0;
}

static void sral_update_current(void) {
  sral_current = PRISM_SHIM_NULL;
  if (sral_ctx == PRISM_SHIM_NULL)
    return;
  const size_t count = prism_registry_count(sral_ctx);
  for (size_t i = 0; i < count; ++i) {
    SralSlot *slot = sral_slot_by_prism(prism_registry_id_at(sral_ctx, i));
    if (slot != PRISM_SHIM_NULL && !(sral_excluded & slot->legacy_id) &&
        sral_slot_active(slot)) {
      sral_current = slot;
      return;
    }
  }
}

static SralSlot *sral_resolve(int engine) {
  if (engine == SRAL_ENGINE_NONE) {
    sral_update_current();
    return sral_current;
  }
  return sral_slot(engine);
}

static int sral_features(SralSlot *slot) {
  if (slot == PRISM_SHIM_NULL || slot->backend == PRISM_SHIM_NULL)
    return -1;
  const uint64_t prism = prism_backend_get_features(slot->backend);
  int result = 0;
  if (prism & PRISM_BACKEND_SUPPORTS_SPEAK) {
    result |= SRAL_SUPPORTS_SPEECH;
  }
  if (prism & PRISM_BACKEND_SUPPORTS_BRAILLE) {
    result |= SRAL_SUPPORTS_BRAILLE;
  }
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
  if (prism & PRISM_BACKEND_SUPPORTS_SPEAK_SSML)
    result |= SRAL_SUPPORTS_SSML;
  if (prism & PRISM_BACKEND_SUPPORTS_SPEAK_TO_MEMORY)
    result |= SRAL_SUPPORTS_SPEAK_TO_MEMORY;
  return result;
}

void *SRAL_malloc(size_t size) { return malloc(size); }
void SRAL_free(void *memory) { free(memory); }

bool SRAL_Initialize(int engines_exclude) {
  fast_lock_acquire(&sral_lock);
  if (sral_initialized) {
    sral_excluded = engines_exclude;
    sral_update_current();
    fast_lock_release(&sral_lock);
    return true;
  }
  PrismConfig config = prism_config_init();
  sral_ctx = prism_init(&config);
  if (sral_ctx == PRISM_SHIM_NULL) {
    fast_lock_release(&sral_lock);
    return false;
  }
  bool any = false;
  for (size_t i = 0; i < sizeof(sral_slots) / sizeof(sral_slots[0]); ++i) {
    SralSlot *slot = &sral_slots[i];
    if (slot->prism_id != PRISM_BACKEND_INVALID &&
        prism_registry_exists(sral_ctx, slot->prism_id)) {
      slot->backend = shim_create_initialized(sral_ctx, slot->prism_id);
      any = ((any || slot->backend != PRISM_SHIM_NULL) != 0);
    }
  }
  if (!any) {
    prism_shutdown(sral_ctx);
    sral_ctx = PRISM_SHIM_NULL;
    fast_lock_release(&sral_lock);
    return false;
  }
  sral_excluded = engines_exclude;
  sral_initialized = true;
  sral_update_current();
  fast_lock_release(&sral_lock);
  return true;
}

void SRAL_Uninitialize(void) {
  fast_lock_acquire(&sral_lock);
  for (size_t i = 0; i < sizeof(sral_slots) / sizeof(sral_slots[0]); ++i) {
    if (sral_slots[i].backend != PRISM_SHIM_NULL)
      prism_backend_free(sral_slots[i].backend);
    sral_slots[i].backend = PRISM_SHIM_NULL;
  }
  sral_current = PRISM_SHIM_NULL;
  if (sral_ctx != PRISM_SHIM_NULL)
    prism_shutdown(sral_ctx);
  sral_ctx = PRISM_SHIM_NULL;
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

static bool sral_speak_locked(SralSlot *slot, const char *text,
                              bool interrupt) {
  return (slot != PRISM_SHIM_NULL && slot->backend != PRISM_SHIM_NULL &&
          text != PRISM_SHIM_NULL &&
          prism_backend_speak(slot->backend, text, interrupt) == PRISM_OK) != 0;
}

bool SRAL_SpeakEx(int engine, const char *text, bool interrupt) {
  fast_lock_acquire(&sral_lock);
  const bool result = sral_speak_locked(sral_resolve(engine), text, interrupt);
  fast_lock_release(&sral_lock);
  return result;
}

bool SRAL_Speak(const char *text, bool interrupt) {
  return SRAL_SpeakEx(SRAL_ENGINE_NONE, text, interrupt);
}

bool SRAL_SpeakSsmlEx(int engine, const char *ssml, bool interrupt) {
  /* Prism exposes SSML capability flags but currently uses the same speak entry
   * point. */
  return SRAL_SpeakEx(engine, ssml, interrupt);
}

bool SRAL_SpeakSsml(const char *ssml, bool interrupt) {
  return SRAL_SpeakSsmlEx(SRAL_ENGINE_NONE, ssml, interrupt);
}
void *SRAL_SpeakToMemoryEx(int engine, const char *text, uint64_t *buffer_size,
                           int *channels, int *sample_rate,
                           int *bits_per_sample) {
  fast_lock_acquire(&sral_lock);
  SralSlot *slot = sral_resolve(engine);
  ShimAudioBuffer audio;
  shim_audio_buffer_init(&audio);
  void *result = PRISM_SHIM_NULL;
  if (slot != PRISM_SHIM_NULL && slot->backend != PRISM_SHIM_NULL &&
      text != PRISM_SHIM_NULL &&
      shim_synthesize_audio(slot->backend, text, &audio)) {
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
  fast_lock_acquire(&sral_lock);
  SralSlot *slot = sral_resolve(engine);
  const bool result =
      (slot != PRISM_SHIM_NULL && slot->backend != PRISM_SHIM_NULL &&
       text != PRISM_SHIM_NULL &&
       prism_backend_braille(slot->backend, text) == PRISM_OK) != 0;
  fast_lock_release(&sral_lock);
  return result;
}

bool SRAL_Braille(const char *text) {
  return SRAL_BrailleEx(SRAL_ENGINE_NONE, text);
}

bool SRAL_OutputEx(int engine, const char *text, bool interrupt) {
  fast_lock_acquire(&sral_lock);
  SralSlot *slot = sral_resolve(engine);
  bool speech = false;
  bool braille = false;
  if (slot != PRISM_SHIM_NULL && slot->backend != PRISM_SHIM_NULL &&
      text != PRISM_SHIM_NULL) {
    speech = prism_backend_speak(slot->backend, text, interrupt) == PRISM_OK;
    braille = prism_backend_braille(slot->backend, text) == PRISM_OK;
  }
  fast_lock_release(&sral_lock);
  return (speech || braille) != 0;
}

bool SRAL_Output(const char *text, bool interrupt) {
  return SRAL_OutputEx(SRAL_ENGINE_NONE, text, interrupt);
}
bool SRAL_StopSpeechEx(int engine) {
  fast_lock_acquire(&sral_lock);
  SralSlot *slot = sral_resolve(engine);
  const bool result =
      (slot != PRISM_SHIM_NULL && slot->backend != PRISM_SHIM_NULL &&
       prism_backend_stop(slot->backend) == PRISM_OK) != 0;
  fast_lock_release(&sral_lock);
  return result;
}

bool SRAL_StopSpeech(void) { return SRAL_StopSpeechEx(SRAL_ENGINE_NONE); }

bool SRAL_PauseSpeechEx(int engine) {
  fast_lock_acquire(&sral_lock);
  SralSlot *slot = sral_resolve(engine);
  const bool result =
      (slot != PRISM_SHIM_NULL && slot->backend != PRISM_SHIM_NULL &&
       prism_backend_pause(slot->backend) == PRISM_OK) != 0;
  fast_lock_release(&sral_lock);
  return result;
}

bool SRAL_PauseSpeech(void) { return SRAL_PauseSpeechEx(SRAL_ENGINE_NONE); }

bool SRAL_ResumeSpeechEx(int engine) {
  fast_lock_acquire(&sral_lock);
  SralSlot *slot = sral_resolve(engine);
  const bool result =
      (slot != PRISM_SHIM_NULL && slot->backend != PRISM_SHIM_NULL &&
       prism_backend_resume(slot->backend) == PRISM_OK) != 0;
  fast_lock_release(&sral_lock);
  return result;
}

bool SRAL_ResumeSpeech(void) { return SRAL_ResumeSpeechEx(SRAL_ENGINE_NONE); }
bool SRAL_IsSpeakingEx(int engine) {
  fast_lock_acquire(&sral_lock);
  SralSlot *slot = sral_resolve(engine);
  bool speaking = false;
  const bool ok =
      (slot != PRISM_SHIM_NULL && slot->backend != PRISM_SHIM_NULL &&
       prism_backend_is_speaking(slot->backend, &speaking) == PRISM_OK) != 0;
  fast_lock_release(&sral_lock);
  return (ok && speaking) != 0;
}

bool SRAL_IsSpeaking(void) { return SRAL_IsSpeakingEx(SRAL_ENGINE_NONE); }

int SRAL_GetCurrentEngine(void) {
  fast_lock_acquire(&sral_lock);
  sral_update_current();
  const int result = sral_current == PRISM_SHIM_NULL ? SRAL_ENGINE_NONE
                                                     : sral_current->legacy_id;
  fast_lock_release(&sral_lock);
  return result;
}

int SRAL_GetEngineFeatures(int engine) {
  fast_lock_acquire(&sral_lock);
  const int result = sral_features(sral_resolve(engine));
  fast_lock_release(&sral_lock);
  return result;
}

static bool sral_set_parameter_locked(SralSlot *slot, int param,
                                      const void *value) {
  if (slot == PRISM_SHIM_NULL || slot->backend == PRISM_SHIM_NULL ||
      value == PRISM_SHIM_NULL)
    return false;
  switch (param) {
  case SRAL_PARAM_SPEECH_RATE: {
    const int rate = *(const int *)value;
    return prism_backend_set_rate(
               slot->backend, shim_clamp01(((float)rate + 100.0F) / 200.0F)) ==
           PRISM_OK;
  }
  case SRAL_PARAM_SPEECH_VOLUME: {
    const int volume = *(const int *)value;
    return prism_backend_set_volume(
               slot->backend, shim_clamp01((float)volume / 100.0F)) == PRISM_OK;
  }
  case SRAL_PARAM_VOICE_INDEX: {
    const int index = *(const int *)value;
    return (index >= 0 && prism_backend_set_voice(
                              slot->backend, (size_t)index) == PRISM_OK) != 0;
  }
  default:
    return false;
  }
}

bool SRAL_SetEngineParameter(int engine, int param, const void *value) {
  fast_lock_acquire(&sral_lock);
  const bool result =
      sral_set_parameter_locked(sral_resolve(engine), param, value);
  fast_lock_release(&sral_lock);
  return result;
}

static bool sral_get_parameter_locked(SralSlot *slot, int param, void *value) {
  if (slot == PRISM_SHIM_NULL || slot->backend == PRISM_SHIM_NULL ||
      value == PRISM_SHIM_NULL)
    return false;
  switch (param) {
  case SRAL_PARAM_SPEECH_RATE: {
    float rate = 0.0F;
    if (prism_backend_get_rate(slot->backend, &rate) != PRISM_OK)
      return false;
    *(int *)value = (int)((rate * 200.0F) - 100.0F);
    return true;
  }
  case SRAL_PARAM_SPEECH_VOLUME: {
    float volume = 0.0F;
    if (prism_backend_get_volume(slot->backend, &volume) != PRISM_OK)
      return false;
    *(int *)value = (int)(volume * 100.0F);
    return true;
  }
  case SRAL_PARAM_VOICE_INDEX: {
    size_t index = 0;
    if (prism_backend_get_voice(slot->backend, &index) != PRISM_OK ||
        index > (size_t)INT_MAX)
      return false;
    *(int *)value = (int)index;
    return true;
  }
  case SRAL_PARAM_VOICE_COUNT: {
    size_t count = 0;
    if (prism_backend_count_voices(slot->backend, &count) != PRISM_OK)
      return false;
    *(int *)value = count > (size_t)INT_MAX ? INT_MAX : (int)count;
    return true;
  }
  case SRAL_PARAM_VOICE_PROPERTIES: {
    size_t count = 0;
    if (prism_backend_count_voices(slot->backend, &count) != PRISM_OK)
      return false;
    SRAL_VoiceInfo *voices = value;
    for (size_t i = 0; i < count; ++i) {
      const char *name = PRISM_SHIM_NULL;
      const char *language = PRISM_SHIM_NULL;
      shim_ignore_error(prism_backend_get_voice_name(slot->backend, i, &name));
      shim_ignore_error(
          prism_backend_get_voice_language(slot->backend, i, &language));
      voices[i].index = i > (size_t)INT_MAX ? INT_MAX : (int)i;
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
      sral_get_parameter_locked(sral_resolve(engine), param, value);
  fast_lock_release(&sral_lock);
  return result;
}

void SRAL_Delay(int time) {
  /* Delayed-output scheduling is not a Prism operation. Compatibility no-op. */
  (void)time;
}

bool SRAL_RegisterKeyboardHooks(void) {
  /* Global input hooks are intentionally outside the shim's scope. */
  return false;
}

void SRAL_UnregisterKeyboardHooks(void) {}

int SRAL_GetAvailableEngines(void) {
  fast_lock_acquire(&sral_lock);
  int result = 0;
  if (sral_ctx != PRISM_SHIM_NULL) {
    for (size_t i = 0; i < sizeof(sral_slots) / sizeof(sral_slots[0]); ++i) {
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
  for (size_t i = 0; i < sizeof(sral_slots) / sizeof(sral_slots[0]); ++i) {
    if (sral_slot_active(&sral_slots[i]))
      result |= sral_slots[i].legacy_id;
  }
  fast_lock_release(&sral_lock);
  return result;
}

SRAL_EngineCategory SRAL_GetEngineCategory(int engine) {
  fast_lock_acquire(&sral_lock);
  SralSlot *slot = sral_slot(engine);
  const bool present = (sral_initialized && slot != PRISM_SHIM_NULL &&
                        slot->prism_id != PRISM_BACKEND_INVALID &&
                        prism_registry_exists(sral_ctx, slot->prism_id)) != 0;
  const SRAL_EngineCategory result =
      (int)present ? slot->category : SRAL_ENGINE_CATEGORY_UNKNOWN;
  fast_lock_release(&sral_lock);
  return result;
}

int SRAL_GetTTSEngines(void) {
  fast_lock_acquire(&sral_lock);
  int result = 0;
  if (sral_initialized) {
    for (size_t i = 0; i < sizeof(sral_slots) / sizeof(sral_slots[0]); ++i) {
      const SralSlot *slot = &sral_slots[i];
      if (slot->category == SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE &&
          slot->prism_id != PRISM_BACKEND_INVALID &&
          prism_registry_exists(sral_ctx, slot->prism_id)) {
        result |= slot->legacy_id;
      }
    }
  }
  fast_lock_release(&sral_lock);
  return result;
}

int SRAL_GetAssistiveTechEngines(void) {
  fast_lock_acquire(&sral_lock);
  int result = 0;
  if (sral_initialized) {
    for (size_t i = 0; i < sizeof(sral_slots) / sizeof(sral_slots[0]); ++i) {
      const SralSlot *slot = &sral_slots[i];
      if (slot->category != SRAL_ENGINE_CATEGORY_TEXT_TO_SPEECH_ENGINE &&
          slot->prism_id != PRISM_BACKEND_INVALID &&
          prism_registry_exists(sral_ctx, slot->prism_id)) {
        result |= slot->legacy_id;
      }
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
  sral_update_current();
  fast_lock_release(&sral_lock);
  return true;
}

int SRAL_GetEnginesExclude(void) {
  fast_lock_acquire(&sral_lock);
  const int result = (int)sral_initialized ? sral_excluded : -1;
  fast_lock_release(&sral_lock);
  return result;
}
