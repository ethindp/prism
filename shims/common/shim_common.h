// SPDX-License-Identifier: MPL-2.0
#ifndef PRISM_SHIM_COMMON_H
#define PRISM_SHIM_COMMON_H

#include <prism.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#ifdef _MSC_VER
#define PRISM_SHIM_NULL NULL
#else
#define PRISM_SHIM_NULL nullptr
#endif

typedef struct ShimFlag {
#ifdef _WIN32
  _Alignas(4) volatile LONG value;
#else
  _Alignas(4) int value;
#endif
} ShimFlag;

typedef struct ShimSelection {
  PrismBackend *backend;
  PrismBackendId id;
} ShimSelection;

typedef struct ShimString ShimString;

typedef struct ShimStrings {
  ShimString *head;
} ShimStrings;

typedef bool (*ShimAcceptFn)(PrismBackendId id, void *userdata);

typedef struct ShimAudioBuffer {
  float *samples;
  size_t sample_count;
  size_t capacity;
  size_t channels;
  size_t sample_rate;
  bool failed;
} ShimAudioBuffer;

void shim_flag_raise(ShimFlag *flag);
bool shim_flag_take(ShimFlag *flag);

PrismContext *shim_context_open(ShimFlag *stale);
PrismBackendId shim_native_tts_id(void);
bool shim_error_means_lost(PrismError error);
bool shim_backend_live(PrismBackend *backend);
PrismBackend *shim_create_live(PrismContext *ctx, PrismBackendId id);
PrismBackend *shim_slot_get(PrismContext *ctx, PrismBackendId id,
                            PrismBackend **slot);
bool shim_slot_live(PrismContext *ctx, PrismBackendId id, PrismBackend **slot);
void shim_slot_drop(PrismBackend **slot);
void shim_select(PrismContext *ctx, ShimSelection *selection,
                 ShimAcceptFn accept, void *userdata);
void shim_selection_clear(ShimSelection *selection);

const wchar_t *shim_intern_wide(ShimStrings *strings, const char *utf8);
const char *shim_intern_utf8(ShimStrings *strings, const char *utf8);
const char *shim_intern_ansi(ShimStrings *strings, const char *utf8);
void shim_strings_free(ShimStrings *strings);

char *shim_wchar_to_utf8(const wchar_t *src);
wchar_t *shim_utf8_to_wchar(const char *src);
char *shim_ansi_to_utf8(const char *text);
char *shim_utf8_to_ansi(const char *text);
char *shim_strdup(const char *text);
float shim_clamp01(float value);
float shim_to_unit(float value, float min, float max);
float shim_from_unit(float value, float min, float max);

static inline void shim_ignore_error(PrismError error) { (void)error; }

void shim_audio_buffer_init(ShimAudioBuffer *buffer);
void shim_audio_buffer_clear(ShimAudioBuffer *buffer);
void PRISM_CALL shim_audio_accumulate(void *userdata,
                                      const float *PRISM_RESTRICT samples,
                                      size_t sample_count, size_t channels,
                                      size_t sample_rate);
bool shim_synthesize_audio(PrismBackend *backend, const char *text,
                           ShimAudioBuffer *buffer);
bool shim_audio_to_pcm16_mono(const ShimAudioBuffer *buffer,
                              size_t target_sample_rate, int16_t **out_samples,
                              size_t *out_sample_count);
bool shim_write_wav(const char *path, const ShimAudioBuffer *buffer);

#endif
