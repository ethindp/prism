// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_OSX
uint8_t voiceover_macos_initialize(void);
uint8_t voiceover_macos_speak(const char* text, bool interrupt);
uint8_t voiceover_macos_stop(void);
uint8_t voiceover_macos_is_speaking(bool* out);
void voiceover_macos_shutdown(void);
#else
uint8_t voiceover_ios_initialize(void);
uint8_t voiceover_ios_speak(const char* text, bool interrupt);
uint8_t voiceover_ios_stop(void);
uint8_t voiceover_ios_is_speaking(bool* out);
void voiceover_ios_shutdown(void);
#endif
#endif

#ifdef __cplusplus
}
#endif