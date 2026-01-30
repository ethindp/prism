// SPDX-License-Identifier: MPL-2.0

#pragma once
#ifndef PRISM_H
#define PRISM_H

#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __ANDROID__
#include <jni.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif
#if defined(__cplusplus)
extern "C" {
#endif

#if defined(_WIN32)
#if defined(PRISM_BUILDING)
#define PRISM_API __declspec(dllexport)
#else
#define PRISM_API __declspec(dllimport)
#endif
#define PRISM_CALL __cdecl
#elif defined(__GNUC__) || defined(__clang__)
#define PRISM_API __attribute__((visibility("default")))
#define PRISM_CALL
#else
#define PRISM_API
#define PRISM_CALL
#endif
#if defined(__GNUC__) || defined(__clang__)
#define PRISM_NODISCARD __attribute__((warn_unused_result))
#define PRISM_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
#define PRISM_PRINTF(fmt, args) __attribute__((format(printf, fmt, args)))
#define PRISM_DEPRECATED(msg) __attribute__((deprecated(msg)))
#define PRISM_MALLOC __attribute__((malloc))
#if __has_attribute(null_terminated_string_arg)
#define PRISM_NULL_TERMINATED_STRING_ARG(n)                                    \
  __attribute__((null_terminated_string_arg(n)))
#else
#define PRISM_NULL_TERMINATED_STRING_ARG(n)
#endif
#elif defined(_MSC_VER)
#define PRISM_NODISCARD _Check_return_
#define PRISM_NONNULL(...)
#define PRISM_PRINTF(fmt, args)
#define PRISM_DEPRECATED(msg) __declspec(deprecated(msg))
#define PRISM_MALLOC __declspec(restrict)
#define PRISM_NULL_TERMINATED_STRING_ARG(n)
#else
#define PRISM_NODISCARD
#define PRISM_NONNULL(...)
#define PRISM_PRINTF(fmt, args)
#define PRISM_DEPRECATED(msg)
#define PRISM_MALLOC
#define PRISM_NULL_TERMINATED_STRING_ARG(n)
#endif
#if defined(__cplusplus)
#define PRISM_RESTRICT
#define PRISM_STATIC_ASSERT static_assert
#elif defined(_MSC_VER)
#define PRISM_RESTRICT __restrict
#define PRISM_STATIC_ASSERT _Static_assert
#else
#define PRISM_RESTRICT restrict
#define PRISM_STATIC_ASSERT _Static_assert
#endif

typedef struct PrismContext PrismContext;
typedef struct PrismBackend PrismBackend;
typedef uint64_t PrismBackendId;
typedef struct {
  uint8_t version;
#ifdef __ANDROID__
  JNIEnv *jni_env;
#endif
#ifdef _WIN32
  HWND hwnd;
#endif
} PrismConfig;

typedef enum PrismError {
  PRISM_OK = 0,
  PRISM_ERROR_NOT_INITIALIZED,
  PRISM_ERROR_INVALID_PARAM,
  PRISM_ERROR_NOT_IMPLEMENTED,
  PRISM_ERROR_NO_VOICES,
  PRISM_ERROR_VOICE_NOT_FOUND,
  PRISM_ERROR_SPEAK_FAILURE,
  PRISM_ERROR_MEMORY_FAILURE,
  PRISM_ERROR_RANGE_OUT_OF_BOUNDS,
  PRISM_ERROR_INTERNAL,
  PRISM_ERROR_NOT_SPEAKING,
  PRISM_ERROR_NOT_PAUSED,
  PRISM_ERROR_ALREADY_PAUSED,
  PRISM_ERROR_INVALID_UTF8,
  PRISM_ERROR_INVALID_OPERATION,
  PRISM_ERROR_ALREADY_INITIALIZED,
  PRISM_ERROR_BACKEND_NOT_AVAILABLE,
  PRISM_ERROR_UNKNOWN,
  PRISM_ERROR_INVALID_AUDIO_FORMAT,
  PRISM_ERROR_COUNT
} PrismError;

typedef void(PRISM_CALL *PrismAudioCallback)(
    void *userdata, const float *PRISM_RESTRICT samples, size_t sample_count,
    size_t channels, size_t sample_rate);

#define PRISM_BACKEND_INVALID UINT64_C(0)
#define PRISM_BACKEND_SAPI UINT64_C(0x1D6DF72422CEEE66)
#define PRISM_BACKEND_AV_SPEECH UINT64_C(0x28E3429577805C24)
#define PRISM_BACKEND_VOICE_OVER UINT64_C(0xCB4897961A754BCB)
#define PRISM_BACKEND_SPEECH_DISPATCHER UINT64_C(0xE3D6F895D949EBFE)
#define PRISM_BACKEND_NVDA UINT64_C(0x89CC19C5C4AC1A56)
#define PRISM_BACKEND_JAWS UINT64_C(0xAC3D60E9BD84B53E)
#define PRISM_BACKEND_ONE_CORE UINT64_C(0x6797D32F0D994CB4)
#define PRISM_BACKEND_ORCA UINT64_C(0x10AA1FC05A17F96C)
#define PRISM_BACKEND_ANDROID_SCREEN_READER UINT64_C(0xD199C175AEEC494B)
#define PRISM_BACKEND_ANDROID_TTS UINT64_C(0xBC175831BFE4E5CC)
#define PRISM_BACKEND_WEB_SPEECH UINT64_C(0x3572538D44D44A8F)
#define PRISM_BACKEND_UIA UINT64_C(0x6238F019DB678F8E)
#define PRISM_BACKEND_ZDSR UINT64_C(0x3D93C56C9E7F2A2E)
#define PRISM_BACKEND_ZOOM_TEXT UINT64_C(0xAE439D62DC7B1479)
#define PRISM_CONFIG_VERSION 1

typedef enum PrismBackendFeature : uint64_t {
  PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME = (1ULL << 0),
  PRISM_BACKEND_SUPPORTS_SPEAK = (1ULL << 2),
  PRISM_BACKEND_SUPPORTS_SPEAK_TO_MEMORY = (1ULL << 3),
  PRISM_BACKEND_SUPPORTS_BRAILLE = (1ULL << 4),
  PRISM_BACKEND_SUPPORTS_OUTPUT = (1ULL << 5),
  PRISM_BACKEND_SUPPORTS_IS_SPEAKING = (1ULL << 6),
  PRISM_BACKEND_SUPPORTS_STOP = (1ULL << 7),
  PRISM_BACKEND_SUPPORTS_PAUSE = (1ULL << 8),
  PRISM_BACKEND_SUPPORTS_RESUME = (1ULL << 9),
  PRISM_BACKEND_SUPPORTS_SET_VOLUME = (1ULL << 10),
  PRISM_BACKEND_SUPPORTS_GET_VOLUME = (1ULL << 11),
  PRISM_BACKEND_SUPPORTS_SET_RATE = (1ULL << 12),
  PRISM_BACKEND_SUPPORTS_GET_RATE = (1ULL << 13),
  PRISM_BACKEND_SUPPORTS_SET_PITCH = (1ULL << 14),
  PRISM_BACKEND_SUPPORTS_GET_PITCH = (1ULL << 15),
  PRISM_BACKEND_SUPPORTS_REFRESH_VOICES = (1ULL << 16),
  PRISM_BACKEND_SUPPORTS_COUNT_VOICES = (1ULL << 17),
  PRISM_BACKEND_SUPPORTS_GET_VOICE_NAME = (1ULL << 18),
  PRISM_BACKEND_SUPPORTS_GET_VOICE_LANGUAGE = (1ULL << 19),
  PRISM_BACKEND_SUPPORTS_GET_VOICE = (1ULL << 20),
  PRISM_BACKEND_SUPPORTS_SET_VOICE = (1ULL << 21),
  PRISM_BACKEND_SUPPORTS_GET_CHANNELS = (1ULL << 22),
  PRISM_BACKEND_SUPPORTS_GET_SAMPLE_RATE = (1ULL << 23),
  PRISM_BACKEND_SUPPORTS_GET_BIT_DEPTH = (1ULL << 24),
  PRISM_BACKEND_PERFORMS_SILENCE_TRIMMING_ON_SPEAK = (1ULL << 25),
  PRISM_BACKEND_PERFORMS_SILENCE_TRIMMING_ON_SPEAK_TO_MEMORY = (1ULL << 26),
  PRISM_BACKEND_SUPPORTS_SPEAK_SSML = (1ULL << 27),
  PRISM_BACKEND_SUPPORTS_SPEAK_TO_MEMORY_SSML = (1ULL << 28)
} PrismBackendFeature;

PRISM_STATIC_ASSERT(sizeof(PrismBackendId) == 8,
                    "PrismBackendId must be 64 bits");
PRISM_STATIC_ASSERT(alignof(PrismBackendId) >= 4, "PrismBackendId alignment");
PRISM_STATIC_ASSERT(PRISM_OK == 0, "PRISM_OK must be zero");

PRISM_API PRISM_NODISCARD PrismConfig PRISM_CALL prism_config_init(void);

PRISM_API PRISM_NODISCARD PRISM_MALLOC PrismContext *PRISM_CALL
prism_init(PrismConfig *cfg);

PRISM_API
void PRISM_CALL prism_shutdown(PrismContext *ctx);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1) size_t PRISM_CALL
    prism_registry_count(PrismContext *ctx);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1) PrismBackendId PRISM_CALL
    prism_registry_id_at(PrismContext *ctx, size_t index);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1, 2)
    PRISM_NULL_TERMINATED_STRING_ARG(2) PrismBackendId PRISM_CALL
    prism_registry_id(PrismContext *ctx, const char *PRISM_RESTRICT name);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1) const char *PRISM_CALL
    prism_registry_name(PrismContext *ctx, PrismBackendId id);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1) int PRISM_CALL
    prism_registry_priority(PrismContext *ctx, PrismBackendId id);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1) bool PRISM_CALL
    prism_registry_exists(PrismContext *ctx, PrismBackendId id);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1) PrismBackend *PRISM_CALL
    prism_registry_get(PrismContext *ctx, PrismBackendId id);

