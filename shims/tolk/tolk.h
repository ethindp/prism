// SPDX-License-Identifier: LGPLv3

#ifndef TOLK_H
#define TOLK_H

#if defined(_WIN32)
#ifdef TOLK_BUILDING
#define TOLK_API __declspec(dllexport)
#else
#define TOLK_API __declspec(dllimport)
#endif
#define TOLK_CALL __cdecl
#elif defined(__GNUC__) || defined(__clang__)
#define TOLK_API __attribute__((visibility("default")))
#define TOLK_CALL
#else
#define TOLK_API
#define TOLK_CALL
#endif

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#include <wchar.h>
#endif

TOLK_API void TOLK_CALL Tolk_Load(void);
TOLK_API bool TOLK_CALL Tolk_IsLoaded(void);
TOLK_API void TOLK_CALL Tolk_Unload(void);
TOLK_API void TOLK_CALL Tolk_TrySAPI(bool trySAPI);
TOLK_API void TOLK_CALL Tolk_PreferSAPI(bool preferSAPI);
TOLK_API const wchar_t *TOLK_CALL Tolk_DetectScreenReader(void);
TOLK_API bool TOLK_CALL Tolk_HasSpeech(void);
TOLK_API bool TOLK_CALL Tolk_HasBraille(void);
#ifdef __cplusplus
TOLK_API bool TOLK_CALL Tolk_Output(const wchar_t *str, bool interrupt = false);
TOLK_API bool TOLK_CALL Tolk_Speak(const wchar_t *str, bool interrupt = false);
#else
TOLK_API bool TOLK_CALL Tolk_Output(const wchar_t *str, bool interrupt);
TOLK_API bool TOLK_CALL Tolk_Speak(const wchar_t *str, bool interrupt);
#endif
TOLK_API bool TOLK_CALL Tolk_Braille(const wchar_t *str);
TOLK_API bool TOLK_CALL Tolk_IsSpeaking(void);
TOLK_API bool TOLK_CALL Tolk_Silence(void);
#ifdef __cplusplus
}
#endif

#endif
