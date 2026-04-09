#pragma once
#ifndef _TOLK_H_
#define _TOLK_H_

#ifdef _EXPORTING
#define TOLK_DLL_DECLSPEC __declspec(dllexport)
#else
#define TOLK_DLL_DECLSPEC __declspec(dllimport)
#endif // _EXPORTING
#define TOLK_CALL __cdecl

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#include <wchar.h>
#endif // __cplusplus

TOLK_DLL_DECLSPEC void TOLK_CALL Tolk_Load();
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_IsLoaded();
TOLK_DLL_DECLSPEC void TOLK_CALL Tolk_Unload();
TOLK_DLL_DECLSPEC void TOLK_CALL Tolk_TrySAPI(bool trySAPI);
TOLK_DLL_DECLSPEC void TOLK_CALL Tolk_PreferSAPI(bool preferSAPI);
TOLK_DLL_DECLSPEC const wchar_t * TOLK_CALL Tolk_DetectScreenReader();
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_HasSpeech();
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_HasBraille();
#ifdef __cplusplus
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_Output(const wchar_t *str, bool interrupt = false);
#else
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_Output(const wchar_t *str, bool interrupt);
#endif // __cplusplus
#ifdef __cplusplus
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_Speak(const wchar_t *str, bool interrupt = false);
#else
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_Speak(const wchar_t *str, bool interrupt);
#endif // __cplusplus
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_Braille(const wchar_t *str);
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_IsSpeaking();
TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_Silence();
#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // _TOLK_H_
