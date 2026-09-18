// SPDX-License-Identifier: MPL-2.0
#include "../common/lock.h"
#include "../common/shim_common.h"
#include "UniversalSpeech.h"

#include <limits.h>
#include <prism.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  (void)reserved;
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(instance);
  }
  return TRUE;
}

#define US_STDCALL __stdcall
#define US_EXPORT_STDCALL __declspec(dllexport) __stdcall
#else
#define US_STDCALL
#define US_EXPORT_STDCALL __attribute__((visibility("default")))
#endif

typedef struct UsSlot {
  const wchar_t *name;
  PrismBackendId prism_id;
  PrismBackend *backend;
  bool paused;
} UsSlot;

static UsSlot us_slots[] = {
    {L"Jaws", PRISM_BACKEND_JAWS, PRISM_SHIM_NULL, false},
    {L"Windows eye", PRISM_BACKEND_WINDOW_EYES, PRISM_SHIM_NULL, false},
    {L"NVDA", PRISM_BACKEND_NVDA, PRISM_SHIM_NULL, false},
    {L"System access", PRISM_BACKEND_SYSTEM_ACCESS, PRISM_SHIM_NULL, false},
    {L"Supernova", PRISM_BACKEND_INVALID, PRISM_SHIM_NULL, false},
    {L"ZoomText", PRISM_BACKEND_ZOOM_TEXT, PRISM_SHIM_NULL, false},
    {L"ZDSR", PRISM_BACKEND_ZDSR, PRISM_SHIM_NULL, false},
    {L"Cobra", PRISM_BACKEND_INVALID, PRISM_SHIM_NULL, false},
    {L"Narrator", PRISM_BACKEND_UIA, PRISM_SHIM_NULL, false},
    {L"SAPI5", PRISM_BACKEND_INVALID, PRISM_SHIM_NULL, false},
};

enum { US_ENGINE_COUNT = (int)(sizeof(us_slots) / sizeof(us_slots[0])) };
enum { US_NATIVE_INDEX = US_ENGINE_COUNT - 1 };

static fast_lock us_lock = FAST_LOCK_INIT;
static PrismContext *us_ctx;
static int us_current = -1;
static int us_auto_engine = 1;
static int us_native_enabled = 1;
static _Thread_local wchar_t *us_wide_result;
static _Thread_local char *us_narrow_result;

typedef int (*UsWaveCallback)(void *, void *, int);
static UsWaveCallback us_wave_callback;
static void *us_wave_userdata;
static int us_wave_sample_rate;

static PrismBackendId us_slot_id(int index) {
  if (index == US_NATIVE_INDEX)
    return shim_native_tts_id();
  if (index < 0 || index >= US_ENGINE_COUNT)
    return PRISM_BACKEND_INVALID;
  return us_slots[index].prism_id;
}
static bool us_ensure_context_locked(void) {
  if (us_ctx != PRISM_SHIM_NULL)
    return true;
  PrismConfig config = prism_config_init();
  us_ctx = prism_init(&config);
  return us_ctx != PRISM_SHIM_NULL;
}

static PrismBackend *us_backend_locked(int index) {
  if (index < 0 || index >= US_ENGINE_COUNT || !us_ensure_context_locked())
    return PRISM_SHIM_NULL;
  UsSlot *slot = &us_slots[index];
  if (slot->backend != PRISM_SHIM_NULL)
    return slot->backend;
  const PrismBackendId id = us_slot_id(index);
  if (id == PRISM_BACKEND_INVALID || !prism_registry_exists(us_ctx, id))
    return PRISM_SHIM_NULL;
  slot->backend = shim_create_initialized(us_ctx, id);
  return slot->backend;
}

static bool us_available_locked(int index) {
  if (index == US_NATIVE_INDEX && !us_native_enabled)
    return false;
  PrismBackend *backend = us_backend_locked(index);
  return (backend != PRISM_SHIM_NULL &&
          (prism_backend_get_features(backend) &
           PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME) != 0) != 0;
}

static int us_detect_locked(void) {
  for (int i = 0; i < US_ENGINE_COUNT; ++i) {
    if (us_available_locked(i))
      return i;
  }
  return -1;
}
static bool us_speak_w_locked(int index, const wchar_t *text, bool interrupt) {
  PrismBackend *backend = us_backend_locked(index);
  if (backend == PRISM_SHIM_NULL || text == PRISM_SHIM_NULL)
    return false;
  char *utf8 = shim_wchar_to_utf8(text);
  if (utf8 == PRISM_SHIM_NULL)
    return false;
  const bool result = prism_backend_speak(backend, utf8, interrupt) == PRISM_OK;
  free(utf8);
  return result;
}

static bool us_braille_w_locked(int index, const wchar_t *text) {
  PrismBackend *backend = us_backend_locked(index);
  if (backend == PRISM_SHIM_NULL || text == PRISM_SHIM_NULL)
    return false;
  char *utf8 = shim_wchar_to_utf8(text);
  if (utf8 == PRISM_SHIM_NULL)
    return false;
  const bool result = prism_backend_braille(backend, utf8) == PRISM_OK;
  free(utf8);
  return result;
}

static bool us_stop_locked(int index) {
  PrismBackend *backend = us_backend_locked(index);
  return (backend != PRISM_SHIM_NULL &&
          prism_backend_stop(backend) == PRISM_OK) != 0;
}

