// SPDX-License-Identifier: MPL-2.0

#include "tolk.h"
#include "lock.h"
#include "thread_safety.h"
#include <prism.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif
#ifdef _WIN32
static const PrismBackendId default_tts_backend = PRISM_BACKEND_SAPI;
#elifdef __APPLE__
static const PrismBackendId default_tts_backend = PRISM_BACKEND_AV_SPEECH;
#elifdef __ANDROID__
static const PrismBackendId default_tts_backend = PRISM_BACKEND_ANDROID_TTS;
#elifdef __EMSCRIPTEN__
static const PrismBackendId default_tts_backend = PRISM_BACKEND_WEB_SPEECH;
#else
static const PrismBackendId default_tts_backend =
    PRISM_BACKEND_SPEECH_DISPATCHER;
#endif

/*
 * The following is extremely nasty, but MSVC, for some reason, does not
 * support nullptr in C23.
 */
#ifdef _MSC_VER
#define NULL_CONSTANT NULL
#else
#define NULL_CONSTANT nullptr
#endif

static fast_lock lock = FAST_LOCK_INIT;
static PrismContext *ctx TSA_GUARDED_BY(lock);
static PrismBackend *backend TSA_GUARDED_BY(lock);
static PrismBackend *sapi_backend TSA_GUARDED_BY(lock);
static wchar_t *backend_name TSA_GUARDED_BY(lock);
static wchar_t *sapi_backend_name TSA_GUARDED_BY(lock);
static bool loaded TSA_GUARDED_BY(lock);
static bool prefer_sapi TSA_GUARDED_BY(lock);

static char *wchar_to_utf8(const wchar_t *src) {
  if (src == NULL_CONSTANT) {
    return NULL_CONSTANT;
  }
  size_t in_len = 0;
  while (src[in_len] != L'\0') {
    ++in_len;
  }
  if (in_len > ((SIZE_MAX - 1) / 4)) {
    return NULL_CONSTANT;
  }
  const size_t cap = (in_len * 4) + 1;
  char *buf = malloc(cap);
  if (buf == NULL_CONSTANT) {
    return NULL_CONSTANT;
  }
  size_t in_pos = 0;
  size_t out_pos = 0;
  while (in_pos < in_len) {
    uint32_t cp = (uint32_t)src[in_pos];
    ++in_pos;
    if (WCHAR_MAX <= 0xFFFF) {
      if ((cp >= 0xD800) && (cp <= 0xDBFF)) {
        if (in_pos >= in_len) {
          free(buf);
          return NULL_CONSTANT;
        }
        const uint32_t low = (uint32_t)src[in_pos];
        if ((low < 0xDC00) || (low > 0xDFFF)) {
          free(buf);
          return NULL_CONSTANT;
        }
        ++in_pos;
        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
      } else if ((cp >= 0xDC00) && (cp <= 0xDFFF)) {
        free(buf);
        return NULL_CONSTANT;
      }
    } else {
      if ((cp >= 0xD800) && (cp <= 0xDFFF)) {
        free(buf);
        return NULL_CONSTANT;
      }
    }
    if (cp > 0x10FFFF) {
      free(buf);
      return NULL_CONSTANT;
    }
    if ((cap - out_pos) < 5) {
      free(buf);
      return NULL_CONSTANT;
    }
    if (cp < 0x80) {
      buf[out_pos] = (char)cp;
      out_pos++;
    } else if (cp < 0x800) {
      buf[out_pos] = (char)(0xC0 | (cp >> 6));
      buf[out_pos + 1] = (char)(0x80 | (cp & 0x3F));
      out_pos += 2;
    } else if (cp < 0x10000) {
      buf[out_pos] = (char)(0xE0 | (cp >> 12));
      buf[out_pos + 1] = (char)(0x80 | ((cp >> 6) & 0x3F));
      buf[out_pos + 2] = (char)(0x80 | (cp & 0x3F));
      out_pos += 3;
    } else {
      buf[out_pos] = (char)(0xF0U | (cp >> 18U));
      buf[out_pos + 1U] = (char)(0x80U | ((cp >> 12U) & 0x3FU));
      buf[out_pos + 2U] = (char)(0x80U | ((cp >> 6U) & 0x3FU));
      buf[out_pos + 3U] = (char)(0x80U | (cp & 0x3FU));
      out_pos += 4U;
    }
  }
  buf[out_pos] = '\0';
  return buf;
}

