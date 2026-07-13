// SPDX-License-Identifier: MPL-2.0

#ifndef prism_speechd_BRIDGE_H
#define prism_speechd_BRIDGE_H
#include <stdbool.h>

#if defined(__x86_64__)
#define PRISM_WINELIB_ABI __attribute__((ms_abi))
#else
#define PRISM_WINELIB_ABI
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PrismSpeechDispatcherInstance PrismSpeechDispatcherInstance;

PRISM_WINELIB_ABI bool prism_speechd_available(void);
PRISM_WINELIB_ABI bool
prism_speechd_create(PrismSpeechDispatcherInstance **out);
PRISM_WINELIB_ABI void prism_speechd_destroy(PrismSpeechDispatcherInstance *h);
PRISM_WINELIB_ABI bool prism_speechd_speak(PrismSpeechDispatcherInstance *h,
                                           const char *text);
PRISM_WINELIB_ABI bool prism_speechd_stop(PrismSpeechDispatcherInstance *h);

#ifdef __cplusplus
}
#endif
#endif