static int us_resolve_current_locked(void) {
  if (us_current < 0 || us_current >= US_ENGINE_COUNT) {
    us_current = us_detect_locked();
  }
  return us_current;
}
export int speechSay(const ____wchar_t *text, int interrupt) {
  fast_lock_acquire(&us_lock);
  int index = us_resolve_current_locked();
  if (index < 0) {
    fast_lock_release(&us_lock);
    return 0;
  }
  bool result = us_speak_w_locked(index, text, interrupt != 0);
  if (!result && !us_available_locked(index) && us_auto_engine) {
    us_current = -1;
    index = us_resolve_current_locked();
    if (index >= 0)
      result = us_speak_w_locked(index, text, interrupt != 0);
  }
  const int compatible = index >= 0 && (result || us_available_locked(index));
  fast_lock_release(&us_lock);
  return compatible;
}

export int speechStop(void) {
  fast_lock_acquire(&us_lock);
  int index = us_resolve_current_locked();
  if (index < 0) {
    fast_lock_release(&us_lock);
    return 0;
  }
  bool result = us_stop_locked(index);
  if (!result && !us_available_locked(index) && us_auto_engine) {
    us_current = -1;
    index = us_resolve_current_locked();
    if (index >= 0)
      result = us_stop_locked(index);
  }
  const int compatible = index >= 0 && (result || us_available_locked(index));
  fast_lock_release(&us_lock);
  return compatible;
}
export int brailleDisplay(const ____wchar_t *text) {
  fast_lock_acquire(&us_lock);
  int index = us_resolve_current_locked();
  if (index < 0) {
    fast_lock_release(&us_lock);
    return 0;
  }
  bool result = us_braille_w_locked(index, text);
  if (!result && !us_available_locked(index) && us_auto_engine) {
    us_current = -1;
    index = us_resolve_current_locked();
    if (index >= 0)
      result = us_braille_w_locked(index, text);
  }
  const int compatible = index >= 0 && (result || us_available_locked(index));
  fast_lock_release(&us_lock);
  return compatible;
}

static bool us_get_float_locked(PrismBackend *backend, int what, float *out) {
  if (backend == PRISM_SHIM_NULL || out == PRISM_SHIM_NULL)
    return false;
  switch (what) {
  case SP_VOLUME:
    return prism_backend_get_volume(backend, out) == PRISM_OK;
  case SP_RATE:
    return prism_backend_get_rate(backend, out) == PRISM_OK;
  case SP_PITCH:
    return prism_backend_get_pitch(backend, out) == PRISM_OK;
  default:
    return false;
  }
}

static uint64_t us_parameter_feature(int what, bool setter) {
  switch (what) {
  case SP_VOLUME:
    return (int)setter ? PRISM_BACKEND_SUPPORTS_SET_VOLUME
                       : PRISM_BACKEND_SUPPORTS_GET_VOLUME;
  case SP_RATE:
    return (int)setter ? PRISM_BACKEND_SUPPORTS_SET_RATE
                       : PRISM_BACKEND_SUPPORTS_GET_RATE;
  case SP_PITCH:
    return (int)setter ? PRISM_BACKEND_SUPPORTS_SET_PITCH
                       : PRISM_BACKEND_SUPPORTS_GET_PITCH;
  default:
    return 0;
  }
}

export int speechGetValue(int what) {
  fast_lock_acquire(&us_lock);
  if (what == SP_ENGINE) {
    const int result = us_resolve_current_locked();
    fast_lock_release(&us_lock);
    return result;
  }
  if (what == SP_ENABLE_NATIVE_SPEECH) {
    const int result = us_native_enabled;
    fast_lock_release(&us_lock);
    return result;
  }
  if (what == SP_AUTO_ENGINE) {
    const int result = us_auto_engine;
    fast_lock_release(&us_lock);
    return result;
  }
  if (what >= SP_ENGINE_AVAILABLE &&
      what < SP_ENGINE_AVAILABLE + US_ENGINE_COUNT) {
    const int result = (int)us_available_locked(what - SP_ENGINE_AVAILABLE);
    fast_lock_release(&us_lock);
    return result;
  }
  const int index = us_resolve_current_locked();
  PrismBackend *backend =
      index < 0 ? PRISM_SHIM_NULL : us_backend_locked(index);
  const uint64_t features =
      backend == PRISM_SHIM_NULL ? 0 : prism_backend_get_features(backend);
  if (what == SP_BUSY) {
    bool speaking = false;
    const int result =
        backend != PRISM_SHIM_NULL &&
        prism_backend_is_speaking(backend, &speaking) == PRISM_OK && speaking;
    fast_lock_release(&us_lock);
    return result;
  }
  if (what == SP_BUSY_SUPPORTED) {
    const int result = (features & PRISM_BACKEND_SUPPORTS_IS_SPEAKING) != 0;
    fast_lock_release(&us_lock);
    return result;
  }
  if (what == SP_PAUSED) {
    const int result =
        index >= 0 && index < US_ENGINE_COUNT && us_slots[index].paused ? 1 : 0;
    fast_lock_release(&us_lock);
    return result;
  }
  if (what == SP_PAUSE_SUPPORTED) {
    const int result = (features & PRISM_BACKEND_SUPPORTS_PAUSE) != 0 &&
                       (features & PRISM_BACKEND_SUPPORTS_RESUME) != 0;
    fast_lock_release(&us_lock);
    return result;
  }
  if (what == SP_WAIT || what == SP_WAIT_SUPPORTED) {
    fast_lock_release(&us_lock);
    return 0;
  }
  if (what == SP_VOLUME_MIN || what == SP_RATE_MIN || what == SP_PITCH_MIN) {
    fast_lock_release(&us_lock);
    return 0;
  }
  if (what == SP_VOLUME_MAX || what == SP_RATE_MAX || what == SP_PITCH_MAX) {
    fast_lock_release(&us_lock);
    return 100;
  }
  if (what == SP_VOLUME_SUPPORTED || what == SP_RATE_SUPPORTED ||
      what == SP_PITCH_SUPPORTED) {
    int base = SP_PITCH;
    if (what == SP_VOLUME_SUPPORTED) {
      base = SP_VOLUME;
    } else if (what == SP_RATE_SUPPORTED) {
      base = SP_RATE;
    }
    const int result = (features & us_parameter_feature(base, false)) != 0;
    fast_lock_release(&us_lock);
    return result;
  }
  if (what == SP_INFLEXION || what == SP_INFLEXION_MIN ||
      what == SP_INFLEXION_MAX || what == SP_INFLEXION_SUPPORTED) {
    fast_lock_release(&us_lock);
    return 0;
  }
  if (what == SP_VOICE) {
    size_t voice = 0;
    const int result =
        backend != PRISM_SHIM_NULL &&
                prism_backend_get_voice(backend, &voice) == PRISM_OK &&
                voice <= (size_t)INT_MAX
            ? (int)voice
            : -1;
    fast_lock_release(&us_lock);
    return result;
  }
  float value = 0.0F;
  if (us_get_float_locked(backend, what, &value)) {
    const int result = (int)(shim_clamp01(value) * 100.0F);
    fast_lock_release(&us_lock);
    return result;
  }
  fast_lock_release(&us_lock);
  return 0;
}

