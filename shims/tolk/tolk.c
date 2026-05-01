// SPDX-License-Identifier: LGPLv3

#include "tolk.h"
#include "lock.h"
#include "thread_safety.h"
#include <prism.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#include "simdutf_c.h"
#if defined(_WIN32)
static const PrismBackendId default_tts_backend = PRISM_BACKEND_SAPI;
#elif defined(__APPLE__)
static const PrismBackendId default_tts_backend = PRISM_BACKEND_AV_SPEECH;
#elif defined(__ANDROID__)
static const PrismBackendId default_tts_backend = PRISM_BACKEND_ANDROID_TTS;
#elif defined(__EMSCRIPTEN__)
static const PrismBackendId default_tts_backend = PRISM_BACKEND_WEB_SPEECH;
#else
static const PrismBackendId default_tts_backend =
    PRISM_BACKEND_SPEECH_DISPATCHER;
#endif

static PrismContext *ctx;
static fast_lock lock = FAST_LOCK_INIT;
static PrismBackend *backend TSA_GUARDED_BY(lock);
static PrismBackend *sapi_backend TSA_GUARDED_BY(lock);
static wchar_t *backend_name TSA_GUARDED_BY(lock);
static wchar_t *sapi_backend_name TSA_GUARDED_BY(lock);
static atomic_bool loaded;
static atomic_bool prefer_sapi;

static inline char *wchar_to_utf8(const wchar_t *src) {
  if (src == NULL)
    return NULL;
#if WCHAR_MAX <= 0xFFFFu
  const char16_t *in = (const char16_t *)src;
  size_t in_len = 0;
  while (in[in_len] != 0)
    ++in_len;
  size_t out_len = simdutf_utf8_length_from_utf16(in, in_len);
  char *buf = malloc(out_len + 1);
  if (buf == NULL)
    return NULL;
  size_t written = simdutf_convert_utf16_to_utf8(in, in_len, buf);
  if (written == 0 && in_len != 0) {
    free(buf);
    return NULL;
  }
  buf[written] = '\0';
  return buf;
#else
  const char32_t *in = (const char32_t *)src;
  size_t in_len = 0;
  while (in[in_len] != 0)
    ++in_len;
  size_t out_len = simdutf_utf8_length_from_utf32(in, in_len);
  char *buf = malloc(out_len + 1);
  if (buf == NULL)
    return NULL;
  size_t written = simdutf_convert_utf32_to_utf8(in, in_len, buf);
  if (written == 0 && in_len != 0) {
    free(buf);
    return NULL;
  }
  buf[written] = '\0';
  return buf;
#endif
}

static inline wchar_t *utf8_to_wchar(const char *src) {
  if (src == NULL)
    return NULL;
  size_t in_len = strlen(src);
#if WCHAR_MAX <= 0xFFFFu
  size_t out_len = simdutf_utf16_length_from_utf8(src, in_len);
  wchar_t *buf = malloc((out_len + 1) * sizeof(wchar_t));
  if (buf == NULL)
    return NULL;
  size_t written = simdutf_convert_utf8_to_utf16(src, in_len, (char16_t *)buf);
  if (written == 0 && in_len != 0) {
    free(buf);
    return NULL;
  }
  buf[written] = L'\0';
  return buf;
#else
  size_t out_len = simdutf_utf32_length_from_utf8(src, in_len);
  wchar_t *buf = malloc((out_len + 1) * sizeof(wchar_t));
  if (buf == NULL)
    return NULL;
  size_t written = simdutf_convert_utf8_to_utf32(src, in_len, (char32_t *)buf);
  if (written == 0 && in_len != 0) {
    free(buf);
    return NULL;
  }
  buf[written] = L'\0';
  return buf;
#endif
}

TOLK_API void TOLK_CALL Tolk_Load(void) {
  fast_lock_acquire(&lock);
  if (atomic_load(&loaded)) {
    fast_lock_release(&lock);
    return;
  }
  PrismConfig cfg = prism_config_init();
  ctx = prism_init(&cfg);
  if (ctx == NULL) {
    fast_lock_release(&lock);
    return;
  }
  backend = prism_registry_create_best(ctx);
  if (backend != NULL) {
    if (prism_backend_initialize(backend) != PRISM_OK) {
      prism_backend_free(backend);
      backend = NULL;
    }
  }
  sapi_backend = prism_registry_create(ctx, default_tts_backend);
  if (sapi_backend != NULL) {
    if (prism_backend_initialize(sapi_backend) != PRISM_OK) {
      prism_backend_free(sapi_backend);
      sapi_backend = NULL;
    }
  }
  if (backend != NULL)
    backend_name = utf8_to_wchar(prism_backend_name(backend));
  if (sapi_backend != NULL)
    sapi_backend_name = utf8_to_wchar(prism_backend_name(sapi_backend));
  atomic_store(&loaded, true);
  fast_lock_release(&lock);
}

TOLK_API bool TOLK_CALL Tolk_IsLoaded(void) { return atomic_load(&loaded); }

