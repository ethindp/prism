// SPDX-License-Identifier: LGPLv3

#include "unicode.h"
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef _WIN32
#include <windows.h>
#endif

void utf_locale_save(UnicodeLocale *loc) {
  const char *cur = setlocale(LC_CTYPE, NULL);
#ifdef _WIN32
  loc->saved = cur ? _strdup(cur) : NULL;
#else
  loc->saved = cur ? strdup(cur) : NULL;
#endif
}

void utf_locale_set_utf8(void) {
#ifndef _WIN32
  if (!setlocale(LC_CTYPE, "C.UTF-8"))
    if (!setlocale(LC_CTYPE, "en_US.UTF-8"))
      setlocale(LC_CTYPE, "");
#endif
}

void utf_locale_restore(UnicodeLocale *loc) {
  if (loc->saved) {
    setlocale(LC_CTYPE, loc->saved);
    free(loc->saved);
    loc->saved = NULL;
  }
}

#ifdef _WIN32
char *utf16_to_utf8_alloc(const uint16_t *src, size_t src_len) {
  if (!src)
    return NULL;
  if (src_len == (size_t)-1) {
    src_len = 0;
    while (src[src_len])
      src_len++;
  }
  int n = WideCharToMultiByte(CP_UTF8, 0, (const wchar_t *)src, (int)src_len,
                              NULL, 0, NULL, NULL);
  if (n <= 0)
    return NULL;
  char *buf = (char *)malloc((size_t)n + 1);
  if (!buf)
    return NULL;
  WideCharToMultiByte(CP_UTF8, 0, (const wchar_t *)src, (int)src_len, buf, n,
                      NULL, NULL);
  buf[n] = '\0';
  return buf;
}

uint16_t *utf8_to_utf16_alloc(const char *src, size_t src_len,
                              size_t *out_len) {
  if (!src)
    return NULL;
  if (src_len == (size_t)-1)
    src_len = strlen(src);
  int n = MultiByteToWideChar(CP_UTF8, 0, src, (int)src_len, NULL, 0);
  if (n <= 0)
    return NULL;
  uint16_t *buf = (uint16_t *)malloc(((size_t)n + 1) * sizeof(uint16_t));
  if (!buf)
    return NULL;
  MultiByteToWideChar(CP_UTF8, 0, src, (int)src_len, (wchar_t *)buf, n);
  buf[n] = 0;
  if (out_len)
    *out_len = (size_t)n;
  return buf;
}
#else
char *utf16_to_utf8_alloc(const uint16_t *src, size_t src_len) {
  if (!src)
    return NULL;
  if (src_len == (size_t)-1) {
    src_len = 0;
    while (src[src_len])
      src_len++;
  }
  wchar_t *wbuf = (wchar_t *)malloc((src_len + 1) * sizeof(wchar_t));
  if (!wbuf)
    return NULL;
  size_t wi = 0;
  for (size_t i = 0; i < src_len; i++) {
    uint16_t w = src[i];
    if (w >= 0xD800 && w <= 0xDBFF) {
      if (i + 1 >= src_len) {
        free(wbuf);
        return NULL;
      }
      uint16_t w2 = src[++i];
      if (w2 < 0xDC00 || w2 > 0xDFFF) {
        free(wbuf);
        return NULL;
      }
      wbuf[wi++] =
          (wchar_t)(0x10000 + ((uint32_t)(w - 0xD800) << 10) + (w2 - 0xDC00));
    } else if (w >= 0xDC00 && w <= 0xDFFF) {
      free(wbuf);
      return NULL;
    } else {
      wbuf[wi++] = (wchar_t)w;
    }
  }
  wbuf[wi] = L'\0';
  size_t utf8_len = wcstombs(NULL, wbuf, 0);
  if (utf8_len == (size_t)-1) {
    free(wbuf);
    return NULL;
  }
  char *buf = (char *)malloc(utf8_len + 1);
  if (!buf) {
    free(wbuf);
    return NULL;
  }
  wcstombs(buf, wbuf, utf8_len + 1);
  free(wbuf);
  return buf;
}

uint16_t *utf8_to_utf16_alloc(const char *src, size_t src_len,
                              size_t *out_len) {
  if (!src)
    return NULL;
  if (src_len == (size_t)-1)
    src_len = strlen(src);
  char *tmp = (char *)malloc(src_len + 1);
  if (!tmp)
    return NULL;
  memcpy(tmp, src, src_len);
  tmp[src_len] = '\0';
  size_t wlen = mbstowcs(NULL, tmp, 0);
  if (wlen == (size_t)-1) {
    free(tmp);
    return NULL;
  }
  wchar_t *wbuf = (wchar_t *)malloc((wlen + 1) * sizeof(wchar_t));
  if (!wbuf) {
    free(tmp);
    return NULL;
  }
  mbstowcs(wbuf, tmp, wlen + 1);
  free(tmp);
  size_t u16_len = 0;
  for (size_t i = 0; i < wlen; i++)
    u16_len += ((uint32_t)wbuf[i] >= 0x10000) ? 2 : 1;
  uint16_t *buf = (uint16_t *)malloc((u16_len + 1) * sizeof(uint16_t));
  if (!buf) {
    free(wbuf);
    return NULL;
  }
  size_t j = 0;
  for (size_t i = 0; i < wlen; i++) {
    uint32_t cp = (uint32_t)wbuf[i];
    if (cp >= 0x10000) {
      cp -= 0x10000;
      buf[j++] = (uint16_t)(0xD800 + (cp >> 10));
      buf[j++] = (uint16_t)(0xDC00 + (cp & 0x3FF));
    } else {
      buf[j++] = (uint16_t)cp;
    }
  }
  buf[j] = 0;
  free(wbuf);
  if (out_len)
    *out_len = u16_len;
  return buf;
}
#endif