static bool us_set_float_locked(PrismBackend *backend, int what, int value) {
  if (backend == PRISM_SHIM_NULL)
    return false;
  const float normalized = shim_clamp01((float)value / 100.0F);
  switch (what) {
  case SP_VOLUME:
    return prism_backend_set_volume(backend, normalized) == PRISM_OK;
  case SP_RATE:
    return prism_backend_set_rate(backend, normalized) == PRISM_OK;
  case SP_PITCH:
    return prism_backend_set_pitch(backend, normalized) == PRISM_OK;
  default:
    return false;
  }
}
export int speechSetValue(int what, int value) {
  fast_lock_acquire(&us_lock);
  if (what == SP_ENGINE) {
    if (value < 0) {
      us_current = -1;
      us_auto_engine = 1;
      fast_lock_release(&us_lock);
      return 1;
    }
    if (value < US_ENGINE_COUNT) {
      us_current = value;
      us_auto_engine = 0;
      fast_lock_release(&us_lock);
      return 1;
    }
    fast_lock_release(&us_lock);
    return 0;
  }
  if (what == SP_ENABLE_NATIVE_SPEECH) {
    us_native_enabled = value != 0;
    if (!us_native_enabled && us_current == US_NATIVE_INDEX)
      us_current = -1;
    const int result = us_native_enabled;
    fast_lock_release(&us_lock);
    return result;
  }
  const int index = us_resolve_current_locked();
  PrismBackend *backend =
      index < 0 ? PRISM_SHIM_NULL : us_backend_locked(index);
  if (what == SP_PAUSED) {
    PrismError error = PRISM_ERROR_INVALID_OPERATION;
    if (backend != PRISM_SHIM_NULL)
      error = value != 0 ? prism_backend_pause(backend)
                         : prism_backend_resume(backend);
    const int result = error == PRISM_OK;
    if (result && index >= 0 && index < US_ENGINE_COUNT)
      us_slots[index].paused = value != 0;
    fast_lock_release(&us_lock);
    return result;
  }
  if (what == SP_VOICE) {
    const int result =
        backend != PRISM_SHIM_NULL && value >= 0 &&
        prism_backend_set_voice(backend, (size_t)value) == PRISM_OK;
    fast_lock_release(&us_lock);
    return result;
  }
  if (what == SP_WAIT) {
    fast_lock_release(&us_lock);
    return 0;
  }
  if (what == SP_VOLUME || what == SP_RATE || what == SP_PITCH) {
    const int result = (int)us_set_float_locked(backend, what, value);
    fast_lock_release(&us_lock);
    return result;
  }
  fast_lock_release(&us_lock);
  return 0;
}

export const ____wchar_t *speechGetString(int what) {
  fast_lock_acquire(&us_lock);
  if (what >= SP_ENGINE && what < SP_ENGINE + US_ENGINE_COUNT) {
    const wchar_t *result = us_slots[what - SP_ENGINE].name;
    fast_lock_release(&us_lock);
    return result;
  }
  const int index = us_resolve_current_locked();
  PrismBackend *backend =
      index < 0 ? PRISM_SHIM_NULL : us_backend_locked(index);
  if (what >= SP_VOICE && what < SP_VOICE + 0x10000 &&
      backend != PRISM_SHIM_NULL) {
    const char *name = PRISM_SHIM_NULL;
    if (prism_backend_get_voice_name(backend, (size_t)(what - SP_VOICE),
                                     &name) == PRISM_OK) {
      free(us_wide_result);
      us_wide_result = shim_utf8_to_wchar(name);
      const wchar_t *result = us_wide_result;
      fast_lock_release(&us_lock);
      return result;
    }
  }
  fast_lock_release(&us_lock);
  return PRISM_SHIM_NULL;
}

export int speechSetString(int what, const ____wchar_t *value) {
  if (value == PRISM_SHIM_NULL || what != SP_VOICE)
    return 0;
  fast_lock_acquire(&us_lock);
  const int index = us_resolve_current_locked();
  PrismBackend *backend =
      index < 0 ? PRISM_SHIM_NULL : us_backend_locked(index);
  char *wanted = shim_wchar_to_utf8(value);
  size_t count = 0;
  int result = 0;
  if (backend != PRISM_SHIM_NULL && wanted != PRISM_SHIM_NULL &&
      prism_backend_count_voices(backend, &count) == PRISM_OK) {
    for (size_t i = 0; i < count; ++i) {
      const char *name = PRISM_SHIM_NULL;
      if (prism_backend_get_voice_name(backend, i, &name) == PRISM_OK &&
          name != PRISM_SHIM_NULL && strcmp(name, wanted) == 0) {
        result = prism_backend_set_voice(backend, i) == PRISM_OK;
        break;
      }
    }
  }
  free(wanted);
  fast_lock_release(&us_lock);
  return result;
}

