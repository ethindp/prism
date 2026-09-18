// SPDX-License-Identifier: MPL-2.0
#define UNIVERSAL_SPEECH_BUILDING
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
#else
#define US_STDCALL
#endif

#define US_EXPORT(type) UNIVERSAL_SPEECH_API type UNIVERSAL_SPEECH_CALL
#define US_EXPORT_STDCALL(type) UNIVERSAL_SPEECH_API type US_STDCALL

typedef struct UsSlot {
  const wchar_t *name;
  PrismBackendId prism_id;
  PrismBackend *backend;
  bool automatic;
  bool paused;
} UsSlot;

typedef enum UsOp { US_OP_SAY, US_OP_BRAILLE, US_OP_STOP } UsOp;
typedef enum UsNarrowEncoding {
  US_NARROW_ANSI,
  US_NARROW_UTF8
} UsNarrowEncoding;

static UsSlot us_slots[] = {
    {L"Jaws", PRISM_BACKEND_JAWS, PRISM_SHIM_NULL, true, false},
    {L"Windows eye", PRISM_BACKEND_WINDOW_EYES, PRISM_SHIM_NULL, true, false},
    {L"NVDA", PRISM_BACKEND_NVDA, PRISM_SHIM_NULL, true, false},
    {L"System access", PRISM_BACKEND_SYSTEM_ACCESS, PRISM_SHIM_NULL, true,
     false},
    {L"Supernova", PRISM_BACKEND_INVALID, PRISM_SHIM_NULL, false, false},
    {L"ZoomText", PRISM_BACKEND_ZOOM_TEXT, PRISM_SHIM_NULL, true, false},
    {L"ZDSR", PRISM_BACKEND_ZDSR, PRISM_SHIM_NULL, true, false},
    {L"Cobra", PRISM_BACKEND_INVALID, PRISM_SHIM_NULL, false, false},
    {L"Narrator", PRISM_BACKEND_UIA, PRISM_SHIM_NULL, true, false},
    {L"SAPI5", PRISM_BACKEND_INVALID, PRISM_SHIM_NULL, true, false},
};

enum { US_ENGINE_COUNT = (int)(sizeof(us_slots) / sizeof(us_slots[0])) };
enum { US_NATIVE_INDEX = US_ENGINE_COUNT - 1 };

typedef int (*UsWaveCallback)(void *, void *, int);

static fast_lock us_lock = FAST_LOCK_INIT;
static ShimFlag us_stale;
static PrismContext *us_ctx;
static ShimStrings us_strings;
static int us_current = -1;
static int us_auto_engine = 1;
static int us_native_enabled = 1;
static UsWaveCallback us_wave_callback;
static void *us_wave_userdata;
static int us_wave_sample_rate;

static bool us_index_valid(int index) {
  return (bool)(index >= 0 && index < US_ENGINE_COUNT);
}

static PrismBackendId us_slot_id(int index) {
  if (!us_index_valid(index))
    return PRISM_BACKEND_INVALID;
  if (index == US_NATIVE_INDEX)
    return shim_native_tts_id();
  return us_slots[index].prism_id;
}

static bool us_ensure_context_locked(void) {
  if (us_ctx == PRISM_SHIM_NULL)
    us_ctx = shim_context_open(&us_stale);
  return us_ctx != PRISM_SHIM_NULL;
}

static PrismBackend *us_backend_locked(int index) {
  if (!us_index_valid(index) || !us_ensure_context_locked())
    return PRISM_SHIM_NULL;
  return shim_slot_get(us_ctx, us_slot_id(index), &us_slots[index].backend);
}

static bool us_available_locked(int index) {
  if (!us_index_valid(index) ||
      (index == US_NATIVE_INDEX && !us_native_enabled) ||
      !us_ensure_context_locked())
    return false;
  return shim_slot_live(us_ctx, us_slot_id(index), &us_slots[index].backend);
}

static void us_drop_locked(int index) {
  if (!us_index_valid(index))
    return;
  shim_slot_drop(&us_slots[index].backend);
  us_slots[index].paused = false;
}

static int us_detect_locked(void) {
  for (int i = 0; i < US_ENGINE_COUNT; ++i) {
    if (us_slots[i].automatic && us_available_locked(i))
      return i;
  }
  return -1;
}

static int us_resolve_current_locked(void) {
  if (shim_flag_take(&us_stale) && us_auto_engine)
    us_current = -1;
  if (us_current < 0 || us_current >= US_ENGINE_COUNT)
    us_current = us_detect_locked();
  return us_current;
}

