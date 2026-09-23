// SPDX-License-Identifier: MPL-2.0

#ifndef PRISM_SPIEL_BRIDGE_H
#define PRISM_SPIEL_BRIDGE_H
#include <prism_winelib_status.h>
#include <stdint.h>

#if defined(__x86_64__)
#define PRISM_WINELIB_ABI __attribute__((ms_abi))
#else
#define PRISM_WINELIB_ABI
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PrismSpielInstance PrismSpielInstance;

PRISM_WINELIB_ABI uint32_t prism_spiel_abi_version(void) PRISM_WINELIB_NOEXCEPT;
PRISM_WINELIB_ABI PrismWinelibStatus prism_spiel_available(void)
    PRISM_WINELIB_NOEXCEPT;
PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_create(PrismSpielInstance **out) PRISM_WINELIB_NOEXCEPT;
PRISM_WINELIB_ABI void
prism_spiel_destroy(PrismSpielInstance *h) PRISM_WINELIB_NOEXCEPT;
PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_speak(PrismSpielInstance *h, const char *text,
                  int32_t interrupt) PRISM_WINELIB_NOEXCEPT;
PRISM_WINELIB_ABI PrismWinelibStatus prism_spiel_stop(PrismSpielInstance *h)
    PRISM_WINELIB_NOEXCEPT;
PRISM_WINELIB_ABI PrismWinelibStatus prism_spiel_pause(PrismSpielInstance *h)
    PRISM_WINELIB_NOEXCEPT;
PRISM_WINELIB_ABI PrismWinelibStatus prism_spiel_resume(PrismSpielInstance *h)
    PRISM_WINELIB_NOEXCEPT;
PRISM_WINELIB_ABI PrismWinelibStatus prism_spiel_is_speaking(
    PrismSpielInstance *h, int32_t *out) PRISM_WINELIB_NOEXCEPT;
PRISM_WINELIB_ABI PrismWinelibStatus prism_spiel_set_volume(
    PrismSpielInstance *h, float value) PRISM_WINELIB_NOEXCEPT;
PRISM_WINELIB_ABI PrismWinelibStatus prism_spiel_get_volume(
    PrismSpielInstance *h, float *out) PRISM_WINELIB_NOEXCEPT;
PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_set_rate(PrismSpielInstance *h, float value) PRISM_WINELIB_NOEXCEPT;
PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_get_rate(PrismSpielInstance *h, float *out) PRISM_WINELIB_NOEXCEPT;
PRISM_WINELIB_ABI PrismWinelibStatus prism_spiel_set_pitch(
    PrismSpielInstance *h, float value) PRISM_WINELIB_NOEXCEPT;
PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_get_pitch(PrismSpielInstance *h, float *out) PRISM_WINELIB_NOEXCEPT;
PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_refresh_voices(PrismSpielInstance *h) PRISM_WINELIB_NOEXCEPT;
PRISM_WINELIB_ABI PrismWinelibStatus prism_spiel_count_voices(
    PrismSpielInstance *h, uint32_t *out) PRISM_WINELIB_NOEXCEPT;
PRISM_WINELIB_ABI PrismWinelibStatus prism_spiel_get_voice_name(
    PrismSpielInstance *h, uint32_t index, char *buf, uint32_t cap,
    uint32_t *needed) PRISM_WINELIB_NOEXCEPT;
PRISM_WINELIB_ABI PrismWinelibStatus prism_spiel_get_voice_language(
    PrismSpielInstance *h, uint32_t index, char *buf, uint32_t cap,
    uint32_t *needed) PRISM_WINELIB_NOEXCEPT;
PRISM_WINELIB_ABI PrismWinelibStatus prism_spiel_set_voice(
    PrismSpielInstance *h, uint32_t index) PRISM_WINELIB_NOEXCEPT;
PRISM_WINELIB_ABI PrismWinelibStatus prism_spiel_get_voice(
    PrismSpielInstance *h, uint32_t *out) PRISM_WINELIB_NOEXCEPT;

#ifdef __cplusplus
}
#endif
#endif
