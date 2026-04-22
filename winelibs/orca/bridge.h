// SPDX-License-Identifier: MPL-2.0

#ifndef PRISM_ORCA_BRIDGE_H
#define PRISM_ORCA_BRIDGE_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct PrismOrcaDBusInstance PrismOrcaDBusInstance;

bool prism_orca_available(void);
bool prism_orca_create(PrismOrcaDBusInstance **out);
void prism_orca_destroy(PrismOrcaDBusInstance *h);
bool prism_orca_speak(PrismOrcaDBusInstance *h, const char *text);
bool prism_orca_stop(PrismOrcaDBusInstance *h);

#ifdef __cplusplus
}
#endif
#endif