static PrismError us_apply(PrismBackend *backend, UsOp op, const char *text,
                           bool interrupt) {
  if (backend == PRISM_SHIM_NULL)
    return PRISM_ERROR_BACKEND_NOT_AVAILABLE;
  switch (op) {
  case US_OP_SAY:
    return prism_backend_speak(backend, text, interrupt);
  case US_OP_BRAILLE:
    return prism_backend_braille(backend, text);
  case US_OP_STOP:
    return prism_backend_stop(backend);
  }
  return PRISM_ERROR_INVALID_PARAM;
}

static int us_run_current(UsOp op, const wchar_t *text, int interrupt) {
  char *utf8 = PRISM_SHIM_NULL;
  if (op != US_OP_STOP) {
    if (text == PRISM_SHIM_NULL)
      return 0;
    utf8 = shim_wchar_to_utf8(text);
    if (utf8 == PRISM_SHIM_NULL)
      return 0;
  }
  fast_lock_acquire(&us_lock);
  int index = us_resolve_current_locked();
  PrismError error = PRISM_ERROR_BACKEND_NOT_AVAILABLE;
  if (index >= 0) {
    error = us_apply(us_backend_locked(index), op, utf8, interrupt != 0);
    if (shim_error_means_lost(error)) {
      us_drop_locked(index);
      us_current = -1;
      index = us_resolve_current_locked();
      if (index >= 0)
        error = us_apply(us_backend_locked(index), op, utf8, interrupt != 0);
    }
  }
  const int result =
      index >= 0 && (error == PRISM_OK || !shim_error_means_lost(error));
  fast_lock_release(&us_lock);
  free(utf8);
  return result;
}

static int us_run_index(int index, UsOp op, const wchar_t *text,
                        int interrupt) {
  char *utf8 = PRISM_SHIM_NULL;
  if (op != US_OP_STOP) {
    if (text == PRISM_SHIM_NULL)
      return 0;
    utf8 = shim_wchar_to_utf8(text);
    if (utf8 == PRISM_SHIM_NULL)
      return 0;
  }
  fast_lock_acquire(&us_lock);
  PrismError error = PRISM_ERROR_BACKEND_NOT_AVAILABLE;
  if (index >= 0) {
    error = us_apply(us_backend_locked(index), op, utf8, interrupt != 0);
    if (shim_error_means_lost(error))
      us_drop_locked(index);
  }
  fast_lock_release(&us_lock);
  free(utf8);
  return error == PRISM_OK;
}

static const char *us_narrow(const wchar_t *value, UsNarrowEncoding encoding) {
  if (value == PRISM_SHIM_NULL)
    return PRISM_SHIM_NULL;
  char *encoded = shim_wchar_to_utf8(value);
  if (encoded == PRISM_SHIM_NULL)
    return PRISM_SHIM_NULL;
  fast_lock_acquire(&us_lock);
  const char *result = PRISM_SHIM_NULL;
  if (encoding == US_NARROW_UTF8)
    result = shim_intern_utf8(&us_strings, encoded);
  else
    result = shim_intern_ansi(&us_strings, encoded);
  fast_lock_release(&us_lock);
  free(encoded);
  return result;
}

US_EXPORT(int) speechSay(const ____wchar_t *text, int interrupt) {
  return us_run_current(US_OP_SAY, text, interrupt);
}

US_EXPORT(int) speechStop(void) {
  return us_run_current(US_OP_STOP, PRISM_SHIM_NULL, 0);
}

US_EXPORT(int) brailleDisplay(const ____wchar_t *text) {
  return us_run_current(US_OP_BRAILLE, text, 0);
}

