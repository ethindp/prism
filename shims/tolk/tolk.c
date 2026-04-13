// SPDX-License-Identifier: LGPLv3

#include "tolk.h"
#include "lock.h"
#include "thread_safety.h"
#include "unicode.h"
#include <prism.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#ifndef _WIN32
#include <locale.h>
#endif
#ifdef _WIN32
static const bool is_windows = true;
static const bool is_macos = false;
static const bool is_ios = false;
static const bool is_android = false;
static const bool is_unix = false;
static const bool is_web = false;
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IOS || TARGET_OS_SIMULATOR || TARGET_OS_TVOS ||                  \
    TARGET_OS_WATCHOS || TARGET_OS_VISIONOS
static const bool is_windows = false;
static const bool is_macos = false;
static const bool is_ios = true;
static const bool is_android = false;
static const bool is_unix = false;
static const bool is_web = false;
#else
static const bool is_windows = false;
static const bool is_macos = true;
static const bool is_ios = false;
static const bool is_android = false;
static const bool is_unix = false;
static const bool is_web = false;
#endif
#elif defined(__ANDROID__)
static const bool is_windows = false;
static const bool is_macos = false;
static const bool is_ios = false;
static const bool is_android = true;
static const bool is_unix = false;
static const bool is_web = false;
#elif defined(__EMSCRIPTEN__)
static const bool is_windows = false;
static const bool is_macos = false;
static const bool is_ios = false;
static const bool is_android = false;
static const bool is_unix = false;
static const bool is_web = true;
#else
static const bool is_windows = false;
static const bool is_macos = false;
static const bool is_ios = false;
static const bool is_android = false;
static const bool is_unix = true;
static const bool is_web = false;
#endif

static PrismContext *ctx;
static fast_lock lock = FAST_LOCK_INIT;
static PrismBackend *backend TSA_GUARDED_BY(lock);
static PrismBackend *sapi_backend TSA_GUARDED_BY(lock);
static wchar_t *backend_name TSA_GUARDED_BY(lock);
static wchar_t *sapi_backend_name TSA_GUARDED_BY(lock);
static UnicodeLocale saved_locale TSA_GUARDED_BY(lock);
static atomic_bool loaded;
static atomic_bool prefer_sapi;

static inline char *wchar_to_utf8(const wchar_t *src) {
  if (src == NULL)
    return NULL;
#ifdef _WIN32
  return utf16_to_utf8_alloc((const uint16_t *)src, (size_t)-1);
#else
  size_t len = wcstombs(NULL, src, 0);
  if (len == (size_t)-1)
    return NULL;
  char *buf = malloc(len + 1);
  if (buf == NULL)
    return NULL;
  wcstombs(buf, src, len + 1);
  return buf;
#endif
}

static inline wchar_t *utf8_to_wchar(const char *src) {
  if (src == NULL)
    return NULL;
#ifdef _WIN32
  return (wchar_t *)utf8_to_utf16_alloc(src, (size_t)-1, NULL);
#else
  size_t len = mbstowcs(NULL, src, 0);
  if (len == (size_t)-1)
    return NULL;
  wchar_t *buf = malloc((len + 1) * sizeof(wchar_t));
  if (buf == NULL)
    return NULL;
  mbstowcs(buf, src, len + 1);
  return buf;
#endif
}

TOLK_API void TOLK_CALL Tolk_Load(void) {
  fast_lock_acquire(&lock);
  if (atomic_load(&loaded)) {
    fast_lock_release(&lock);
    return;
  }
  utf_locale_save(&saved_locale);
  utf_locale_set_utf8();
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
  sapi_backend =
      prism_registry_create(ctx, is_windows           ? PRISM_BACKEND_SAPI
                                 : is_macos || is_ios ? PRISM_BACKEND_AV_SPEECH
                                 : is_unix    ? PRISM_BACKEND_SPEECH_DISPATCHER
                                 : is_android ? PRISM_BACKEND_ANDROID_TTS
                                 : is_web     ? PRISM_BACKEND_WEB_SPEECH
                                              : PRISM_BACKEND_INVALID);
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
  utf_locale_restore(&saved_locale);
  fast_lock_release(&lock);
}

TOLK_API void TOLK_CALL Tolk_TrySAPI(bool trySAPI) {
  return; // Do nothing
}

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
