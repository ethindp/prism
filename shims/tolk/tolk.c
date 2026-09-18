// SPDX-License-Identifier: MPL-2.0

#include "tolk.h"
#include "lock.h"
#include "shim_common.h"
#include "thread_safety.h"
#include <prism.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

typedef enum TolkOp {
  TOLK_OP_OUTPUT,
  TOLK_OP_SPEAK,
  TOLK_OP_BRAILLE,
  TOLK_OP_SILENCE
} TolkOp;

static fast_lock tolk_lock = FAST_LOCK_INIT;
static ShimFlag tolk_stale;
static PrismContext *tolk_ctx TSA_GUARDED_BY(tolk_lock);
static ShimSelection tolk_auto TSA_GUARDED_BY(tolk_lock);
static PrismBackend *tolk_sapi TSA_GUARDED_BY(tolk_lock);
static ShimStrings tolk_strings TSA_GUARDED_BY(tolk_lock);
static bool tolk_loaded TSA_GUARDED_BY(tolk_lock);
static bool tolk_try_sapi TSA_GUARDED_BY(tolk_lock);
static bool tolk_prefer_sapi TSA_GUARDED_BY(tolk_lock);

static PrismBackend *tolk_current_locked(void) TSA_REQUIRES(tolk_lock) {
  if (tolk_try_sapi && tolk_prefer_sapi) {
    PrismBackend *sapi =
        shim_slot_get(tolk_ctx, shim_native_tts_id(), &tolk_sapi);
    if (sapi != PRISM_SHIM_NULL) {
      return sapi;
    }
  }
  if (shim_flag_take(&tolk_stale)) {
    shim_select(tolk_ctx, &tolk_auto, PRISM_SHIM_NULL, PRISM_SHIM_NULL);
  }
  return tolk_auto.backend;
}

static PrismError tolk_apply(PrismBackend *backend, TolkOp op, const char *text,
                             bool interrupt) {
  if (backend == PRISM_SHIM_NULL) {
    return PRISM_ERROR_BACKEND_NOT_AVAILABLE;
  }
  switch (op) {
  case TOLK_OP_OUTPUT:
    return prism_backend_output(backend, text, interrupt);
  case TOLK_OP_SPEAK:
    return prism_backend_speak(backend, text, interrupt);
  case TOLK_OP_BRAILLE:
    return prism_backend_braille(backend, text);
  case TOLK_OP_SILENCE:
    return prism_backend_stop(backend);
  }
  return PRISM_ERROR_INVALID_PARAM;
}

static bool tolk_run(TolkOp op, const wchar_t *str, bool interrupt)
    TSA_EXCLUDES(tolk_lock) {
  char *utf8 = PRISM_SHIM_NULL;
  if (op != TOLK_OP_SILENCE) {
    if (str == PRISM_SHIM_NULL) {
      return false;
    }
    utf8 = shim_wchar_to_utf8(str);
    if (utf8 == PRISM_SHIM_NULL) {
      return false;
    }
  }
  fast_lock_acquire(&tolk_lock);
  PrismError error = PRISM_ERROR_NOT_INITIALIZED;
  if (tolk_loaded) {
    PrismBackend *backend = tolk_current_locked();
    error = tolk_apply(backend, op, utf8, interrupt);
    if (backend != PRISM_SHIM_NULL && shim_error_means_lost(error)) {
      if (backend == tolk_sapi) {
        shim_slot_drop(&tolk_sapi);
      } else {
        shim_selection_clear(&tolk_auto);
      }
      shim_flag_raise(&tolk_stale);
      error = tolk_apply(tolk_current_locked(), op, utf8, interrupt);
    }
  }
  fast_lock_release(&tolk_lock);
  free(utf8);
  return error == PRISM_OK;
}

TOLK_API void TOLK_CALL Tolk_Load(void) {
  fast_lock_acquire(&tolk_lock);
  if (!tolk_loaded) {
    tolk_ctx = shim_context_open(&tolk_stale);
    if (tolk_ctx != PRISM_SHIM_NULL) {
      shim_select(tolk_ctx, &tolk_auto, PRISM_SHIM_NULL, PRISM_SHIM_NULL);
      tolk_loaded = true;
    }
  }
  fast_lock_release(&tolk_lock);
}

