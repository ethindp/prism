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

typedef struct ShimAudioBuffer {
  float *samples;
  size_t sample_count;
  size_t capacity;
  size_t channels;
  size_t sample_rate;
  bool failed;
} ShimAudioBuffer;

PrismBackendId shim_native_tts_id(void);
PrismBackend *shim_create_initialized(PrismContext *ctx, PrismBackendId id);
char *shim_wchar_to_utf8(const wchar_t *src);
wchar_t *shim_utf8_to_wchar(const char *src);
char *shim_ansi_to_utf8(const char *text);
char *shim_utf8_to_ansi(const char *text);
char *shim_strdup(const char *text);
float shim_clamp01(float value);

static inline void shim_ignore_error(PrismError error) { (void)error; }

void shim_audio_buffer_init(ShimAudioBuffer *buffer);
void shim_audio_buffer_clear(ShimAudioBuffer *buffer);
void PRISM_CALL shim_audio_accumulate(void *userdata, const float *samples,
                                      size_t sample_count, size_t channels,
                                      size_t sample_rate);
bool shim_synthesize_audio(PrismBackend *backend, const char *text,
                           ShimAudioBuffer *buffer);
bool shim_audio_to_pcm16_mono(const ShimAudioBuffer *buffer,
                              size_t target_sample_rate, int16_t **out_samples,
                              size_t *out_sample_count);
bool shim_write_wav(const char *path, const ShimAudioBuffer *buffer);

#endif