static const char *us_convert_result(const wchar_t *value, bool utf8) {
  free(us_narrow_result);
  us_narrow_result = PRISM_SHIM_NULL;
  if (value == PRISM_SHIM_NULL)
    return PRISM_SHIM_NULL;
  char *encoded = shim_wchar_to_utf8(value);
  if (encoded == PRISM_SHIM_NULL)
    return PRISM_SHIM_NULL;
  if (utf8) {
    us_narrow_result = encoded;
  } else {
    us_narrow_result = shim_utf8_to_ansi(encoded);
    free(encoded);
  }
  return us_narrow_result;
}

export int speechSayA(const char *text, int interrupt) {
  char *utf8 = shim_ansi_to_utf8(text);
  wchar_t *wide = shim_utf8_to_wchar(utf8);
  const int result = wide == PRISM_SHIM_NULL ? 0 : speechSay(wide, interrupt);
  free(wide);
  free(utf8);
  return result;
}

export int brailleDisplayA(const char *text) {
  char *utf8 = shim_ansi_to_utf8(text);
  wchar_t *wide = shim_utf8_to_wchar(utf8);
  const int result = wide == PRISM_SHIM_NULL ? 0 : brailleDisplay(wide);
  free(wide);
  free(utf8);
  return result;
}

export const char *speechGetStringA(int what) {
  return us_convert_result(speechGetString(what), false);
}

export int speechSetStringA(int what, const char *value) {
  char *utf8 = shim_ansi_to_utf8(value);
  wchar_t *wide = shim_utf8_to_wchar(utf8);
  const int result = wide == PRISM_SHIM_NULL ? 0 : speechSetString(what, wide);
  free(wide);
  free(utf8);
  return result;
}

export int speechSayU(const char *text, int interrupt) {
  wchar_t *wide = shim_utf8_to_wchar(text);
  const int result = wide == PRISM_SHIM_NULL ? 0 : speechSay(wide, interrupt);
  free(wide);
  return result;
}
export int brailleDisplayU(const char *text) {
  wchar_t *wide = shim_utf8_to_wchar(text);
  const int result = wide == PRISM_SHIM_NULL ? 0 : brailleDisplay(wide);
  free(wide);
  return result;
}

export const char *speechGetStringU(int what) {
  return us_convert_result(speechGetString(what), true);
}

export int speechSetStringU(int what, const char *value) {
  wchar_t *wide = shim_utf8_to_wchar(value);
  const int result = wide == PRISM_SHIM_NULL ? 0 : speechSetString(what, wide);
  free(wide);
  return result;
}

export int speechSetStringW(int what, const wchar_t *value) {
  return speechSetString(what, value);
}

export int brailleDisplayW(const wchar_t *text) { return brailleDisplay(text); }

static int us_index_for_id(PrismBackendId id) {
  if (id == shim_native_tts_id())
    return US_NATIVE_INDEX;
  for (int i = 0; i < US_ENGINE_COUNT - 1; ++i)
    if (us_slots[i].prism_id == id)
      return i;
  return -1;
}
static int us_id_available(PrismBackendId id) {
  fast_lock_acquire(&us_lock);
  const int index = us_index_for_id(id);
  const int result = index >= 0 && us_available_locked(index);
  fast_lock_release(&us_lock);
  return result;
}

static int us_id_speak_w(PrismBackendId id, const wchar_t *text,
                         int interrupt) {
  fast_lock_acquire(&us_lock);
  const int index = us_index_for_id(id);
  const int result =
      index >= 0 && us_speak_w_locked(index, text, interrupt != 0);
  fast_lock_release(&us_lock);
  return result;
}

static int us_id_braille_w(PrismBackendId id, const wchar_t *text) {
  fast_lock_acquire(&us_lock);
  const int index = us_index_for_id(id);
  const int result = index >= 0 && us_braille_w_locked(index, text);
  fast_lock_release(&us_lock);
  return result;
}

static int us_id_stop(PrismBackendId id) {
  fast_lock_acquire(&us_lock);
  const int index = us_index_for_id(id);
  const int result = index >= 0 && us_stop_locked(index);
  fast_lock_release(&us_lock);
  return result;
}
static int us_id_speak_a(PrismBackendId id, const char *text, int interrupt) {
  char *utf8 = shim_ansi_to_utf8(text);
  wchar_t *wide = shim_utf8_to_wchar(utf8);
  const int result =
      wide == PRISM_SHIM_NULL ? 0 : us_id_speak_w(id, wide, interrupt);
  free(wide);
  free(utf8);
  return result;
}

static int us_id_braille_a(PrismBackendId id, const char *text) {
  char *utf8 = shim_ansi_to_utf8(text);
  wchar_t *wide = shim_utf8_to_wchar(utf8);
  const int result = wide == PRISM_SHIM_NULL ? 0 : us_id_braille_w(id, wide);
  free(wide);
  free(utf8);
  return result;
}

