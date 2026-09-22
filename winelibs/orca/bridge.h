// SPDX-License-Identifier: MPL-2.0

#ifndef PRISM_ORCA_BRIDGE_H
#define PRISM_ORCA_BRIDGE_H
#include <stdbool.h>

#if defined(__x86_64__) && !defined(PRISM_ORCA_BRIDGE_NATIVE)
#define PRISM_WINELIB_ABI __attribute__((ms_abi))
#else
#define PRISM_WINELIB_ABI
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PrismOrcaDBusInstance PrismOrcaDBusInstance;

PRISM_WINELIB_ABI bool prism_orca_available(void);
PRISM_WINELIB_ABI bool prism_orca_create(PrismOrcaDBusInstance **out);
PRISM_WINELIB_ABI void prism_orca_destroy(PrismOrcaDBusInstance *instance);
PRISM_WINELIB_ABI bool prism_orca_speak(PrismOrcaDBusInstance *instance,
                                        const char *text);
PRISM_WINELIB_ABI bool prism_orca_stop(PrismOrcaDBusInstance *instance);

#ifdef __cplusplus
}
#endif
#endif