// SPDX-License-Identifier: MPL-2.0

#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void(__stdcall *BoyCtrlSpeakCompleteFunc)(int reason);

typedef enum {
  e_bcerr_success = 0,
  e_bcerr_fail = 1,
  e_bcerr_arg = 2,
  e_bcerr_unavailable = 3,
} BoyCtrlError;

typedef enum {
  e_bcspf_none = 0,
  e_bcspf_withSlave = 1,
  e_bcspf_append = 2,
  e_bcspf_allowBreak = 4,
  e_bcspf_isReader = 8,
} BoyCtrlSpeakFlags;

typedef enum {
  e_bcirm_time,
  e_bcirm_date,
} BoyCtrlInfoReportMode;

__declspec(dllimport) BoyCtrlError __stdcall
BoyCtrlInitialize(const wchar_t *logPath);
__declspec(dllimport) BoyCtrlError __stdcall
BoyCtrlInitializeAnsi(const char *logPath);
__declspec(dllimport) BoyCtrlError __stdcall
BoyCtrlInitializeU8(const char *logPath);
__declspec(dllimport) BoyCtrlError __stdcall
BoyCtrlSpeak(const wchar_t *text, bool withSlave, bool append, bool allowBreak,
             BoyCtrlSpeakCompleteFunc onCompletion);
__declspec(dllimport) BoyCtrlError __stdcall BoyCtrlSpeak2(const wchar_t *text);
__declspec(dllimport) BoyCtrlError __stdcall
BoyCtrlSpeakEx(const wchar_t *text, int flags,
               BoyCtrlSpeakCompleteFunc onCompletion);
__declspec(dllimport) BoyCtrlError __stdcall
BoyCtrlSpeakAnsi(const char *text, bool withSlave, bool append, bool allowBreak,
                 BoyCtrlSpeakCompleteFunc onCompletion);
__declspec(dllimport) BoyCtrlError __stdcall
BoyCtrlSpeakU8(const char *text, bool withSlave, bool append, bool allowBreak,
               BoyCtrlSpeakCompleteFunc onCompletion);
__declspec(dllimport) BoyCtrlError __stdcall
BoyCtrlStopSpeaking(bool withSlave);
__declspec(dllimport) BoyCtrlError __stdcall BoyCtrlStopSpeakingEx(int flags);
__declspec(dllimport) BoyCtrlError __stdcall BoyCtrlStopSpeaking2();
__declspec(dllimport) BoyCtrlError __stdcall BoyCtrlPauseScreenReader(int ms);
__declspec(dllimport) void __stdcall BoyCtrlUninitialize();
__declspec(dllimport) bool __stdcall BoyCtrlIsReaderRunning();
__declspec(dllimport) int __stdcall BoyCtrlGetReaderState();
__declspec(dllimport) bool __stdcall BoyCtrlVerify(const char *key);
__declspec(dllimport) bool __stdcall
BoyCtrlSetAnyKeyStopSpeaking(bool withSlave);
__declspec(dllimport) bool __stdcall BoyCtrlReportInfo(int mode);
__declspec(dllimport) bool __stdcall BoyCtrlStartTextToAudio(
    int taskId, const wchar_t *inputFilePath, const wchar_t *outputFilePath,
    const wchar_t *speechCase, int interval, const wchar_t *format,
    unsigned hwnd, unsigned notifyBaseMsg);
__declspec(dllimport) bool __stdcall BoyCtrlCancelTextToAudio(int taskId);
__declspec(dllimport) bool __stdcall
BoyCtrlActivateYTApp(const wchar_t *appName, unsigned msg, unsigned wParam,
                     unsigned lParam);

#ifdef __cplusplus
}
#endif
