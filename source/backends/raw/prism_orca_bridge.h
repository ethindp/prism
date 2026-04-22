// SPDX-License-Identifier: MPL-2.0

#ifndef PRISM_ORCA_BRIDGE_H
#define PRISM_ORCA_BRIDGE_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct PrismOrcaDBusInstance PrismOrcaDBusInstance;

__declspec(dllimport) bool prism_orca_available(void);
__declspec(dllimport) bool prism_orca_create(PrismOrcaDBusInstance **out);
__declspec(dllimport) void prism_orca_destroy(PrismOrcaDBusInstance *h);
__declspec(dllimport) bool prism_orca_speak(PrismOrcaDBusInstance *h,
                                            const char *text);
__declspec(dllimport) bool prism_orca_stop(PrismOrcaDBusInstance *h);

#ifdef __cplusplus
}
#endif
#endif