static void us_id_unload(PrismBackendId id) {
  fast_lock_acquire(&us_lock);
  const int index = us_index_for_id(id);
  if (index >= 0 && us_slots[index].backend != PRISM_SHIM_NULL) {
    prism_backend_free(us_slots[index].backend);
    us_slots[index].backend = PRISM_SHIM_NULL;
    us_slots[index].paused = false;
    if (us_current == index && us_auto_engine)
      us_current = -1;
  }
  fast_lock_release(&us_lock);
}
/* JAWS */
export int jfwLoad(void) { return us_id_available(PRISM_BACKEND_JAWS); }
export void jfwUnload(void) { us_id_unload(PRISM_BACKEND_JAWS); }
export int jfwIsAvailable(void) { return us_id_available(PRISM_BACKEND_JAWS); }
export int jfwSayA(const char *text, int interrupt) {
  return us_id_speak_a(PRISM_BACKEND_JAWS, text, interrupt);
}
export int jfwSayW(const wchar_t *text, int interrupt) {
  return us_id_speak_w(PRISM_BACKEND_JAWS, text, interrupt);
}
export int jfwBrailleA(const char *text) {
  return us_id_braille_a(PRISM_BACKEND_JAWS, text);
}
export int jfwBrailleW(const wchar_t *text) {
  return us_id_braille_w(PRISM_BACKEND_JAWS, text);
}
export int jfwStopSpeech(void) { return us_id_stop(PRISM_BACKEND_JAWS); }
export int jfwRunScriptA(const char *name) {
  (void)name;
  return 0;
}
export int jfwRunScriptW(const wchar_t *name) {
  (void)name;
  return 0;
}
export int jfwRunFunctionA(const char *name) {
  (void)name;
  return 0;
}
export int jfwRunFunctionW(const wchar_t *name) {
  (void)name;
  return 0;
}
export int jfwGetUserSettingsDirectory(char *buf, size_t size) {
  if (buf != PRISM_SHIM_NULL && size != 0)
    buf[0] = '\0';
  return 0;
}
export int jfwGetRunningVersion(char *buf, int size) {
  if (buf != PRISM_SHIM_NULL && size > 0)
    buf[0] = '\0';
  return 0;
}
/* NVDA */
export int nvdaLoad(void) { return us_id_available(PRISM_BACKEND_NVDA); }
export void nvdaUnload(void) { us_id_unload(PRISM_BACKEND_NVDA); }
export int nvdaIsAvailable(void) { return us_id_available(PRISM_BACKEND_NVDA); }
export int nvdaSay(const wchar_t *text) {
  return us_id_speak_w(PRISM_BACKEND_NVDA, text, 0);
}
export int nvdaSayW(const wchar_t *text, int interrupt) {
  return us_id_speak_w(PRISM_BACKEND_NVDA, text, interrupt);
}
export int nvdaBraille(const wchar_t *text) {
  return us_id_braille_w(PRISM_BACKEND_NVDA, text);
}
export int nvdaStopSpeech(void) { return us_id_stop(PRISM_BACKEND_NVDA); }
export int nvdaGetRunningVersion(char *buf, int size) {
  if (buf != PRISM_SHIM_NULL && size > 0)
    buf[0] = '\0';
  return 0;
}

/* Window-Eyes */
export int weLoad(void) { return us_id_available(PRISM_BACKEND_WINDOW_EYES); }
export void weUnload(void) { us_id_unload(PRISM_BACKEND_WINDOW_EYES); }
export int weIsAvailable(void) {
  return us_id_available(PRISM_BACKEND_WINDOW_EYES);
}
export int weSayA(const char *text) {
  return us_id_speak_a(PRISM_BACKEND_WINDOW_EYES, text, 0);
}
export int weSayW(const wchar_t *text) {
  return us_id_speak_w(PRISM_BACKEND_WINDOW_EYES, text, 0);
}
export int weBrailleA(const char *text) {
  return us_id_braille_a(PRISM_BACKEND_WINDOW_EYES, text);
}
export int weBrailleW(const wchar_t *text) {
  return us_id_braille_w(PRISM_BACKEND_WINDOW_EYES, text);
}
export int weStopSpeech(void) { return us_id_stop(PRISM_BACKEND_WINDOW_EYES); }

/* System Access */
export int saLoad(void) { return us_id_available(PRISM_BACKEND_SYSTEM_ACCESS); }
export void saUnload(void) { us_id_unload(PRISM_BACKEND_SYSTEM_ACCESS); }
export int saIsAvailable(void) {
  return us_id_available(PRISM_BACKEND_SYSTEM_ACCESS);
}
export int saSayA(const char *text) {
  return us_id_speak_a(PRISM_BACKEND_SYSTEM_ACCESS, text, 0);
}
export int saSayW(const wchar_t *text) {
  return us_id_speak_w(PRISM_BACKEND_SYSTEM_ACCESS, text, 0);
}
export int saBrailleA(const char *text) {
  return us_id_braille_a(PRISM_BACKEND_SYSTEM_ACCESS, text);
}
export int saBrailleW(const wchar_t *text) {
  return us_id_braille_w(PRISM_BACKEND_SYSTEM_ACCESS, text);
}
export int saStopSpeech(void) {
  return us_id_stop(PRISM_BACKEND_SYSTEM_ACCESS);
}

/* Dolphin/SuperNova and Cobra have no Prism backend. */
export int dolLoad(void) { return 0; }
export void dolUnload(void) {}
export int dolIsAvailable(void) { return 0; }
export int dolSay(const wchar_t *text) {
  (void)text;
  return 0;
}
export int dolStopSpeech(void) { return 0; }
export int cbrLoad(void) { return 0; }
export void cbrUnload(void) {}
export int cbrIsAvailable(void) { return 0; }
export int cbrSayA(const char *text) {
  (void)text;
  return 0;
}
export int cbrSayW(const wchar_t *text) {
  (void)text;
  return 0;
}
export int cbrBrailleA(const char *text) {
  (void)text;
  return 0;
}
export int cbrBrailleW(const wchar_t *text) {
  (void)text;
  return 0;
}
export int cbrStopSpeech(void) { return 0; }