TOLK_API bool TOLK_CALL Tolk_IsLoaded(void) {
  fast_lock_acquire(&tolk_lock);
  const bool result = tolk_loaded;
  fast_lock_release(&tolk_lock);
  return result;
}

TOLK_API void TOLK_CALL Tolk_Unload(void) {
  fast_lock_acquire(&tolk_lock);
  if (tolk_loaded) {
    shim_selection_clear(&tolk_auto);
    shim_slot_drop(&tolk_sapi);
    prism_shutdown(tolk_ctx);
    tolk_ctx = PRISM_SHIM_NULL;
    (void)shim_flag_take(&tolk_stale);
    shim_strings_free(&tolk_strings);
    tolk_loaded = false;
  }
  fast_lock_release(&tolk_lock);
}

TOLK_API void TOLK_CALL Tolk_TrySAPI(bool trySAPI) {
  fast_lock_acquire(&tolk_lock);
  tolk_try_sapi = trySAPI;
  fast_lock_release(&tolk_lock);
}

TOLK_API void TOLK_CALL Tolk_PreferSAPI(bool preferSAPI) {
  fast_lock_acquire(&tolk_lock);
  tolk_prefer_sapi = preferSAPI;
  fast_lock_release(&tolk_lock);
}

TOLK_API const wchar_t *TOLK_CALL Tolk_DetectScreenReader(void) {
  fast_lock_acquire(&tolk_lock);
  const wchar_t *result = PRISM_SHIM_NULL;
  if (tolk_loaded) {
    PrismBackend *backend = tolk_current_locked();
    if (backend != PRISM_SHIM_NULL) {
      result = shim_intern_wide(&tolk_strings, prism_backend_name(backend));
    }
  }
  fast_lock_release(&tolk_lock);
  return result;
}

TOLK_API bool TOLK_CALL Tolk_HasSpeech(void) {
  fast_lock_acquire(&tolk_lock);
  uint64_t features = 0;
  if (tolk_loaded) {
    PrismBackend *backend = tolk_current_locked();
    if (backend != PRISM_SHIM_NULL) {
      features = prism_backend_get_features(backend);
    }
  }
  fast_lock_release(&tolk_lock);
  return (features & PRISM_BACKEND_SUPPORTS_SPEAK) != 0;
}

TOLK_API bool TOLK_CALL Tolk_HasBraille(void) {
  fast_lock_acquire(&tolk_lock);
  uint64_t features = 0;
  if (tolk_loaded) {
    PrismBackend *backend = tolk_current_locked();
    if (backend != PRISM_SHIM_NULL) {
      features = prism_backend_get_features(backend);
    }
  }
  fast_lock_release(&tolk_lock);
  return (features & PRISM_BACKEND_SUPPORTS_BRAILLE) != 0;
}

TOLK_API bool TOLK_CALL Tolk_Output(const wchar_t *str, bool interrupt) {
  return tolk_run(TOLK_OP_OUTPUT, str, interrupt);
}

TOLK_API bool TOLK_CALL Tolk_Speak(const wchar_t *str, bool interrupt) {
  return tolk_run(TOLK_OP_SPEAK, str, interrupt);
}

TOLK_API bool TOLK_CALL Tolk_Braille(const wchar_t *str) {
  return tolk_run(TOLK_OP_BRAILLE, str, false);
}

TOLK_API bool TOLK_CALL Tolk_IsSpeaking(void) {
  fast_lock_acquire(&tolk_lock);
  bool speaking = false;
  PrismError error = PRISM_ERROR_NOT_INITIALIZED;
  if (tolk_loaded) {
    PrismBackend *backend = tolk_current_locked();
    if (backend != PRISM_SHIM_NULL) {
      error = prism_backend_is_speaking(backend, &speaking);
    }
  }
  fast_lock_release(&tolk_lock);
  return (bool)((int)error == PRISM_OK && speaking);
}

TOLK_API bool TOLK_CALL Tolk_Silence(void) {
  return tolk_run(TOLK_OP_SILENCE, PRISM_SHIM_NULL, false);
}
