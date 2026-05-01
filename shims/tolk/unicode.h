// SPDX-License-Identifier: LGPLv3

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

char *utf16_to_utf8_alloc(const uint16_t *src, size_t src_len);
uint16_t *utf8_to_utf16_alloc(const char *src, size_t src_len, size_t *out_len);

#endif // WCONV_H