/* ZoomText */
export int ztLoad(void) { return us_id_available(PRISM_BACKEND_ZOOM_TEXT); }
export int ztUnload(void) {
  us_id_unload(PRISM_BACKEND_ZOOM_TEXT);
  return 1;
}
export int ztIsAvailable(void) {
  return us_id_available(PRISM_BACKEND_ZOOM_TEXT);
}
export int ztIsActive(void) { return ztIsAvailable(); }
export int ztSayW(const wchar_t *text, int interrupt) {
  return us_id_speak_w(PRISM_BACKEND_ZOOM_TEXT, text, interrupt);
}
export int ztStopSpeech(void) { return us_id_stop(PRISM_BACKEND_ZOOM_TEXT); }

/* ZDSR */
export int zdsrLoad(void) { return us_id_available(PRISM_BACKEND_ZDSR); }
export void zdsrUnload(void) { us_id_unload(PRISM_BACKEND_ZDSR); }
export int zdsrIsAvailable(void) { return us_id_available(PRISM_BACKEND_ZDSR); }
export int zdsrSayW(const wchar_t *text, int interrupt) {
  return us_id_speak_w(PRISM_BACKEND_ZDSR, text, interrupt);
}
export int zdsrStopSpeech(void) { return us_id_stop(PRISM_BACKEND_ZDSR); }
export int zdsrIsSpeaking(void) {
  fast_lock_acquire(&us_lock);
  const int index = us_index_for_id(PRISM_BACKEND_ZDSR);
  PrismBackend *backend =
      index < 0 ? PRISM_SHIM_NULL : us_backend_locked(index);
  bool speaking = false;
  const int result =
      backend != PRISM_SHIM_NULL &&
      prism_backend_is_speaking(backend, &speaking) == PRISM_OK && speaking;
  fast_lock_release(&us_lock);
  return result;
}

export int narIsAvailable(void) { return us_id_available(PRISM_BACKEND_UIA); }

static PrismBackend *us_native_locked(void) {
  return us_backend_locked(US_NATIVE_INDEX);
}

export int sapiLoad(void) {
  fast_lock_acquire(&us_lock);
  const int result = us_native_locked() != PRISM_SHIM_NULL;
  fast_lock_release(&us_lock);
  return result;
}

export void sapiUnload(void) { us_id_unload(shim_native_tts_id()); }

export int sapiIsAvailable(void) {
  return us_id_available(shim_native_tts_id());
}
static int us_sapi_say_w_locked(const wchar_t *text, int interrupt) {
  PrismBackend *backend = us_native_locked();
  if (backend == PRISM_SHIM_NULL || text == PRISM_SHIM_NULL)
    return 0;
  if (us_wave_callback == PRISM_SHIM_NULL)
    return (int)us_speak_w_locked(US_NATIVE_INDEX, text, interrupt != 0);
  char *utf8 = shim_wchar_to_utf8(text);
  ShimAudioBuffer audio;
  shim_audio_buffer_init(&audio);
  int16_t *pcm = PRISM_SHIM_NULL;
  size_t sample_count = 0;
  int result = 0;
  if (utf8 != PRISM_SHIM_NULL && shim_synthesize_audio(backend, utf8, &audio) &&
      shim_audio_to_pcm16_mono(&audio, (size_t)us_wave_sample_rate, &pcm,
                               &sample_count) &&
      sample_count <= (size_t)INT_MAX / sizeof(int16_t)) {
    const int bytes = (int)(sample_count * sizeof(int16_t));
    (void)us_wave_callback(us_wave_userdata, pcm, bytes);
    result = 1;
  }
  free(pcm);
  shim_audio_buffer_clear(&audio);
  free(utf8);
  return result;
}

export int sapiSayW(const wchar_t *text, int interrupt) {
  fast_lock_acquire(&us_lock);
  const int result = us_sapi_say_w_locked(text, interrupt);
  fast_lock_release(&us_lock);
  return result;
}
export int sapiSaySSMLW(const wchar_t *text, int interrupt) {
  return sapiSayW(text, interrupt);
}

export int sapiSayA(const char *text, int interrupt) {
  char *utf8 = shim_ansi_to_utf8(text);
  wchar_t *wide = shim_utf8_to_wchar(utf8);
  const int result = wide == PRISM_SHIM_NULL ? 0 : sapiSayW(wide, interrupt);
  free(wide);
  free(utf8);
  return result;
}

export int sapiSaySSMLA(const char *text, int interrupt) {
  return sapiSayA(text, interrupt);
}

export int sapiStopSpeech(void) { return us_id_stop(shim_native_tts_id()); }

export int sapiSetPaused(int paused) {
  fast_lock_acquire(&us_lock);
  PrismBackend *backend = us_native_locked();
  PrismError error = PRISM_ERROR_INVALID_OPERATION;
  if (backend != PRISM_SHIM_NULL) {
    error = paused != 0 ? prism_backend_pause(backend)
                        : prism_backend_resume(backend);
  }
  const int result = error == PRISM_OK;
  if (result)
    us_slots[US_NATIVE_INDEX].paused = paused != 0;
  fast_lock_release(&us_lock);
  return result;
}

export int sapiIsPaused(void) {
  fast_lock_acquire(&us_lock);
  const int result = (int)us_slots[US_NATIVE_INDEX].paused;
  fast_lock_release(&us_lock);
  return result;
}
export int sapiWait(int timeout) {
  (void)timeout;
  return 0;
}
export int sapiIsSpeaking(void) {
  fast_lock_acquire(&us_lock);
  PrismBackend *backend = us_native_locked();
  bool speaking = false;
  const int result =
      backend != PRISM_SHIM_NULL &&
      prism_backend_is_speaking(backend, &speaking) == PRISM_OK && speaking;
  fast_lock_release(&us_lock);
  return result;
}

export int sapiSetRate(int rate) {
  fast_lock_acquire(&us_lock);
  PrismBackend *backend = us_native_locked();
  const int result =
      backend != PRISM_SHIM_NULL &&
      prism_backend_set_rate(backend, shim_clamp01((float)rate / 100.0F)) ==
          PRISM_OK;
  fast_lock_release(&us_lock);
  return result;
}