TOLK_API void TOLK_CALL Tolk_Unload(void) {
  fast_lock_acquire(&lock);
  if (!atomic_load(&loaded)) {
    fast_lock_release(&lock);
    return;
  }
  atomic_store(&loaded, false);
  if (backend != NULL) {
    prism_backend_free(backend);
    backend = NULL;
  }
  if (sapi_backend != NULL) {
    prism_backend_free(sapi_backend);
    sapi_backend = NULL;
  }
  prism_shutdown(ctx);
  ctx = NULL;
  free(backend_name);
  backend_name = NULL;
  free(sapi_backend_name);
  sapi_backend_name = NULL;
  fast_lock_release(&lock);
}

TOLK_API void TOLK_CALL Tolk_TrySAPI(bool trySAPI) {}

TOLK_API void TOLK_CALL Tolk_PreferSAPI(bool preferSAPI) {
  atomic_store(&prefer_sapi, preferSAPI);
}

TOLK_API const wchar_t *TOLK_CALL Tolk_DetectScreenReader(void) {
  static _Thread_local wchar_t buf[256];
  if (!atomic_load(&loaded))
    return NULL;
  fast_lock_acquire(&lock);
  const wchar_t *name =
      atomic_load(&prefer_sapi) ? sapi_backend_name : backend_name;
  if (name == NULL) {
    fast_lock_release(&lock);
    return NULL;
  }
  wcsncpy(buf, name, 255);
  buf[255] = L'\0';
  fast_lock_release(&lock);
  return buf;
}

TOLK_API bool TOLK_CALL Tolk_HasSpeech(void) {
  if (!atomic_load(&loaded))
    return false;
  fast_lock_acquire(&lock);
  PrismBackend *b = atomic_load(&prefer_sapi) ? sapi_backend : backend;
  if (b == NULL) {
    fast_lock_release(&lock);
    return false;
  }
  const uint64_t features = prism_backend_get_features(b);
  fast_lock_release(&lock);
  return (features & PRISM_BACKEND_SUPPORTS_SPEAK) != 0;
}

TOLK_API bool TOLK_CALL Tolk_HasBraille(void) {
  if (!atomic_load(&loaded))
    return false;
  fast_lock_acquire(&lock);
  PrismBackend *b = atomic_load(&prefer_sapi) ? sapi_backend : backend;
  if (b == NULL) {
    fast_lock_release(&lock);
    return false;
  }
  const uint64_t features = prism_backend_get_features(b);
  fast_lock_release(&lock);
  return (features & PRISM_BACKEND_SUPPORTS_BRAILLE) != 0;
}

TOLK_API bool TOLK_CALL Tolk_Output(const wchar_t *str, bool interrupt) {
  if (str == NULL || !atomic_load(&loaded))
    return false;
  char *utf8 = wchar_to_utf8(str);
  if (utf8 == NULL)
    return false;
  fast_lock_acquire(&lock);
  PrismBackend *b = atomic_load(&prefer_sapi) ? sapi_backend : backend;
  PrismError err = PRISM_ERROR_NOT_INITIALIZED;
  if (b != NULL)
    err = prism_backend_output(b, utf8, interrupt);
  fast_lock_release(&lock);
  free(utf8);
  return err == PRISM_OK;
}

TOLK_API bool TOLK_CALL Tolk_Speak(const wchar_t *str, bool interrupt) {
  if (str == NULL || !atomic_load(&loaded))
    return false;
  char *utf8 = wchar_to_utf8(str);
  if (utf8 == NULL)
    return false;
  fast_lock_acquire(&lock);
  PrismBackend *b = atomic_load(&prefer_sapi) ? sapi_backend : backend;
  PrismError err = PRISM_ERROR_NOT_INITIALIZED;
  if (b != NULL)
    err = prism_backend_speak(b, utf8, interrupt);
  fast_lock_release(&lock);
  free(utf8);
  return err == PRISM_OK;
}

TOLK_API bool TOLK_CALL Tolk_Braille(const wchar_t *str) {
  if (str == NULL || !atomic_load(&loaded))
    return false;
  char *utf8 = wchar_to_utf8(str);
  if (utf8 == NULL)
    return false;
  fast_lock_acquire(&lock);
  PrismBackend *b = atomic_load(&prefer_sapi) ? sapi_backend : backend;
  PrismError err = PRISM_ERROR_NOT_INITIALIZED;
  if (b != NULL)
    err = prism_backend_braille(b, utf8);
  fast_lock_release(&lock);
  free(utf8);
  return err == PRISM_OK;
}

TOLK_API bool TOLK_CALL Tolk_IsSpeaking(void) {
  if (!atomic_load(&loaded))
    return false;
  fast_lock_acquire(&lock);
  PrismBackend *b = atomic_load(&prefer_sapi) ? sapi_backend : backend;
  bool speaking = false;
  if (b != NULL)
    if (prism_backend_is_speaking(b, &speaking) != PRISM_OK) {
      fast_lock_release(&lock);
      return false;
    }
  fast_lock_release(&lock);
  return speaking;
}

TOLK_API bool TOLK_CALL Tolk_Silence(void) {
  if (!atomic_load(&loaded))
    return false;
  fast_lock_acquire(&lock);
  PrismBackend *b = atomic_load(&prefer_sapi) ? sapi_backend : backend;
  PrismError err = PRISM_ERROR_NOT_INITIALIZED;
  if (b != NULL)
    err = prism_backend_stop(b);
  fast_lock_release(&lock);
  return err == PRISM_OK;
}
