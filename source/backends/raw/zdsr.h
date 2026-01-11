// SPDX-License-Identifier: MPL-2.0

#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

__declspec(dllimport) int WINAPI InitTTS(int type, const WCHAR* channelName, BOOL bKeyDownInterrupt);
__declspec(dllimport) int WINAPI Speak(const WCHAR* text, BOOL bInterrupt);
__declspec(dllimport) int WINAPI GetSpeakState();
__declspec(dllimport) void WINAPI StopSpeak();

#ifdef __cplusplus
}
#endif