static wchar_t *utf8_to_wchar(const char *src) {
  if (src == NULL_CONSTANT) {
    return NULL_CONSTANT;
  }
  const size_t in_len = strlen(src);
  if (in_len > ((SIZE_MAX / sizeof(wchar_t)) - 1)) {
    return NULL_CONSTANT;
  }
  const size_t cap = in_len + 1;
  wchar_t *buf = malloc(cap * sizeof(wchar_t));
  if (buf == NULL_CONSTANT) {
    return NULL_CONSTANT;
  }
  const unsigned char *in = (const unsigned char *)src;
  size_t in_pos = 0;
  size_t out_pos = 0;
  while (in_pos < in_len) {
    const unsigned char lead = in[in_pos];
    uint32_t cp = 0;
    size_t trail = 0;
    if (lead < 0x80) {
      cp = (uint32_t)lead;
      trail = 0;
    } else if ((lead & 0xE0) == 0xC0) {
      cp = (uint32_t)(lead & 0x1F);
      trail = 1;
    } else if ((lead & 0xF0) == 0xE0) {
      cp = (uint32_t)(lead & 0x0F);
      trail = 2;
    } else if ((lead & 0xF8) == 0xF0) {
      cp = (uint32_t)(lead & 0x07);
      trail = 3;
    } else {
      free(buf);
      return NULL_CONSTANT;
    }
    if ((in_len - in_pos) <= trail) {
      free(buf);
      return NULL_CONSTANT;
    }
    for (size_t k = 1; k <= trail; ++k) {
      const unsigned char cont = in[in_pos + k];
      if ((cont & 0xC0) != 0x80) {
        free(buf);
        return NULL_CONSTANT;
      }
      cp = (cp << 6) | (uint32_t)(cont & 0x3F);
    }
    in_pos += trail + 1;
    if (((trail == 1) && (cp < 0x80)) || ((trail == 2) && (cp < 0x800)) ||
        ((trail == 3) && (cp < 0x10000))) {
      free(buf);
      return NULL_CONSTANT;
    }
    if ((cp > 0x10FFFF) || ((cp >= 0xD800) && (cp <= 0xDFFF))) {
      free(buf);
      return NULL_CONSTANT;
    }
    if (WCHAR_MAX <= 0xFFFF) {
      if (cp >= 0x10000) {
        if ((cap - out_pos) < 3) {
          free(buf);
          return NULL_CONSTANT;
        }
        const uint32_t adjusted = cp - 0x10000;
        buf[out_pos] = (wchar_t)(0xD800 + (adjusted >> 10));
        buf[out_pos + 1] = (wchar_t)(0xDC00 + (adjusted & 0x3FF));
        out_pos += 2;
        continue;
      }
    }
    if ((cap - out_pos) < 2) {
      free(buf);
      return NULL_CONSTANT;
    }
    buf[out_pos] = (wchar_t)cp;
    out_pos++;
  }
  buf[out_pos] = L'\0';
  return buf;
}