PRISM_API PRISM_NODISCARD PRISM_MALLOC PRISM_NONNULL(1) PrismBackend *PRISM_CALL
    prism_registry_create(PrismContext *ctx, PrismBackendId id);

PRISM_API PRISM_NODISCARD PRISM_MALLOC PRISM_NONNULL(1) PrismBackend *PRISM_CALL
    prism_registry_create_best(PrismContext *ctx);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1) PrismBackend *PRISM_CALL
    prism_registry_acquire(PrismContext *ctx, PrismBackendId id);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1) PrismBackend *PRISM_CALL
    prism_registry_acquire_best(PrismContext *ctx);

PRISM_API
void PRISM_CALL prism_backend_free(PrismBackend *backend);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1) const char *PRISM_CALL
    prism_backend_name(PrismBackend *backend);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1) uint64_t PRISM_CALL
    prism_backend_get_features(PrismBackend *backend);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1) PrismError PRISM_CALL
    prism_backend_initialize(PrismBackend *backend);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1, 2)
    PRISM_NULL_TERMINATED_STRING_ARG(2) PrismError PRISM_CALL
    prism_backend_speak(PrismBackend *backend, const char *PRISM_RESTRICT text,
                        bool interrupt);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1, 2, 3)
    PRISM_NULL_TERMINATED_STRING_ARG(2) PrismError PRISM_CALL
    prism_backend_speak_to_memory(PrismBackend *backend,
                                  const char *PRISM_RESTRICT text,
                                  PrismAudioCallback callback, void *userdata);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1, 2)
    PRISM_NULL_TERMINATED_STRING_ARG(2) PrismError PRISM_CALL
    prism_backend_braille(PrismBackend *backend,
                          const char *PRISM_RESTRICT text);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1, 2)
    PRISM_NULL_TERMINATED_STRING_ARG(2) PrismError PRISM_CALL
    prism_backend_output(PrismBackend *backend, const char *PRISM_RESTRICT text,
                         bool interrupt);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1) PrismError PRISM_CALL
    prism_backend_stop(PrismBackend *backend);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1) PrismError PRISM_CALL
    prism_backend_pause(PrismBackend *backend);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1) PrismError PRISM_CALL
    prism_backend_resume(PrismBackend *backend);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1, 2) PrismError PRISM_CALL
    prism_backend_is_speaking(PrismBackend *backend,
                              bool *PRISM_RESTRICT out_speaking);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1) PrismError PRISM_CALL
    prism_backend_set_volume(PrismBackend *backend, float volume);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1) PrismError PRISM_CALL
    prism_backend_set_rate(PrismBackend *backend, float rate);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1) PrismError PRISM_CALL
    prism_backend_set_pitch(PrismBackend *backend, float pitch);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1, 2) PrismError PRISM_CALL
    prism_backend_get_volume(PrismBackend *backend,
                             float *PRISM_RESTRICT out_volume);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1, 2) PrismError PRISM_CALL
    prism_backend_get_rate(PrismBackend *backend,
                           float *PRISM_RESTRICT out_rate);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1, 2) PrismError PRISM_CALL
    prism_backend_get_pitch(PrismBackend *backend,
                            float *PRISM_RESTRICT out_pitch);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1) PrismError PRISM_CALL
    prism_backend_refresh_voices(PrismBackend *backend);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1, 2) PrismError PRISM_CALL
    prism_backend_count_voices(PrismBackend *backend,
                               size_t *PRISM_RESTRICT out_count);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1, 3) PrismError PRISM_CALL
    prism_backend_get_voice_name(PrismBackend *backend, size_t voice_id,
                                 const char **PRISM_RESTRICT out_name);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1, 3) PrismError PRISM_CALL
    prism_backend_get_voice_language(PrismBackend *backend, size_t voice_id,
                                     const char **PRISM_RESTRICT out_language);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1) PrismError PRISM_CALL
    prism_backend_set_voice(PrismBackend *backend, size_t voice_id);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1, 2) PrismError PRISM_CALL
    prism_backend_get_voice(PrismBackend *backend,
                            size_t *PRISM_RESTRICT out_voice_id);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1, 2) PrismError PRISM_CALL
    prism_backend_get_channels(PrismBackend *backend,
                               size_t *PRISM_RESTRICT out_channels);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1, 2) PrismError PRISM_CALL
    prism_backend_get_sample_rate(PrismBackend *backend,
                                  size_t *PRISM_RESTRICT out_sample_rate);

PRISM_API PRISM_NODISCARD PRISM_NONNULL(1, 2) PrismError PRISM_CALL
    prism_backend_get_bit_depth(PrismBackend *backend,
                                size_t *PRISM_RESTRICT out_bit_depth);

PRISM_API PRISM_NODISCARD const char *PRISM_CALL
prism_error_string(PrismError error);

#if defined(__cplusplus)
}
#endif

#endif