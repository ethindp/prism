// SPDX-License-Identifier: MPL-2.0

#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

__declspec(dllimport) BOOL __stdcall SA_SayW(const wchar_t *);
__declspec(dllimport) BOOL __stdcall SA_BrlShowTextW(const wchar_t *);
__declspec(dllimport) BOOL __stdcall SA_StopAudio(void);
__declspec(dllimport) BOOL __stdcall SA_IsRunning(void);

#ifdef __cplusplus
}
#endif
