// SPDX-License-Identifier: MPL-2.0

#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  AVSPEECH_OK = 0,
  AVSPEECH_ERROR_NOT_INITIALIZED,
  AVSPEECH_ERROR_INVALID_PARAM,
  AVSPEECH_ERROR_NOT_IMPLEMENTED,
  AVSPEECH_ERROR_NO_VOICES,
  AVSPEECH_ERROR_VOICE_NOT_FOUND,
  AVSPEECH_ERROR_SPEAK_FAILED,
  AVSPEECH_ERROR_MEMORY_FAILED,
  AVSPEECH_ERROR_UNKNOWN
} AVSpeechError;

typedef struct AVSpeechContext AVSpeechContext;

typedef void (*AVSpeechAudioCallback)(void *userdata, const float *samples,
                                      size_t sample_count, int channels,
                                      int sample_rate);

AVSpeechError avspeech_initialize(AVSpeechContext **ctx);
AVSpeechError avspeech_cleanup(AVSpeechContext *ctx);
AVSpeechError avspeech_speak(AVSpeechContext *ctx, const char *text);
AVSpeechError avspeech_speak_to_memory(AVSpeechContext *ctx, const char *text,
                                       AVSpeechAudioCallback callback,
                                       void *userdata);
AVSpeechError avspeech_stop(AVSpeechContext *ctx);
AVSpeechError avspeech_pause(AVSpeechContext *ctx);
AVSpeechError avspeech_resume(AVSpeechContext *ctx);
bool avspeech_is_speaking(AVSpeechContext *ctx);
AVSpeechError avspeech_set_volume(AVSpeechContext *ctx, float volume);
AVSpeechError avspeech_get_volume(AVSpeechContext *ctx, float *volume);
AVSpeechError avspeech_set_pitch(AVSpeechContext *ctx, float pitch);
AVSpeechError avspeech_get_pitch(AVSpeechContext *ctx, float *pitch);
AVSpeechError avspeech_set_rate(AVSpeechContext *ctx, float rate);
AVSpeechError avspeech_get_rate(AVSpeechContext *ctx, float *rate);
float avspeech_get_rate_min(void);
float avspeech_get_rate_max(void);
float avspeech_get_rate_default(void);
AVSpeechError avspeech_refresh_voices(AVSpeechContext *ctx);
AVSpeechError avspeech_count_voices(AVSpeechContext *ctx, int *count);
AVSpeechError avspeech_get_voice_name(AVSpeechContext *ctx, int voice_id,
                                      const char **name);
AVSpeechError avspeech_get_voice_language(AVSpeechContext *ctx, int voice_id,
                                          const char **language);
AVSpeechError avspeech_set_voice(AVSpeechContext *ctx, int voice_id);
AVSpeechError avspeech_get_voice(AVSpeechContext *ctx, int *voice_id);
AVSpeechError avspeech_get_channels(AVSpeechContext *ctx, int *channels);
AVSpeechError avspeech_get_sample_rate(AVSpeechContext *ctx, int *sample_rate);
AVSpeechError avspeech_get_bit_depth(AVSpeechContext *ctx, int *bit_depth);

#ifdef __cplusplus
}
#endif