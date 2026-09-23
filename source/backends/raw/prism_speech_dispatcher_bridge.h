// SPDX-License-Identifier: MPL-2.0

#ifndef PRISM_SPEECHD_BRIDGE_H
#define PRISM_SPEECHD_BRIDGE_H
#include "prism_winelib_status.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PrismSpeechDispatcherInstance PrismSpeechDispatcherInstance;

__declspec(dllimport) uint32_t prism_speechd_abi_version(void)
    PRISM_WINELIB_NOEXCEPT;
__declspec(dllimport) PrismWinelibStatus prism_speechd_available(void)
    PRISM_WINELIB_NOEXCEPT;
__declspec(dllimport) PrismWinelibStatus prism_speechd_create(
    PrismSpeechDispatcherInstance **out) PRISM_WINELIB_NOEXCEPT;
__declspec(dllimport) void
prism_speechd_destroy(PrismSpeechDispatcherInstance *h) PRISM_WINELIB_NOEXCEPT;
__declspec(dllimport) PrismWinelibStatus
prism_speechd_speak(PrismSpeechDispatcherInstance *h, const char *text,
                    int32_t interrupt) PRISM_WINELIB_NOEXCEPT;
__declspec(dllimport) PrismWinelibStatus
prism_speechd_stop(PrismSpeechDispatcherInstance *h) PRISM_WINELIB_NOEXCEPT;
__declspec(dllimport) PrismWinelibStatus
prism_speechd_pause(PrismSpeechDispatcherInstance *h) PRISM_WINELIB_NOEXCEPT;
__declspec(dllimport) PrismWinelibStatus
prism_speechd_resume(PrismSpeechDispatcherInstance *h) PRISM_WINELIB_NOEXCEPT;
__declspec(dllimport) PrismWinelibStatus prism_speechd_set_volume(
    PrismSpeechDispatcherInstance *h, float value) PRISM_WINELIB_NOEXCEPT;
__declspec(dllimport) PrismWinelibStatus prism_speechd_get_volume(
    PrismSpeechDispatcherInstance *h, float *out) PRISM_WINELIB_NOEXCEPT;
__declspec(dllimport) PrismWinelibStatus prism_speechd_set_rate(
    PrismSpeechDispatcherInstance *h, float value) PRISM_WINELIB_NOEXCEPT;
__declspec(dllimport) PrismWinelibStatus prism_speechd_get_rate(
    PrismSpeechDispatcherInstance *h, float *out) PRISM_WINELIB_NOEXCEPT;
__declspec(dllimport) PrismWinelibStatus prism_speechd_set_pitch(
    PrismSpeechDispatcherInstance *h, float value) PRISM_WINELIB_NOEXCEPT;
__declspec(dllimport) PrismWinelibStatus prism_speechd_get_pitch(
    PrismSpeechDispatcherInstance *h, float *out) PRISM_WINELIB_NOEXCEPT;
__declspec(dllimport) PrismWinelibStatus prism_speechd_refresh_voices(
    PrismSpeechDispatcherInstance *h) PRISM_WINELIB_NOEXCEPT;
__declspec(dllimport) PrismWinelibStatus prism_speechd_count_voices(
    PrismSpeechDispatcherInstance *h, uint32_t *out) PRISM_WINELIB_NOEXCEPT;
__declspec(dllimport) PrismWinelibStatus prism_speechd_get_voice_name(
    PrismSpeechDispatcherInstance *h, uint32_t index, char *buf, uint32_t cap,
    uint32_t *needed) PRISM_WINELIB_NOEXCEPT;
__declspec(dllimport) PrismWinelibStatus prism_speechd_get_voice_language(
    PrismSpeechDispatcherInstance *h, uint32_t index, char *buf, uint32_t cap,
    uint32_t *needed) PRISM_WINELIB_NOEXCEPT;
__declspec(dllimport) PrismWinelibStatus prism_speechd_set_voice(
    PrismSpeechDispatcherInstance *h, uint32_t index) PRISM_WINELIB_NOEXCEPT;
__declspec(dllimport) PrismWinelibStatus prism_speechd_get_voice(
    PrismSpeechDispatcherInstance *h, uint32_t *out) PRISM_WINELIB_NOEXCEPT;

#ifdef __cplusplus
}
#endif
#endif