export int sapiGetRate(void) {
  fast_lock_acquire(&us_lock);
  PrismBackend *backend = us_native_locked();
  float rate = 0.0F;
  const int result = backend != PRISM_SHIM_NULL &&
                             prism_backend_get_rate(backend, &rate) == PRISM_OK
                         ? (int)(shim_clamp01(rate) * 100.0F)
                         : -1;
  fast_lock_release(&us_lock);
  return result;
}

export int sapiSetVolume(int volume) {
  fast_lock_acquire(&us_lock);
  PrismBackend *backend = us_native_locked();
  const int result =
      backend != PRISM_SHIM_NULL &&
      prism_backend_set_volume(backend, shim_clamp01((float)volume / 100.0F)) ==
          PRISM_OK;
  fast_lock_release(&us_lock);
  return result;
}

export int sapiGetVolume(void) {
  fast_lock_acquire(&us_lock);
  PrismBackend *backend = us_native_locked();
  float volume = 0.0F;
  const int result =
      backend != PRISM_SHIM_NULL &&
              prism_backend_get_volume(backend, &volume) == PRISM_OK
          ? (int)(shim_clamp01(volume) * 100.0F)
          : -1;
  fast_lock_release(&us_lock);
  return result;
}

export int sapiGetNumVoices(void) {
  fast_lock_acquire(&us_lock);
  PrismBackend *backend = us_native_locked();
  size_t count = 0;
  const int result =
      backend != PRISM_SHIM_NULL &&
              prism_backend_count_voices(backend, &count) == PRISM_OK &&
              count <= (size_t)INT_MAX
          ? (int)count
          : -1;
  fast_lock_release(&us_lock);
  return result;
}

export int sapiSetVoice(int voice) {
  fast_lock_acquire(&us_lock);
  PrismBackend *backend = us_native_locked();
  const int result =
      backend != PRISM_SHIM_NULL && voice >= 0 &&
      prism_backend_set_voice(backend, (size_t)voice) == PRISM_OK;
  fast_lock_release(&us_lock);
  return result;
}

export int sapiGetVoice(void) {
  fast_lock_acquire(&us_lock);
  PrismBackend *backend = us_native_locked();
  size_t voice = 0;
  const int result =
      backend != PRISM_SHIM_NULL &&
              prism_backend_get_voice(backend, &voice) == PRISM_OK &&
              voice <= (size_t)INT_MAX
          ? (int)voice
          : -1;
  fast_lock_release(&us_lock);
  return result;
}

export const wchar_t *sapiGetVoiceNameW(int voice) {
  fast_lock_acquire(&us_lock);
  PrismBackend *backend = us_native_locked();
  const char *name = PRISM_SHIM_NULL;
  if (backend != PRISM_SHIM_NULL && voice >= 0)
    shim_ignore_error(
        prism_backend_get_voice_name(backend, (size_t)voice, &name));
  free(us_wide_result);
  us_wide_result =
      name == PRISM_SHIM_NULL ? PRISM_SHIM_NULL : shim_utf8_to_wchar(name);
  const wchar_t *result = us_wide_result;
  fast_lock_release(&us_lock);
  return result;
}

export const char *sapiGetVoiceNameA(int voice) {
  return us_convert_result(sapiGetVoiceNameW(voice), false);
}
export int sapiGetValue(int what) {
  switch (what) {
  case SP_RATE:
    return sapiGetRate();
  case SP_VOLUME:
    return sapiGetVolume();
  case SP_VOICE:
    return sapiGetVoice();
  case SP_PAUSED:
    return sapiIsPaused();
  case SP_BUSY:
    return sapiIsSpeaking();
  case SP_WAIT:
    return sapiWait(-1);
  case SP_RATE_MIN:
  case SP_VOLUME_MIN:
    return 0;
  case SP_RATE_MAX:
  case SP_VOLUME_MAX:
    return 100;
  case SP_RATE_SUPPORTED:
  case SP_VOLUME_SUPPORTED:
  case SP_PAUSE_SUPPORTED:
  case SP_BUSY_SUPPORTED:
    return 1;
  case SP_WAIT_SUPPORTED:
  default:
    return 0;
  }
}

export int sapiSetValue(int what, int value) {
  switch (what) {
  case SP_RATE:
    return sapiSetRate(value);
  case SP_VOLUME:
    return sapiSetVolume(value);
  case SP_VOICE:
    return sapiSetVoice(value);
  case SP_PAUSED:
    return sapiSetPaused(value);
  case SP_WAIT:
    return sapiWait(value);
  default:
    return 0;
  }
}
export const wchar_t *sapiGetString(int what) {
  if (what >= SP_VOICE && what <= SP_VOICE + 0xFFFF)
    return sapiGetVoiceNameW(what - SP_VOICE);
  return PRISM_SHIM_NULL;
}

export int sapiSetOutputCallback(int sample_rate, UsWaveCallback callback,
                                 void *userdata) {
  fast_lock_acquire(&us_lock);
  if (sample_rate <= 0 || callback == PRISM_SHIM_NULL) {
    us_wave_callback = PRISM_SHIM_NULL;
    us_wave_userdata = PRISM_SHIM_NULL;
    us_wave_sample_rate = 0;
    const int result = us_native_locked() != PRISM_SHIM_NULL;
    fast_lock_release(&us_lock);
    return result;
  }
  if (us_native_locked() == PRISM_SHIM_NULL) {
    fast_lock_release(&us_lock);
    return 0;
  }
  us_wave_callback = callback;
  us_wave_userdata = userdata;
  us_wave_sample_rate = sample_rate;
  fast_lock_release(&us_lock);
  return 1;
}