TOLK_API void TOLK_CALL Tolk_Load(void) {
  fast_lock_acquire(&lock);
  if (loaded) {
    fast_lock_release(&lock);
    return;
  }
  PrismConfig cfg = prism_config_init();
  ctx = prism_init(&cfg);
  if (ctx == NULL_CONSTANT) {
    fast_lock_release(&lock);
    return;
  }
  backend = prism_registry_create_best(ctx);
  if (backend != NULL_CONSTANT) {
    const PrismError res = prism_backend_initialize(backend);
    if (res != PRISM_OK && res != PRISM_ERROR_ALREADY_INITIALIZED) {
      prism_backend_free(backend);
      backend = NULL_CONSTANT;
    }
  }
  sapi_backend = prism_registry_create(ctx, default_tts_backend);
  if (sapi_backend != NULL_CONSTANT) {
    const PrismError res = prism_backend_initialize(sapi_backend);
    if (res != PRISM_OK && res != PRISM_ERROR_ALREADY_INITIALIZED) {
      prism_backend_free(sapi_backend);
      sapi_backend = NULL_CONSTANT;
    }
  }
  if (backend != NULL_CONSTANT) {
    backend_name = utf8_to_wchar(prism_backend_name(backend));
  }
  if (sapi_backend != NULL_CONSTANT) {
    sapi_backend_name = utf8_to_wchar(prism_backend_name(sapi_backend));
  }
  if (backend != NULL_CONSTANT || sapi_backend != NULL_CONSTANT) {
    loaded = true;
  } else {
    prism_shutdown(ctx);
    ctx = NULL_CONSTANT;
  }
  fast_lock_release(&lock);
}

TOLK_API bool TOLK_CALL Tolk_IsLoaded(void) {
  fast_lock_acquire(&lock);
  const bool result = loaded;
  fast_lock_release(&lock);
  return result;
}

TOLK_API void TOLK_CALL Tolk_Unload(void) {
  fast_lock_acquire(&lock);
  if (!loaded) {
    fast_lock_release(&lock);
    return;
  }
  if (backend != NULL_CONSTANT) {
    prism_backend_free(backend);
    backend = NULL_CONSTANT;
  }
  if (sapi_backend != NULL_CONSTANT) {
    prism_backend_free(sapi_backend);
    sapi_backend = NULL_CONSTANT;
  }
  if (ctx != NULL_CONSTANT) {
    prism_shutdown(ctx);
    ctx = NULL_CONSTANT;
  }
  free(backend_name);
  backend_name = NULL_CONSTANT;
  free(sapi_backend_name);
  sapi_backend_name = NULL_CONSTANT;
  loaded = false;
  fast_lock_release(&lock);
}

TOLK_API void TOLK_CALL Tolk_TrySAPI(bool trySAPI) { (void)trySAPI; }

TOLK_API void TOLK_CALL Tolk_PreferSAPI(bool preferSAPI) {
  fast_lock_acquire(&lock);
  prefer_sapi = preferSAPI;
  fast_lock_release(&lock);
}

TOLK_API const wchar_t *TOLK_CALL Tolk_DetectScreenReader(void) {
  static _Thread_local wchar_t buf[256];
  fast_lock_acquire(&lock);
  if (!loaded) {
    fast_lock_release(&lock);
    return NULL_CONSTANT;
  }
  const wchar_t *name = prefer_sapi ? sapi_backend_name : backend_name;
  if (name == NULL_CONSTANT) {
    fast_lock_release(&lock);
    return NULL_CONSTANT;
  }
  wcsncpy(buf, name, 255);
  buf[255] = L'\0';
  fast_lock_release(&lock);
  return buf;
}

TOLK_API bool TOLK_CALL Tolk_HasSpeech(void) {
  fast_lock_acquire(&lock);
  if (!loaded) {
    fast_lock_release(&lock);
    return false;
  }
  PrismBackend *b = prefer_sapi ? sapi_backend : backend;
  if (b == NULL_CONSTANT) {
    fast_lock_release(&lock);
    return false;
  }
  const uint64_t features = prism_backend_get_features(b);
  fast_lock_release(&lock);
  return (features & PRISM_BACKEND_SUPPORTS_SPEAK) != 0;
}

