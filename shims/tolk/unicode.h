// SPDX-License-Identifier: LGPLv3
// Portable UTF-8 <-> UTF-16 conversion.

#pragma once
#ifndef WCONV_H
#define WCONV_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
  char *saved;
} UnicodeLocale;

void utf_locale_save(UnicodeLocale *loc);
void utf_locale_set_utf8(void);
void utf_locale_restore(UnicodeLocale *loc);

// UTF-16 -> UTF-8. Returns malloc'd NUL-terminated string. Caller frees.
// src_len is in uint16_t units, or (size_t)-1 if NUL-terminated.
// Returns NULL on error.
char *utf16_to_utf8_alloc(const uint16_t *src, size_t src_len);

// UTF-8 -> UTF-16. Returns malloc'd NUL-terminated uint16_t array. Caller
// frees. src_len is in bytes, or (size_t)-1 if NUL-terminated. If out_len is
// non-NULL, receives the number of uint16_t (excluding NUL). Returns NULL on
// error.
uint16_t *utf8_to_utf16_alloc(const char *src, size_t src_len, size_t *out_len);

#endif // WCONV_H
