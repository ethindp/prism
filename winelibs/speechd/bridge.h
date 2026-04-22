// SPDX-License-Identifier: MPL-2.0

#ifndef prism_speechd_BRIDGE_H
#define prism_speechd_BRIDGE_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct PrismSpeechDispatcherInstance PrismSpeechDispatcherInstance;

bool prism_speechd_available(void);
bool prism_speechd_create(PrismSpeechDispatcherInstance **out);
void prism_speechd_destroy(PrismSpeechDispatcherInstance *h);
bool prism_speechd_speak(PrismSpeechDispatcherInstance *h, const char *text);
bool prism_speechd_stop(PrismSpeechDispatcherInstance *h);

#ifdef __cplusplus
}
#endif
#endif