TOLK_API bool TOLK_CALL Tolk_HasBraille(void) {
  fast_lock_acquire(&lock);
  if (!loaded) {
    fast_lock_release(&lock);
    return false;
  }
  PrismBackend *b = prefer_sapi ? sapi_backend : backend;
  if (b == NULL_CONSTANT) {
    fast_lock_release(&lock);
    return false;
  }
  const uint64_t features = prism_backend_get_features(b);
  fast_lock_release(&lock);
  return (features & PRISM_BACKEND_SUPPORTS_BRAILLE) != 0;
}

TOLK_API bool TOLK_CALL Tolk_Output(const wchar_t *str, bool interrupt) {
  if (str == NULL_CONSTANT)
    return false;
  char *utf8 = wchar_to_utf8(str);
  if (utf8 == NULL_CONSTANT)
    return false;
  fast_lock_acquire(&lock);
  if (!loaded) {
    fast_lock_release(&lock);
    free(utf8);
    return false;
  }
  PrismBackend *b = prefer_sapi ? sapi_backend : backend;
  PrismError err = PRISM_ERROR_NOT_INITIALIZED;
  if (b != NULL_CONSTANT)
    err = prism_backend_output(b, utf8, interrupt);
  fast_lock_release(&lock);
  free(utf8);
  return err == PRISM_OK;
}

TOLK_API bool TOLK_CALL Tolk_Speak(const wchar_t *str, bool interrupt) {
  if (str == NULL_CONSTANT)
    return false;
  char *utf8 = wchar_to_utf8(str);
  if (utf8 == NULL_CONSTANT)
    return false;
  fast_lock_acquire(&lock);
  if (!loaded) {
    fast_lock_release(&lock);
    free(utf8);
    return false;
  }
  PrismBackend *b = prefer_sapi ? sapi_backend : backend;
  PrismError err = PRISM_ERROR_NOT_INITIALIZED;
  if (b != NULL_CONSTANT)
    err = prism_backend_speak(b, utf8, interrupt);
  fast_lock_release(&lock);
  free(utf8);
  return err == PRISM_OK;
}

TOLK_API bool TOLK_CALL Tolk_Braille(const wchar_t *str) {
  if (str == NULL_CONSTANT)
    return false;
  char *utf8 = wchar_to_utf8(str);
  if (utf8 == NULL_CONSTANT)
    return false;
  fast_lock_acquire(&lock);
  if (!loaded) {
    fast_lock_release(&lock);
    free(utf8);
    return false;
  }
  PrismBackend *b = prefer_sapi ? sapi_backend : backend;
  PrismError err = PRISM_ERROR_NOT_INITIALIZED;
  if (b != NULL_CONSTANT)
    err = prism_backend_braille(b, utf8);
  fast_lock_release(&lock);
  free(utf8);
  return err == PRISM_OK;
}

TOLK_API bool TOLK_CALL Tolk_IsSpeaking(void) {
  fast_lock_acquire(&lock);
  if (!loaded) {
    fast_lock_release(&lock);
    return false;
  }
  PrismBackend *b = prefer_sapi ? sapi_backend : backend;
  if (b == NULL_CONSTANT) {
    fast_lock_release(&lock);
    return false;
  }
  bool speaking = false;
  const PrismError err = prism_backend_is_speaking(b, &speaking);
  fast_lock_release(&lock);
  if (err != PRISM_OK)
    return false;
  return speaking;
}

TOLK_API bool TOLK_CALL Tolk_Silence(void) {
  fast_lock_acquire(&lock);
  if (!loaded) {
    fast_lock_release(&lock);
    return false;
  }
  PrismBackend *b = prefer_sapi ? sapi_backend : backend;
  PrismError err = PRISM_ERROR_NOT_INITIALIZED;
  if (b != NULL_CONSTANT)
    err = prism_backend_stop(b);
  fast_lock_release(&lock);
  return err == PRISM_OK;
}