/* ScreenReaderAPI.dll compatibility aliases. */
US_EXPORT_STDCALL int sayStringA(const char *text, int interrupt) {
  return speechSayA(text, interrupt);
}
US_EXPORT_STDCALL int sayStringW(const wchar_t *text, int interrupt) {
  return speechSay(text, interrupt);
}
US_EXPORT_STDCALL int brailleMessageA(const char *text) {
  return brailleDisplayA(text);
}
US_EXPORT_STDCALL int brailleMessageW(const wchar_t *text) {
  return brailleDisplay(text);
}
US_EXPORT_STDCALL int stopSpeech(void) { return speechStop(); }
US_EXPORT_STDCALL int getCurrentScreenReader(void) {
  return speechGetValue(SP_ENGINE);
}
US_EXPORT_STDCALL int setCurrentScreenReader(int index) {
  return speechSetValue(SP_ENGINE, index);
}
US_EXPORT_STDCALL const wchar_t *getScreenReaderNameW(int index) {
  return speechGetString(SP_ENGINE + index);
}
US_EXPORT_STDCALL const char *getScreenReaderNameA(int index) {
  return speechGetStringA(SP_ENGINE + index);
}
US_EXPORT_STDCALL const wchar_t *getCurrentScreenReaderNameW(void) {
  return getScreenReaderNameW(getCurrentScreenReader());
}
US_EXPORT_STDCALL const char *getCurrentScreenReaderNameA(void) {
  return getScreenReaderNameA(getCurrentScreenReader());
}
US_EXPORT_STDCALL int getScreenReaderIdW(const wchar_t *name) {
  if (name == PRISM_SHIM_NULL)
    return -1;
  for (int i = 0; i < US_ENGINE_COUNT; ++i)
    if (wcscmp(name, us_slots[i].name) == 0)
      return i;
  return -1;
}
US_EXPORT_STDCALL int getScreenReaderIdA(const char *name) {
  char *utf8 = shim_ansi_to_utf8(name);
  wchar_t *wide = shim_utf8_to_wchar(utf8);
  const int result = wide == PRISM_SHIM_NULL ? -1 : getScreenReaderIdW(wide);
  free(wide);
  free(utf8);
  return result;
}
US_EXPORT_STDCALL int setCurrentScreenReaderNameW(const wchar_t *name) {
  return setCurrentScreenReader(getScreenReaderIdW(name));
}
US_EXPORT_STDCALL int setCurrentScreenReaderNameA(const char *name) {
  return setCurrentScreenReader(getScreenReaderIdA(name));
}
US_EXPORT_STDCALL int getSupportedScreenReadersCount(void) {
  return US_ENGINE_COUNT;
}
US_EXPORT_STDCALL int sapiIsEnabled(void) {
  return speechGetValue(SP_ENABLE_NATIVE_SPEECH);
}
US_EXPORT_STDCALL int sapiEnable(int enable) {
  return speechSetValue(SP_ENABLE_NATIVE_SPEECH, enable);
}
int US_STDCALL sapiGetRate2(void) { return sapiGetRate(); }
int US_STDCALL sapiSetRate2(int rate) { return sapiSetRate(rate); }

/* Legacy peripheral helpers intentionally remain inert. */
export int installKeyboardHook(void) { return 0; }
export void uninstallKeyboardHook(void) {}
export int FindProcess(const char *needle, char *buf, size_t size) {
  (void)needle;
  if (buf != PRISM_SHIM_NULL && size != 0)
    buf[0] = '\0';
  return 0;
}
export void *getProcessHandle(const char *needle, char *buf, size_t size) {
  (void)needle;
  if (buf != PRISM_SHIM_NULL && size != 0)
    buf[0] = '\0';
  return PRISM_SHIM_NULL;
}

/* JNI entry points remain present without imposing a JVM build dependency. */
#ifdef _WIN32
#define US_JNI_EXPORT __declspec(dllexport)
#define US_JNI_CALL __stdcall
#else
#define US_JNI_EXPORT __attribute__((visibility("default")))
#define US_JNI_CALL
#endif
US_JNI_EXPORT unsigned char US_JNI_CALL Java_quentinc_UniversalSpeech_say(
    void *env, void *clazz, void *text, unsigned char interrupt) {
  (void)env;
  (void)clazz;
  (void)text;
  (void)interrupt;
  return 0;
}
US_JNI_EXPORT unsigned char US_JNI_CALL
Java_quentinc_UniversalSpeech_braille(void *env, void *clazz, void *text) {
  (void)env;
  (void)clazz;
  (void)text;
  return 0;
}
US_JNI_EXPORT unsigned char US_JNI_CALL
Java_quentinc_UniversalSpeech_stop(void *env, void *clazz) {
  (void)env;
  (void)clazz;
  return (unsigned char)speechStop();
}
US_JNI_EXPORT unsigned char US_JNI_CALL Java_quentinc_UniversalSpeech_setValue(
    void *env, void *clazz, int what, int value) {
  (void)env;
  (void)clazz;
  return (unsigned char)speechSetValue(what, value);
}
US_JNI_EXPORT int US_JNI_CALL
Java_quentinc_UniversalSpeech_getValue(void *env, void *clazz, int what) {
  (void)env;
  (void)clazz;
  return speechGetValue(what);
}
US_JNI_EXPORT unsigned char US_JNI_CALL Java_quentinc_UniversalSpeech_setString(
    void *env, void *clazz, int what, void *value) {
  (void)env;
  (void)clazz;
  (void)what;
  (void)value;
  return 0;
}
US_JNI_EXPORT void *US_JNI_CALL
Java_quentinc_UniversalSpeech_getString(void *env, void *clazz, int what) {
  (void)env;
  (void)clazz;
  (void)what;
  return PRISM_SHIM_NULL;
}