static bool us_get_float_locked(PrismBackend *backend, int what, float *out) {
  if (backend == PRISM_SHIM_NULL)
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

static uint64_t us_getter_feature(int what) {
  switch (what) {
  case SP_VOLUME:
    return PRISM_BACKEND_SUPPORTS_GET_VOLUME;
  case SP_RATE:
    return PRISM_BACKEND_SUPPORTS_GET_RATE;
  case SP_PITCH:
    return PRISM_BACKEND_SUPPORTS_GET_PITCH;
  default:
    return 0;
  }
}

static int us_get_value_locked(int what) {
  switch (what) {
  case SP_ENGINE:
    return us_resolve_current_locked();
  case SP_ENABLE_NATIVE_SPEECH:
    return us_native_enabled;
  case SP_AUTO_ENGINE:
    return us_auto_engine;
  default:
    break;
  }
  if (what >= SP_ENGINE_AVAILABLE &&
      what < SP_ENGINE_AVAILABLE + US_ENGINE_COUNT)
    return (int)us_available_locked(what - SP_ENGINE_AVAILABLE);
  const int index = us_resolve_current_locked();
  PrismBackend *backend = us_backend_locked(index);
  const uint64_t features =
      backend == PRISM_SHIM_NULL ? 0 : prism_backend_get_features(backend);
  switch (what) {
  case SP_BUSY: {
    bool speaking = false;
    return backend != PRISM_SHIM_NULL &&
           prism_backend_is_speaking(backend, &speaking) == PRISM_OK &&
           speaking;
  }
  case SP_BUSY_SUPPORTED:
    return (features & PRISM_BACKEND_SUPPORTS_IS_SPEAKING) != 0;
  case SP_PAUSED:
    if (!us_index_valid(index))
      return 0;
    return (int)us_slots[index].paused;
  case SP_PAUSE_SUPPORTED:
    return (features & PRISM_BACKEND_SUPPORTS_PAUSE) != 0 &&
           (features & PRISM_BACKEND_SUPPORTS_RESUME) != 0;
  case SP_VOLUME_MIN:
  case SP_RATE_MIN:
  case SP_PITCH_MIN:
    return 0;
  case SP_VOLUME_MAX:
  case SP_RATE_MAX:
  case SP_PITCH_MAX:
    return 100;
  case SP_VOLUME_SUPPORTED:
    return (features & us_getter_feature(SP_VOLUME)) != 0;
  case SP_RATE_SUPPORTED:
    return (features & us_getter_feature(SP_RATE)) != 0;
  case SP_PITCH_SUPPORTED:
    return (features & us_getter_feature(SP_PITCH)) != 0;
  case SP_VOICE: {
    size_t voice = 0;
    return backend != PRISM_SHIM_NULL &&
                   prism_backend_get_voice(backend, &voice) == PRISM_OK &&
                   voice <= (size_t)INT_MAX
               ? (int)voice
               : -1;
  }
  default: {
    float value = 0.0F;
    if (!us_get_float_locked(backend, what, &value))
      return 0;
    return (int)((shim_clamp01(value) * 100.0F) + 0.5F);
  }
  }
}

US_EXPORT(int) speechGetValue(int what) {
  fast_lock_acquire(&us_lock);
  const int result = us_get_value_locked(what);
  fast_lock_release(&us_lock);
  return result;
}

static int us_set_value_locked(int what, int value) {
  if (what == SP_ENGINE) {
    if (value < 0) {
      us_current = -1;
      us_auto_engine = 1;
      return 1;
    }
    if (value < US_ENGINE_COUNT) {
      us_current = value;
      us_auto_engine = 0;
      return 1;
    }
    return 0;
  }
  if (what == SP_ENABLE_NATIVE_SPEECH) {
    us_native_enabled = value != 0;
    if (!us_native_enabled && us_current == US_NATIVE_INDEX)
      us_current = -1;
    return us_native_enabled;
  }
  const int index = us_resolve_current_locked();
  PrismBackend *backend = us_backend_locked(index);
  if (backend == PRISM_SHIM_NULL)
    return 0;
  const float normalized = shim_to_unit((float)value, 0.0F, 100.0F);
  switch (what) {
  case SP_PAUSED: {
    const PrismError error = value != 0 ? prism_backend_pause(backend)
                                        : prism_backend_resume(backend);
    if (error != PRISM_OK)
      return 0;
    us_slots[index].paused = value != 0;
    return 1;
  }
  case SP_VOICE:
    return value >= 0 &&
           prism_backend_set_voice(backend, (size_t)value) == PRISM_OK;
  case SP_VOLUME:
    return prism_backend_set_volume(backend, normalized) == PRISM_OK;
  case SP_RATE:
    return prism_backend_set_rate(backend, normalized) == PRISM_OK;
  case SP_PITCH:
    return prism_backend_set_pitch(backend, normalized) == PRISM_OK;
  default:
    return 0;
  }
}

US_EXPORT(int) speechSetValue(int what, int value) {
  fast_lock_acquire(&us_lock);
  const int result = us_set_value_locked(what, value);
  fast_lock_release(&us_lock);
  return result;
}

US_EXPORT(const ____wchar_t *) speechGetString(int what) {
  if (what >= SP_ENGINE && what < SP_ENGINE + US_ENGINE_COUNT)
    return us_slots[what - SP_ENGINE].name;
  fast_lock_acquire(&us_lock);
  const wchar_t *result = PRISM_SHIM_NULL;
  if (what >= SP_VOICE && what < SP_VOICE + 0x10000) {
    PrismBackend *backend = us_backend_locked(us_resolve_current_locked());
    const char *name = PRISM_SHIM_NULL;
    if (backend != PRISM_SHIM_NULL &&
        prism_backend_get_voice_name(backend, (size_t)(what - SP_VOICE),
                                     &name) == PRISM_OK)
      result = shim_intern_wide(&us_strings, name);
  }
  fast_lock_release(&us_lock);
  return result;
}

US_EXPORT(int) speechSetString(int what, const ____wchar_t *value) {
  if (value == PRISM_SHIM_NULL || what != SP_VOICE)
    return 0;
  char *wanted = shim_wchar_to_utf8(value);
  if (wanted == PRISM_SHIM_NULL)
    return 0;
  fast_lock_acquire(&us_lock);
  PrismBackend *backend = us_backend_locked(us_resolve_current_locked());
  size_t count = 0;
  int result = 0;
  if (backend != PRISM_SHIM_NULL &&
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
  fast_lock_release(&us_lock);
  free(wanted);
  return result;
}

US_EXPORT(int) speechSayA(const char *text, int interrupt) {
  char *utf8 = shim_ansi_to_utf8(text);
  wchar_t *wide = shim_utf8_to_wchar(utf8);
  const int result = wide == PRISM_SHIM_NULL ? 0 : speechSay(wide, interrupt);
  free(wide);
  free(utf8);
  return result;
}

US_EXPORT(int) brailleDisplayA(const char *text) {
  char *utf8 = shim_ansi_to_utf8(text);
  wchar_t *wide = shim_utf8_to_wchar(utf8);
  const int result = wide == PRISM_SHIM_NULL ? 0 : brailleDisplay(wide);
  free(wide);
  free(utf8);
  return result;
}

US_EXPORT(const char *) speechGetStringA(int what) {
  return us_narrow(speechGetString(what), US_NARROW_ANSI);
}

US_EXPORT(int) speechSetStringA(int what, const char *value) {
  char *utf8 = shim_ansi_to_utf8(value);
  wchar_t *wide = shim_utf8_to_wchar(utf8);
  const int result = wide == PRISM_SHIM_NULL ? 0 : speechSetString(what, wide);
  free(wide);
  free(utf8);
  return result;
}

US_EXPORT(int) speechSayU(const char *text, int interrupt) {
  wchar_t *wide = shim_utf8_to_wchar(text);
  const int result = wide == PRISM_SHIM_NULL ? 0 : speechSay(wide, interrupt);
  free(wide);
  return result;
}

US_EXPORT(int) brailleDisplayU(const char *text) {
  wchar_t *wide = shim_utf8_to_wchar(text);
  const int result = wide == PRISM_SHIM_NULL ? 0 : brailleDisplay(wide);
  free(wide);
  return result;
}

US_EXPORT(const char *) speechGetStringU(int what) {
  return us_narrow(speechGetString(what), US_NARROW_UTF8);
}

US_EXPORT(int) speechSetStringU(int what, const char *value) {
  wchar_t *wide = shim_utf8_to_wchar(value);
  const int result = wide == PRISM_SHIM_NULL ? 0 : speechSetString(what, wide);
  free(wide);
  return result;
}

#ifndef _WIN32
US_EXPORT(int) speechSetStringW(int what, const wchar_t *value) {
  return speechSetString(what, value);
}

US_EXPORT(int) brailleDisplayW(const wchar_t *text) {
  return brailleDisplay(text);
}
#endif

static int us_index_for_id(PrismBackendId id) {
  if (id == shim_native_tts_id())
    return US_NATIVE_INDEX;
  for (int i = 0; i < US_NATIVE_INDEX; ++i)
    if (us_slots[i].prism_id == id)
      return i;
  return -1;
}

static int us_id_available(PrismBackendId id) {
  fast_lock_acquire(&us_lock);
  const int result = (int)us_available_locked(us_index_for_id(id));
  fast_lock_release(&us_lock);
  return result;
}

static int us_id_speak_w(PrismBackendId id, const wchar_t *text,
                         int interrupt) {
  return us_run_index(us_index_for_id(id), US_OP_SAY, text, interrupt);
}

static int us_id_braille_w(PrismBackendId id, const wchar_t *text) {
  return us_run_index(us_index_for_id(id), US_OP_BRAILLE, text, 0);
}

static int us_id_stop(PrismBackendId id) {
  return us_run_index(us_index_for_id(id), US_OP_STOP, PRISM_SHIM_NULL, 0);
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
  if (index >= 0) {
    us_drop_locked(index);
    if (us_current == index && us_auto_engine)
      us_current = -1;
  }
  fast_lock_release(&us_lock);
}

US_EXPORT(int) jfwLoad(void) { return us_id_available(PRISM_BACKEND_JAWS); }
US_EXPORT(void) jfwUnload(void) { us_id_unload(PRISM_BACKEND_JAWS); }
US_EXPORT(int) jfwIsAvailable(void) {
  return us_id_available(PRISM_BACKEND_JAWS);
}
US_EXPORT(int) jfwSayA(const char *text, int interrupt) {
  return us_id_speak_a(PRISM_BACKEND_JAWS, text, interrupt);
}
US_EXPORT(int) jfwSayW(const wchar_t *text, int interrupt) {
  return us_id_speak_w(PRISM_BACKEND_JAWS, text, interrupt);
}
US_EXPORT(int) jfwBrailleA(const char *text) {
  return us_id_braille_a(PRISM_BACKEND_JAWS, text);
}
US_EXPORT(int) jfwBrailleW(const wchar_t *text) {
  return us_id_braille_w(PRISM_BACKEND_JAWS, text);
}
US_EXPORT(int) jfwStopSpeech(void) { return us_id_stop(PRISM_BACKEND_JAWS); }
US_EXPORT(int) jfwRunScriptA(const char *name) {
  (void)name;
  return 0;
}
US_EXPORT(int) jfwRunScriptW(const wchar_t *name) {
  (void)name;
  return 0;
}
US_EXPORT(int) jfwRunFunctionA(const char *name) {
  (void)name;
  return 0;
}
US_EXPORT(int) jfwRunFunctionW(const wchar_t *name) {
  (void)name;
  return 0;
}
US_EXPORT(int) jfwGetUserSettingsDirectory(char *buf, size_t size) {
  if (buf != PRISM_SHIM_NULL && size != 0)
    buf[0] = '\0';
  return 0;
}
US_EXPORT(int) jfwGetRunningVersion(char *buf, int size) {
  if (buf != PRISM_SHIM_NULL && size > 0)
    buf[0] = '\0';
  return 0;
}
US_EXPORT(int) nvdaLoad(void) { return us_id_available(PRISM_BACKEND_NVDA); }
US_EXPORT(void) nvdaUnload(void) { us_id_unload(PRISM_BACKEND_NVDA); }
US_EXPORT(int) nvdaIsAvailable(void) {
  return us_id_available(PRISM_BACKEND_NVDA);
}
US_EXPORT(int) nvdaSay(const wchar_t *text) {
  return us_id_speak_w(PRISM_BACKEND_NVDA, text, 0);
}
US_EXPORT(int) nvdaSayW(const wchar_t *text, int interrupt) {
  return us_id_speak_w(PRISM_BACKEND_NVDA, text, interrupt);
}
US_EXPORT(int) nvdaBraille(const wchar_t *text) {
  return us_id_braille_w(PRISM_BACKEND_NVDA, text);
}
US_EXPORT(int) nvdaStopSpeech(void) { return us_id_stop(PRISM_BACKEND_NVDA); }
US_EXPORT(int) nvdaGetRunningVersion(char *buf, int size) {
  if (buf != PRISM_SHIM_NULL && size > 0)
    buf[0] = '\0';
  return 0;
}

US_EXPORT(int) weLoad(void) {
  return us_id_available(PRISM_BACKEND_WINDOW_EYES);
}
US_EXPORT(void) weUnload(void) { us_id_unload(PRISM_BACKEND_WINDOW_EYES); }
US_EXPORT(int) weIsAvailable(void) {
  return us_id_available(PRISM_BACKEND_WINDOW_EYES);
}
US_EXPORT(int) weSayA(const char *text) {
  return us_id_speak_a(PRISM_BACKEND_WINDOW_EYES, text, 0);
}
US_EXPORT(int) weSayW(const wchar_t *text) {
  return us_id_speak_w(PRISM_BACKEND_WINDOW_EYES, text, 0);
}
US_EXPORT(int) weBrailleA(const char *text) {
  return us_id_braille_a(PRISM_BACKEND_WINDOW_EYES, text);
}
US_EXPORT(int) weBrailleW(const wchar_t *text) {
  return us_id_braille_w(PRISM_BACKEND_WINDOW_EYES, text);
}
US_EXPORT(int) weStopSpeech(void) {
  return us_id_stop(PRISM_BACKEND_WINDOW_EYES);
}

US_EXPORT(int) saLoad(void) {
  return us_id_available(PRISM_BACKEND_SYSTEM_ACCESS);
}
US_EXPORT(void) saUnload(void) { us_id_unload(PRISM_BACKEND_SYSTEM_ACCESS); }
US_EXPORT(int) saIsAvailable(void) {
  return us_id_available(PRISM_BACKEND_SYSTEM_ACCESS);
}
US_EXPORT(int) saSayA(const char *text) {
  return us_id_speak_a(PRISM_BACKEND_SYSTEM_ACCESS, text, 0);
}
US_EXPORT(int) saSayW(const wchar_t *text) {
  return us_id_speak_w(PRISM_BACKEND_SYSTEM_ACCESS, text, 0);
}
US_EXPORT(int) saBrailleA(const char *text) {
  return us_id_braille_a(PRISM_BACKEND_SYSTEM_ACCESS, text);
}
US_EXPORT(int) saBrailleW(const wchar_t *text) {
  return us_id_braille_w(PRISM_BACKEND_SYSTEM_ACCESS, text);
}
US_EXPORT(int) saStopSpeech(void) {
  return us_id_stop(PRISM_BACKEND_SYSTEM_ACCESS);
}

US_EXPORT(int) dolLoad(void) { return 0; }
US_EXPORT(void) dolUnload(void) {}
US_EXPORT(int) dolIsAvailable(void) { return 0; }
US_EXPORT(int) dolSay(const wchar_t *text) {
  (void)text;
  return 0;
}
US_EXPORT(int) dolStopSpeech(void) { return 0; }
US_EXPORT(int) cbrLoad(void) { return 0; }
US_EXPORT(void) cbrUnload(void) {}
US_EXPORT(int) cbrIsAvailable(void) { return 0; }
US_EXPORT(int) cbrSayA(const char *text) {
  (void)text;
  return 0;
}
US_EXPORT(int) cbrSayW(const wchar_t *text) {
  (void)text;
  return 0;
}
US_EXPORT(int) cbrBrailleA(const char *text) {
  (void)text;
  return 0;
}
US_EXPORT(int) cbrBrailleW(const wchar_t *text) {
  (void)text;
  return 0;
}
US_EXPORT(int) cbrStopSpeech(void) { return 0; }

US_EXPORT(int) ztLoad(void) { return us_id_available(PRISM_BACKEND_ZOOM_TEXT); }
US_EXPORT(int) ztUnload(void) {
  us_id_unload(PRISM_BACKEND_ZOOM_TEXT);
  return 1;
}
US_EXPORT(int) ztIsAvailable(void) {
  return us_id_available(PRISM_BACKEND_ZOOM_TEXT);
}
US_EXPORT(int) ztIsActive(void) { return ztIsAvailable(); }
US_EXPORT(int) ztSayW(const wchar_t *text, int interrupt) {
  return us_id_speak_w(PRISM_BACKEND_ZOOM_TEXT, text, interrupt);
}
US_EXPORT(int) ztStopSpeech(void) {
  return us_id_stop(PRISM_BACKEND_ZOOM_TEXT);
}

US_EXPORT(int) zdsrLoad(void) { return us_id_available(PRISM_BACKEND_ZDSR); }
US_EXPORT(void) zdsrUnload(void) { us_id_unload(PRISM_BACKEND_ZDSR); }
US_EXPORT(int) zdsrIsAvailable(void) {
  return us_id_available(PRISM_BACKEND_ZDSR);
}
US_EXPORT(int) zdsrSayW(const wchar_t *text, int interrupt) {
  return us_id_speak_w(PRISM_BACKEND_ZDSR, text, interrupt);
}
US_EXPORT(int) zdsrStopSpeech(void) { return us_id_stop(PRISM_BACKEND_ZDSR); }
US_EXPORT(int) zdsrIsSpeaking(void) {
  fast_lock_acquire(&us_lock);
  PrismBackend *backend =
      us_backend_locked(us_index_for_id(PRISM_BACKEND_ZDSR));
  bool speaking = false;
  const int result =
      backend != PRISM_SHIM_NULL &&
      prism_backend_is_speaking(backend, &speaking) == PRISM_OK && speaking;
  fast_lock_release(&us_lock);
  return result;
}

US_EXPORT(int) narIsAvailable(void) {
  return us_id_available(PRISM_BACKEND_UIA);
}

static PrismBackend *us_native_locked(void) {
  return us_backend_locked(US_NATIVE_INDEX);
}

US_EXPORT(int) sapiLoad(void) {
  fast_lock_acquire(&us_lock);
  const int result = us_native_locked() != PRISM_SHIM_NULL;
  fast_lock_release(&us_lock);
  return result;
}

US_EXPORT(void) sapiUnload(void) { us_id_unload(shim_native_tts_id()); }

US_EXPORT(int) sapiIsAvailable(void) {
  return us_id_available(shim_native_tts_id());
}
static int us_sapi_say_w_locked(const wchar_t *text, int interrupt) {
  PrismBackend *backend = us_native_locked();
  if (backend == PRISM_SHIM_NULL || text == PRISM_SHIM_NULL)
    return 0;
  char *utf8 = shim_wchar_to_utf8(text);
  if (utf8 == PRISM_SHIM_NULL)
    return 0;
  if (us_wave_callback == PRISM_SHIM_NULL) {
    const int spoken =
        prism_backend_speak(backend, utf8, interrupt != 0) == PRISM_OK;
    free(utf8);
    return spoken;
  }
  ShimAudioBuffer audio;
  shim_audio_buffer_init(&audio);
  int16_t *pcm = PRISM_SHIM_NULL;
  size_t sample_count = 0;
  int result = 0;
  if (shim_synthesize_audio(backend, utf8, &audio) &&
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

US_EXPORT(int) sapiSayW(const wchar_t *text, int interrupt) {
  fast_lock_acquire(&us_lock);
  const int result = us_sapi_say_w_locked(text, interrupt);
  fast_lock_release(&us_lock);
  return result;
}
US_EXPORT(int) sapiSaySSMLW(const wchar_t *text, int interrupt) {
  (void)text;
  (void)interrupt;
  return 0;
}

US_EXPORT(int) sapiSayA(const char *text, int interrupt) {
  char *utf8 = shim_ansi_to_utf8(text);
  wchar_t *wide = shim_utf8_to_wchar(utf8);
  const int result = wide == PRISM_SHIM_NULL ? 0 : sapiSayW(wide, interrupt);
  free(wide);
  free(utf8);
  return result;
}

US_EXPORT(int) sapiSaySSMLA(const char *text, int interrupt) {
  (void)text;
  (void)interrupt;
  return 0;
}

US_EXPORT(int) sapiStopSpeech(void) { return us_id_stop(shim_native_tts_id()); }

US_EXPORT(int) sapiSetPaused(int paused) {
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

US_EXPORT(int) sapiIsPaused(void) {
  fast_lock_acquire(&us_lock);
  const int result = (int)us_slots[US_NATIVE_INDEX].paused;
  fast_lock_release(&us_lock);
  return result;
}
US_EXPORT(int) sapiWait(int timeout) {
  (void)timeout;
  return 0;
}
US_EXPORT(int) sapiIsSpeaking(void) {
  fast_lock_acquire(&us_lock);
  PrismBackend *backend = us_native_locked();
  bool speaking = false;
  const int result =
      backend != PRISM_SHIM_NULL &&
      prism_backend_is_speaking(backend, &speaking) == PRISM_OK && speaking;
  fast_lock_release(&us_lock);
  return result;
}

US_EXPORT(int) sapiSetRate(int rate) {
  fast_lock_acquire(&us_lock);
  PrismBackend *backend = us_native_locked();
  const int result =
      backend != PRISM_SHIM_NULL &&
      prism_backend_set_rate(backend, shim_clamp01((float)rate / 100.0F)) ==
          PRISM_OK;
  fast_lock_release(&us_lock);
  return result;
}

US_EXPORT(int) sapiGetRate(void) {
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

US_EXPORT(int) sapiSetVolume(int volume) {
  fast_lock_acquire(&us_lock);
  PrismBackend *backend = us_native_locked();
  const int result =
      backend != PRISM_SHIM_NULL &&
      prism_backend_set_volume(backend, shim_clamp01((float)volume / 100.0F)) ==
          PRISM_OK;
  fast_lock_release(&us_lock);
  return result;
}

US_EXPORT(int) sapiGetVolume(void) {
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

US_EXPORT(int) sapiGetNumVoices(void) {
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

US_EXPORT(int) sapiSetVoice(int voice) {
  fast_lock_acquire(&us_lock);
  PrismBackend *backend = us_native_locked();
  const int result =
      backend != PRISM_SHIM_NULL && voice >= 0 &&
      prism_backend_set_voice(backend, (size_t)voice) == PRISM_OK;
  fast_lock_release(&us_lock);
  return result;
}

US_EXPORT(int) sapiGetVoice(void) {
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

US_EXPORT(const wchar_t *) sapiGetVoiceNameW(int voice) {
  fast_lock_acquire(&us_lock);
  PrismBackend *backend = us_native_locked();
  const char *name = PRISM_SHIM_NULL;
  if (backend != PRISM_SHIM_NULL && voice >= 0)
    shim_ignore_error(
        prism_backend_get_voice_name(backend, (size_t)voice, &name));
  const wchar_t *result = shim_intern_wide(&us_strings, name);
  fast_lock_release(&us_lock);
  return result;
}

US_EXPORT(const char *) sapiGetVoiceNameA(int voice) {
  return us_narrow(sapiGetVoiceNameW(voice), US_NARROW_ANSI);
}
US_EXPORT(int) sapiGetValue(int what) {
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

US_EXPORT(int) sapiSetValue(int what, int value) {
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
US_EXPORT(const wchar_t *) sapiGetString(int what) {
  if (what >= SP_VOICE && what <= SP_VOICE + 0xFFFF)
    return sapiGetVoiceNameW(what - SP_VOICE);
  return PRISM_SHIM_NULL;
}

US_EXPORT(int)
sapiSetOutputCallback(int sample_rate, UsWaveCallback callback,
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

US_EXPORT_STDCALL(int) sayStringA(const char *text, int interrupt) {
  return speechSayA(text, interrupt);
}
US_EXPORT_STDCALL(int) sayStringW(const wchar_t *text, int interrupt) {
  return speechSay(text, interrupt);
}
US_EXPORT_STDCALL(int) brailleMessageA(const char *text) {
  return brailleDisplayA(text);
}
US_EXPORT_STDCALL(int) brailleMessageW(const wchar_t *text) {
  return brailleDisplay(text);
}
US_EXPORT_STDCALL(int) stopSpeech(void) { return speechStop(); }
US_EXPORT_STDCALL(int) getCurrentScreenReader(void) {
  return speechGetValue(SP_ENGINE);
}
US_EXPORT_STDCALL(int) setCurrentScreenReader(int index) {
  return speechSetValue(SP_ENGINE, index);
}
US_EXPORT_STDCALL(const wchar_t *) getScreenReaderNameW(int index) {
  return speechGetString(SP_ENGINE + index);
}
US_EXPORT_STDCALL(const char *) getScreenReaderNameA(int index) {
  return speechGetStringA(SP_ENGINE + index);
}
US_EXPORT_STDCALL(const wchar_t *) getCurrentScreenReaderNameW(void) {
  return getScreenReaderNameW(getCurrentScreenReader());
}
US_EXPORT_STDCALL(const char *) getCurrentScreenReaderNameA(void) {
  return getScreenReaderNameA(getCurrentScreenReader());
}
US_EXPORT_STDCALL(int) getScreenReaderIdW(const wchar_t *name) {
  if (name == PRISM_SHIM_NULL)
    return -1;
  for (int i = 0; i < US_ENGINE_COUNT; ++i)
    if (wcscmp(name, us_slots[i].name) == 0)
      return i;
  return -1;
}
US_EXPORT_STDCALL(int) getScreenReaderIdA(const char *name) {
  char *utf8 = shim_ansi_to_utf8(name);
  wchar_t *wide = shim_utf8_to_wchar(utf8);
  const int result = wide == PRISM_SHIM_NULL ? -1 : getScreenReaderIdW(wide);
  free(wide);
  free(utf8);
  return result;
}
US_EXPORT_STDCALL(int) setCurrentScreenReaderNameW(const wchar_t *name) {
  return setCurrentScreenReader(getScreenReaderIdW(name));
}
US_EXPORT_STDCALL(int) setCurrentScreenReaderNameA(const char *name) {
  return setCurrentScreenReader(getScreenReaderIdA(name));
}
US_EXPORT_STDCALL(int) getSupportedScreenReadersCount(void) {
  return US_ENGINE_COUNT;
}
US_EXPORT_STDCALL(int) sapiIsEnabled(void) {
  return speechGetValue(SP_ENABLE_NATIVE_SPEECH);
}
US_EXPORT_STDCALL(int) sapiEnable(int enable) {
  return speechSetValue(SP_ENABLE_NATIVE_SPEECH, enable);
}
int US_STDCALL sapiGetRate2(void) { return sapiGetRate(); }
int US_STDCALL sapiSetRate2(int rate) { return sapiSetRate(rate); }

US_EXPORT(int) installKeyboardHook(void) { return 0; }
US_EXPORT(void) uninstallKeyboardHook(void) {}
US_EXPORT(int) FindProcess(const char *needle, char *buf, size_t size) {
  (void)needle;
  if (buf != PRISM_SHIM_NULL && size != 0)
    buf[0] = '\0';
  return 0;
}
US_EXPORT(void *) getProcessHandle(const char *needle, char *buf, size_t size) {
  (void)needle;
  if (buf != PRISM_SHIM_NULL && size != 0)
    buf[0] = '\0';
  return PRISM_SHIM_NULL;
}

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
