// SPDX-License-Identifier: MPL-2.0

#ifdef _WIN32
#include <windows.h>
#include <array>
#include <cstring>
#include <cwchar>
#include <delayimp.h>
#include <filesystem>
#include <tchar.h>
#include <utility>

template <typename T> static constexpr FARPROC stub_cast(T func) {
  // NOLINTNEXTLINE(bugprone-casting-through-void)
  return reinterpret_cast<FARPROC>(reinterpret_cast<void *>(func));
}

extern "C" {
using StubEntry = struct {
  const char *dll;
  const char *func;
  FARPROC stub;
};

#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64__) ||          \
    defined(__amd64) || defined(_M_X64) || defined(_M_IX86) ||                 \
    defined(__i386__)
#if defined(_M_X64) || defined(__x86_64__)
static constexpr const char *SA_DLL = "SAAPI64.dll";
static constexpr const char *ZDSR_DLL = "ZDSRAPI_x64.dll";
static constexpr const char *BOY_PC_READER_DLL = "BoyCtrl-x64.dll";
#elif defined(_M_IX86) || defined(__i386__)
static constexpr const char *SA_DLL = "SAAPI32.dll";
static constexpr const char *ZDSR_DLL = "ZDSRAPI.dll";
static constexpr const char *BOY_PC_READER_DLL = "BoyCtrl.dll";
#endif
static constexpr const char *PCTK_DLL = "PCTKUSR.dll";
#endif
static constexpr const char *PRISM_ORCA_BRIDGE_DLL = "prism_orca_bridge.dll";
static constexpr const char *PRISM_SPEECH_DISPATCHER_BRIDGE_DLL =
    "prism_speech_dispatcher_bridge.dll";

namespace system_access {
static BOOL __stdcall stub_SA_SayW([[maybe_unused]] const wchar_t *text) {
  return FALSE;
}

static BOOL __stdcall
stub_SA_BrlShowTextW([[maybe_unused]] const wchar_t *msg) {
  return FALSE;
}

static BOOL __stdcall stub_SA_StopAudio() { return FALSE; }

static BOOL __stdcall stub_SA_IsRunning() { return FALSE; }
} // namespace system_access

namespace zdsr {
static int WINAPI stub_zdsr_InitTTS([[maybe_unused]] int type,
                                    [[maybe_unused]] const WCHAR *channelName,
                                    [[maybe_unused]] BOOL bKeyDownInterrupt) {
  return 2;
}

static int WINAPI stub_zdsr_Speak([[maybe_unused]] const WCHAR *text,
                                  [[maybe_unused]] BOOL bInterrupt) {
  return 2;
}

static int WINAPI stub_zdsr_GetSpeakState() { return 2; }

static void WINAPI stub_zdsr_StopSpeak() {}
} // namespace zdsr

namespace boy_pc_reader {
using BoyCtrlSpeakCompleteFunc = void(__stdcall *)(int reason);

// NOLINTBEGIN(performance-enum-size)
enum BoyCtrlError {
  e_bcerr_success = 0,
  e_bcerr_fail = 1,
  e_bcerr_arg = 2,
  e_bcerr_unavailable = 3,
};

enum BoyCtrlSpeakFlags {
  e_bcspf_none = 0,
  e_bcspf_withSlave = 1,
  e_bcspf_append = 2,
  e_bcspf_allowBreak = 4,
  e_bcspf_isReader = 8,
};

enum BoyCtrlInfoReportMode {
  e_bcirm_time,
  e_bcirm_date,
};
// NOLINTEND(performance-enum-size)

static BoyCtrlError __stdcall
stub_BoyCtrlInitialize([[maybe_unused]] const wchar_t *logPath) {
  return e_bcerr_unavailable;
}

static BoyCtrlError __stdcall
stub_BoyCtrlInitializeAnsi([[maybe_unused]] const char *logPath) {
  return e_bcerr_unavailable;
}

static BoyCtrlError __stdcall
stub_BoyCtrlInitializeU8([[maybe_unused]] const char *logPath) {
  return e_bcerr_unavailable;
}

static BoyCtrlError __stdcall
stub_BoyCtrlSpeak([[maybe_unused]] const wchar_t *text,
                  [[maybe_unused]] bool withSlave, [[maybe_unused]] bool append,
                  [[maybe_unused]] bool allowBreak,
                  [[maybe_unused]] BoyCtrlSpeakCompleteFunc onCompletion) {
  return e_bcerr_unavailable;
}

static BoyCtrlError __stdcall
stub_BoyCtrlSpeak2([[maybe_unused]] const wchar_t *text) {
  return e_bcerr_unavailable;
}

static BoyCtrlError __stdcall stub_BoyCtrlSpeak3(
    [[maybe_unused]] const wchar_t *text, [[maybe_unused]] bool withSlave,
    [[maybe_unused]] const wchar_t *slaveName, [[maybe_unused]] bool append,
    [[maybe_unused]] bool allowBreak,
    [[maybe_unused]] BoyCtrlSpeakCompleteFunc onCompletion) {
  return e_bcerr_unavailable;
}

static BoyCtrlError __stdcall
stub_BoyCtrlSpeakEx([[maybe_unused]] const wchar_t *text,
                    [[maybe_unused]] int flags,
                    [[maybe_unused]] BoyCtrlSpeakCompleteFunc onCompletion) {
  return e_bcerr_unavailable;
}

static BoyCtrlError __stdcall stub_BoyCtrlSpeakAnsi(
    [[maybe_unused]] const char *text, [[maybe_unused]] bool withSlave,
    [[maybe_unused]] bool append, [[maybe_unused]] bool allowBreak,
    [[maybe_unused]] BoyCtrlSpeakCompleteFunc onCompletion) {
  return e_bcerr_unavailable;
}

static BoyCtrlError __stdcall stub_BoyCtrlSpeakU8(
    [[maybe_unused]] const char *text, [[maybe_unused]] bool withSlave,
    [[maybe_unused]] bool append, [[maybe_unused]] bool allowBreak,
    [[maybe_unused]] BoyCtrlSpeakCompleteFunc onCompletion) {
  return e_bcerr_unavailable;
}

static BoyCtrlError __stdcall
stub_BoyCtrlStopSpeaking([[maybe_unused]] bool withSlave) {
  return e_bcerr_unavailable;
}

static BoyCtrlError __stdcall
stub_BoyCtrlStopSpeakingEx([[maybe_unused]] int flags) {
  return e_bcerr_unavailable;
}

static BoyCtrlError __stdcall stub_BoyCtrlStopSpeaking2() {
  return e_bcerr_unavailable;
}

static BoyCtrlError __stdcall
stub_BoyCtrlStopSpeaking3([[maybe_unused]] bool withSlave,
                          [[maybe_unused]] const wchar_t *slaveName) {
  return e_bcerr_unavailable;
}

static BoyCtrlError __stdcall
stub_BoyCtrlPauseScreenReader([[maybe_unused]] int ms) {
  return e_bcerr_unavailable;
}

static void __stdcall stub_BoyCtrlUninitialize() {}

static bool __stdcall stub_BoyCtrlIsReaderRunning() { return false; }

static int __stdcall stub_BoyCtrlGetReaderState() { return 0; }

static bool __stdcall stub_BoyCtrlVerify([[maybe_unused]] const char *key) {
  return false;
}

static bool __stdcall
stub_BoyCtrlSetAnyKeyStopSpeaking([[maybe_unused]] bool withSlave) {
  return false;
}

static bool __stdcall stub_BoyCtrlReportInfo([[maybe_unused]] int mode) {
  return false;
}

static bool __stdcall stub_BoyCtrlStartTextToAudio(
    [[maybe_unused]] int taskId, [[maybe_unused]] const wchar_t *inputFilePath,
    [[maybe_unused]] const wchar_t *outputFilePath,
    [[maybe_unused]] const wchar_t *speechCase, [[maybe_unused]] int interval,
    [[maybe_unused]] const wchar_t *format, [[maybe_unused]] unsigned hwnd,
    [[maybe_unused]] unsigned notifyBaseMsg) {
  return false;
}

static bool __stdcall
stub_BoyCtrlCancelTextToAudio([[maybe_unused]] int taskId) {
  return false;
}

static bool __stdcall stub_BoyCtrlActivateYTApp(
    [[maybe_unused]] const wchar_t *appName, [[maybe_unused]] unsigned msg,
    [[maybe_unused]] unsigned wParam, [[maybe_unused]] unsigned lParam) {
  return false;
}
} // namespace boy_pc_reader

namespace pctalker {
static BOOL __stdcall stub_PCTKStatus() { return FALSE; }

static DWORD __stdcall stub_PCTKGetVersion() { return 0; }

static BOOL __stdcall stub_PCTKPRead([[maybe_unused]] const char *text,
                                     [[maybe_unused]] int priority,
                                     [[maybe_unused]] BOOL analyze) {
  return FALSE;
}

static BOOL __stdcall stub_PCTKPReadEx([[maybe_unused]] const char *text,
                                       [[maybe_unused]] int priority,
                                       [[maybe_unused]] BOOL analyze,
                                       [[maybe_unused]] int flags) {
  return FALSE;
}

static BOOL __stdcall stub_PCTKPReadW([[maybe_unused]] const wchar_t *text,
                                      [[maybe_unused]] int priority,
                                      [[maybe_unused]] BOOL analyze) {
  return FALSE;
}

static BOOL __stdcall stub_PCTKPReadExW([[maybe_unused]] const wchar_t *text,
                                        [[maybe_unused]] int priority,
                                        [[maybe_unused]] BOOL analyze,
                                        [[maybe_unused]] int flags) {
  return FALSE;
}

static void __stdcall stub_PCTKVReset() {}

static BOOL __stdcall stub_PCTKGetVStatus() { return FALSE; }

static void __stdcall stub_PCTKVoiceGuide([[maybe_unused]] const char *text) {}

static void __stdcall stub_PCTKBeep([[maybe_unused]] int type,
                                    [[maybe_unused]] int param) {}

static BOOL __stdcall stub_PCTKCGuide([[maybe_unused]] const char *text,
                                      [[maybe_unused]] DWORD mode) {
  return FALSE;
}

static BOOL __stdcall stub_PCTKCGuideEx([[maybe_unused]] const char *text,
                                        [[maybe_unused]] DWORD mode,
                                        [[maybe_unused]] int flags) {
  return FALSE;
}

static BOOL __stdcall stub_PCTKCGuideW([[maybe_unused]] const wchar_t *text,
                                       [[maybe_unused]] DWORD mode) {
  return FALSE;
}

static BOOL __stdcall stub_PCTKCGuideExW([[maybe_unused]] const wchar_t *text,
                                         [[maybe_unused]] DWORD mode,
                                         [[maybe_unused]] int flags) {
  return FALSE;
}

static DWORD __stdcall stub_PCTKGetStatus([[maybe_unused]] UINT item,
                                          [[maybe_unused]] LPVOID param1,
                                          [[maybe_unused]] LPVOID param2) {
  return 0;
}

static DWORD __stdcall stub_PCTKSetStatus([[maybe_unused]] UINT item,
                                          [[maybe_unused]] LPVOID param1,
                                          [[maybe_unused]] LPVOID param2) {
  return 0;
}

static int __stdcall stub_PCTKCommand([[maybe_unused]] const char *cmdstr,
                                      [[maybe_unused]] LPARAM param1,
                                      [[maybe_unused]] LPARAM param2) {
  return 0;
}

static BOOL __stdcall stub_PCTKLoadUserDict() { return FALSE; }

static BOOL __stdcall stub_PCTKPinStatus() { return FALSE; }

static BOOL __stdcall stub_PCTKPinFocus([[maybe_unused]] LONG_PTR context,
                                        [[maybe_unused]] const char *text,
                                        [[maybe_unused]] DWORD dispFlags,
                                        [[maybe_unused]] const char *auxText,
                                        [[maybe_unused]] LONG_PTR auxParam) {
  return FALSE;
}

static BOOL __stdcall stub_PCTKPinFocusW(
    [[maybe_unused]] LONG_PTR context, [[maybe_unused]] const wchar_t *text,
    [[maybe_unused]] DWORD dispFlags, [[maybe_unused]] const wchar_t *auxText,
    [[maybe_unused]] LONG_PTR auxParam) {
  return FALSE;
}

static BOOL __stdcall stub_PCTKPinIsFocus([[maybe_unused]] LONG_PTR context) {
  return FALSE;
}

static BOOL __stdcall stub_PCTKPinWrite([[maybe_unused]] const char *text,
                                        [[maybe_unused]] int mode,
                                        [[maybe_unused]] int flags) {
  return FALSE;
}

static BOOL __stdcall stub_PCTKPinWriteW([[maybe_unused]] const wchar_t *text,
                                         [[maybe_unused]] int mode,
                                         [[maybe_unused]] int flags) {
  return FALSE;
}

static BOOL __stdcall stub_PCTKPinEDWrite([[maybe_unused]] const char *text,
                                          [[maybe_unused]] int editMode,
                                          [[maybe_unused]] int cursorPos,
                                          [[maybe_unused]] int selStart,
                                          [[maybe_unused]] int selLen,
                                          [[maybe_unused]] int lineOffset,
                                          [[maybe_unused]] int charAttr) {
  return FALSE;
}

static BOOL __stdcall stub_PCTKPinEDWriteW([[maybe_unused]] const wchar_t *text,
                                           [[maybe_unused]] int editMode,
                                           [[maybe_unused]] int cursorPos,
                                           [[maybe_unused]] int selStart,
                                           [[maybe_unused]] int selLen,
                                           [[maybe_unused]] int lineOffset,
                                           [[maybe_unused]] int charAttr) {
  return FALSE;
}

static BOOL __stdcall
stub_PCTKPinStatusCell([[maybe_unused]] const void *cellData,
                       [[maybe_unused]] void *outBuf) {
  return FALSE;
}

static BOOL __stdcall
stub_PCTKPinStatusCellW([[maybe_unused]] const void *cellData,
                        [[maybe_unused]] void *outBuf) {
  return FALSE;
}

static void __stdcall stub_PCTKPinReset() {}

static BOOL __stdcall stub_SoundMessage([[maybe_unused]] const char *text,
                                        [[maybe_unused]] int flags) {
  return FALSE;
}

static BOOL __stdcall stub_SoundStatus() { return FALSE; }

static BOOL __stdcall stub_SoundModifyMode([[maybe_unused]] int on,
                                           [[maybe_unused]] int off) {
  return FALSE;
}

static BOOL __stdcall stub_SoundPause([[maybe_unused]] BOOL sw) {
  return FALSE;
}

static int __stdcall stub_AGSEvent([[maybe_unused]] int eventType,
                                   [[maybe_unused]] LPARAM param1,
                                   [[maybe_unused]] LPARAM param2) {
  return 0;
}

static LONGLONG __stdcall stub_GetUIActionMode() { return 0; }

static BOOL __stdcall stub_IsImmInput([[maybe_unused]] HWND hwnd) {
  return FALSE;
}

static int __stdcall stub_PCTKEventHook() { return 0; }

static int __stdcall stub_PCTKGetVoiceLog() { return 0; }

static int __stdcall stub_SetKeyBreak() { return 1; }

static void __stdcall stub_EncodeFlags() {}

static int __stdcall stub_dic_regist() { return 0; }

static int __stdcall stub_dic_regist_detail() { return 0; }

static int __stdcall stub_dic_reg_from_file() { return 0; }

static int __stdcall stub_dic_text_out() { return 0; }

static int __stdcall stub_dic_reg_detail_from_file() { return 0; }

static int __stdcall stub_dic_detail_text_out() { return 0; }
} // namespace pctalker

namespace prism_orca_bridge {
using PrismOrcaDBusInstance = void *;

bool __cdecl prism_orca_available_stub(void) { return false; }

bool __cdecl
prism_orca_create_stub([[maybe_unused]] PrismOrcaDBusInstance **out) {
  return false;
}

void __cdecl
prism_orca_destroy_stub([[maybe_unused]] PrismOrcaDBusInstance *h) {}

bool __cdecl prism_orca_speak_stub([[maybe_unused]] PrismOrcaDBusInstance *h,
                                   [[maybe_unused]] const char *text) {
  return false;
}

bool __cdecl prism_orca_stop_stub([[maybe_unused]] PrismOrcaDBusInstance *h) {
  return false;
}
} // namespace prism_orca_bridge

namespace prism_speech_dispatcher_bridge {
using PrismSpeechDispatcherInstance = void *;

bool __cdecl prism_speechd_available_stub(void) { return false; }

bool __cdecl prism_speechd_create_stub(
    [[maybe_unused]] PrismSpeechDispatcherInstance **out) {
  return false;
}

void __cdecl
prism_speechd_destroy_stub([[maybe_unused]] PrismSpeechDispatcherInstance *h) {}

bool __cdecl
prism_speechd_speak_stub([[maybe_unused]] PrismSpeechDispatcherInstance *h,
                         [[maybe_unused]] const char *text) {
  return false;
}

bool __cdecl
prism_speechd_stop_stub([[maybe_unused]] PrismSpeechDispatcherInstance *h) {
  return false;
}
} // namespace prism_speech_dispatcher_bridge

static const
#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64__) ||          \
    defined(__amd64) || defined(_M_X64) || defined(_M_IX86) ||                 \
    defined(__i386__)
    auto stubs = std::to_array<StubEntry>({
        {.dll = SA_DLL,
         .func = "SA_SayW",
         .stub = stub_cast(system_access::stub_SA_SayW)},
        {.dll = SA_DLL,
         .func = "SA_BrlShowTextW",
         .stub = stub_cast(system_access::stub_SA_BrlShowTextW)},
        {.dll = SA_DLL,
         .func = "SA_StopAudio",
         .stub = stub_cast(system_access::stub_SA_StopAudio)},
        {.dll = SA_DLL,
         .func = "SA_IsRunning",
         .stub = stub_cast(system_access::stub_SA_IsRunning)},
        {.dll = ZDSR_DLL,
         .func = "InitTTS",
         .stub = stub_cast(zdsr::stub_zdsr_InitTTS)},
        {.dll = ZDSR_DLL,
         .func = "Speak",
         .stub = stub_cast(zdsr::stub_zdsr_Speak)},
        {.dll = ZDSR_DLL,
         .func = "GetSpeakState",
         .stub = stub_cast(zdsr::stub_zdsr_GetSpeakState)},
        {.dll = ZDSR_DLL,
         .func = "StopSpeak",
         .stub = stub_cast(zdsr::stub_zdsr_StopSpeak)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlInitialize",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlInitialize)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlInitializeAnsi",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlInitializeAnsi)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlInitializeU8",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlInitializeU8)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlSpeak",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlSpeak)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlSpeak2",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlSpeak2)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlSpeak3",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlSpeak3)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlSpeakEx",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlSpeakEx)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlSpeakAnsi",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlSpeakAnsi)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlSpeakU8",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlSpeakU8)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlStopSpeaking",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlStopSpeaking)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlStopSpeakingEx",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlStopSpeakingEx)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlStopSpeaking2",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlStopSpeaking2)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlStopSpeaking3",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlStopSpeaking3)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlPauseScreenReader",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlPauseScreenReader)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlUninitialize",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlUninitialize)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlIsReaderRunning",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlIsReaderRunning)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlGetReaderState",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlGetReaderState)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlVerify",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlVerify)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlSetAnyKeyStopSpeaking",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlSetAnyKeyStopSpeaking)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlReportInfo",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlReportInfo)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlStartTextToAudio",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlStartTextToAudio)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlCancelTextToAudio",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlCancelTextToAudio)},
        {.dll = BOY_PC_READER_DLL,
         .func = "BoyCtrlActivateYTApp",
         .stub = stub_cast(boy_pc_reader::stub_BoyCtrlActivateYTApp)},
        {.dll = PCTK_DLL,
         .func = "PCTKStatus",
         .stub = stub_cast(pctalker::stub_PCTKStatus)},
        {.dll = PCTK_DLL,
         .func = "PCTKGetVersion",
         .stub = stub_cast(pctalker::stub_PCTKGetVersion)},
        {.dll = PCTK_DLL,
         .func = "PCTKPRead",
         .stub = stub_cast(pctalker::stub_PCTKPRead)},
        {.dll = PCTK_DLL,
         .func = "PCTKPReadEx",
         .stub = stub_cast(pctalker::stub_PCTKPReadEx)},
        {.dll = PCTK_DLL,
         .func = "PCTKPReadW",
         .stub = stub_cast(pctalker::stub_PCTKPReadW)},
        {.dll = PCTK_DLL,
         .func = "PCTKPReadExW",
         .stub = stub_cast(pctalker::stub_PCTKPReadExW)},
        {.dll = PCTK_DLL,
         .func = "PCTKVReset",
         .stub = stub_cast(pctalker::stub_PCTKVReset)},
        {.dll = PCTK_DLL,
         .func = "PCTKGetVStatus",
         .stub = stub_cast(pctalker::stub_PCTKGetVStatus)},
        {.dll = PCTK_DLL,
         .func = "PCTKVoiceGuide",
         .stub = stub_cast(pctalker::stub_PCTKVoiceGuide)},
        {.dll = PCTK_DLL,
         .func = "PCTKBeep",
         .stub = stub_cast(pctalker::stub_PCTKBeep)},
        {.dll = PCTK_DLL,
         .func = "PCTKCGuide",
         .stub = stub_cast(pctalker::stub_PCTKCGuide)},
        {.dll = PCTK_DLL,
         .func = "PCTKCGuideEx",
         .stub = stub_cast(pctalker::stub_PCTKCGuideEx)},
        {.dll = PCTK_DLL,
         .func = "PCTKCGuideW",
         .stub = stub_cast(pctalker::stub_PCTKCGuideW)},
        {.dll = PCTK_DLL,
         .func = "PCTKCGuideExW",
         .stub = stub_cast(pctalker::stub_PCTKCGuideExW)},
        {.dll = PCTK_DLL,
         .func = "PCTKGetStatus",
         .stub = stub_cast(pctalker::stub_PCTKGetStatus)},
        {.dll = PCTK_DLL,
         .func = "PCTKSetStatus",
         .stub = stub_cast(pctalker::stub_PCTKSetStatus)},
        {.dll = PCTK_DLL,
         .func = "PCTKCommand",
         .stub = stub_cast(pctalker::stub_PCTKCommand)},
        {.dll = PCTK_DLL,
         .func = "PCTKLoadUserDict",
         .stub = stub_cast(pctalker::stub_PCTKLoadUserDict)},
        {.dll = PCTK_DLL,
         .func = "PCTKPinStatus",
         .stub = stub_cast(pctalker::stub_PCTKPinStatus)},
        {.dll = PCTK_DLL,
         .func = "PCTKPinFocus",
         .stub = stub_cast(pctalker::stub_PCTKPinFocus)},
        {.dll = PCTK_DLL,
         .func = "PCTKPinFocusW",
         .stub = stub_cast(pctalker::stub_PCTKPinFocusW)},
        {.dll = PCTK_DLL,
         .func = "PCTKPinIsFocus",
         .stub = stub_cast(pctalker::stub_PCTKPinIsFocus)},
        {.dll = PCTK_DLL,
         .func = "PCTKPinWrite",
         .stub = stub_cast(pctalker::stub_PCTKPinWrite)},
        {.dll = PCTK_DLL,
         .func = "PCTKPinWriteW",
         .stub = stub_cast(pctalker::stub_PCTKPinWriteW)},
        {.dll = PCTK_DLL,
         .func = "PCTKPinEDWrite",
         .stub = stub_cast(pctalker::stub_PCTKPinEDWrite)},
        {.dll = PCTK_DLL,
         .func = "PCTKPinEDWriteW",
         .stub = stub_cast(pctalker::stub_PCTKPinEDWriteW)},
        {.dll = PCTK_DLL,
         .func = "PCTKPinStatusCell",
         .stub = stub_cast(pctalker::stub_PCTKPinStatusCell)},
        {.dll = PCTK_DLL,
         .func = "PCTKPinStatusCellW",
         .stub = stub_cast(pctalker::stub_PCTKPinStatusCellW)},
        {.dll = PCTK_DLL,
         .func = "PCTKPinReset",
         .stub = stub_cast(pctalker::stub_PCTKPinReset)},
        {.dll = PCTK_DLL,
         .func = "SoundMessage",
         .stub = stub_cast(pctalker::stub_SoundMessage)},
        {.dll = PCTK_DLL,
         .func = "SoundStatus",
         .stub = stub_cast(pctalker::stub_SoundStatus)},
        {.dll = PCTK_DLL,
         .func = "SoundModifyMode",
         .stub = stub_cast(pctalker::stub_SoundModifyMode)},
        {.dll = PCTK_DLL,
         .func = "SoundPause",
         .stub = stub_cast(pctalker::stub_SoundPause)},
        {.dll = PCTK_DLL,
         .func = "AGSEvent",
         .stub = stub_cast(pctalker::stub_AGSEvent)},
        {.dll = PCTK_DLL,
         .func = "GetUIActionMode",
         .stub = stub_cast(pctalker::stub_GetUIActionMode)},
        {.dll = PCTK_DLL,
         .func = "IsImmInput",
         .stub = stub_cast(pctalker::stub_IsImmInput)},
        {.dll = PCTK_DLL,
         .func = "PCTKEventHook",
         .stub = stub_cast(pctalker::stub_PCTKEventHook)},
        {.dll = PCTK_DLL,
         .func = "PCTKGetVoiceLog",
         .stub = stub_cast(pctalker::stub_PCTKGetVoiceLog)},
        {.dll = PCTK_DLL,
         .func = "SetKeyBreak",
         .stub = stub_cast(pctalker::stub_SetKeyBreak)},
        {.dll = PCTK_DLL,
         .func = "EncodeFlags",
         .stub = stub_cast(pctalker::stub_EncodeFlags)},
        {.dll = PCTK_DLL,
         .func = "dic_regist",
         .stub = stub_cast(pctalker::stub_dic_regist)},
        {.dll = PCTK_DLL,
         .func = "dic_regist_detail",
         .stub = stub_cast(pctalker::stub_dic_regist_detail)},
        {.dll = PCTK_DLL,
         .func = "dic_reg_from_file",
         .stub = stub_cast(pctalker::stub_dic_reg_from_file)},
        {.dll = PCTK_DLL,
         .func = "dic_text_out",
         .stub = stub_cast(pctalker::stub_dic_text_out)},
        {.dll = PCTK_DLL,
         .func = "dic_reg_detail_from_file",
         .stub = stub_cast(pctalker::stub_dic_reg_detail_from_file)},
        {.dll = PCTK_DLL,
         .func = "dic_detail_text_out",
         .stub = stub_cast(pctalker::stub_dic_detail_text_out)},
        {.dll = PCTK_DLL,
         .func = "PCTKSTATUS",
         .stub = stub_cast(pctalker::stub_PCTKStatus)},
        {.dll = PCTK_DLL,
         .func = "PCTKGETVERSION",
         .stub = stub_cast(pctalker::stub_PCTKGetVersion)},
        {.dll = PCTK_DLL,
         .func = "PCTKGETVSTATUS",
         .stub = stub_cast(pctalker::stub_PCTKGetVStatus)},
        {.dll = PCTK_DLL,
         .func = "PCTKGETSTATUS",
         .stub = stub_cast(pctalker::stub_PCTKGetStatus)},
        {.dll = PCTK_DLL,
         .func = "PCTKSETSTATUS",
         .stub = stub_cast(pctalker::stub_PCTKSetStatus)},
        {.dll = PCTK_DLL,
         .func = "PCTKPREAD",
         .stub = stub_cast(pctalker::stub_PCTKPRead)},
        {.dll = PCTK_DLL,
         .func = "PCTKPREADEX",
         .stub = stub_cast(pctalker::stub_PCTKPReadEx)},
        {.dll = PCTK_DLL,
         .func = "PCTKPREADW",
         .stub = stub_cast(pctalker::stub_PCTKPReadW)},
        {.dll = PCTK_DLL,
         .func = "PCTKPREADEXW",
         .stub = stub_cast(pctalker::stub_PCTKPReadExW)},
        {.dll = PCTK_DLL,
         .func = "PCTKCGUIDE",
         .stub = stub_cast(pctalker::stub_PCTKCGuide)},
        {.dll = PCTK_DLL,
         .func = "PCTKCGUIDEEX",
         .stub = stub_cast(pctalker::stub_PCTKCGuideEx)},
        {.dll = PCTK_DLL,
         .func = "PCTKCGUIDEW",
         .stub = stub_cast(pctalker::stub_PCTKCGuideW)},
        {.dll = PCTK_DLL,
         .func = "PCTKCGUIDEEXW",
         .stub = stub_cast(pctalker::stub_PCTKCGuideExW)},
        {.dll = PCTK_DLL,
         .func = "PCTKCOMMAND",
         .stub = stub_cast(pctalker::stub_PCTKCommand)},
        {.dll = PCTK_DLL,
         .func = "PCTKVOICEGUIDE",
         .stub = stub_cast(pctalker::stub_PCTKVoiceGuide)},
        {.dll = PCTK_DLL,
         .func = "PCTKVRESET",
         .stub = stub_cast(pctalker::stub_PCTKVReset)},
        {.dll = PCTK_DLL,
         .func = "PCTKBEEP",
         .stub = stub_cast(pctalker::stub_PCTKBeep)},
        {.dll = PCTK_DLL,
         .func = "PCTKLOADUSERDICT",
         .stub = stub_cast(pctalker::stub_PCTKLoadUserDict)},
        {.dll = PCTK_DLL,
         .func = "PCTKEVENTHOOK",
         .stub = stub_cast(pctalker::stub_PCTKEventHook)},
        {.dll = PCTK_DLL,
         .func = "PCTKGETVOICELOG",
         .stub = stub_cast(pctalker::stub_PCTKGetVoiceLog)},
        {.dll = PRISM_ORCA_BRIDGE_DLL,
         .func = "prism_orca_available",
         .stub = stub_cast(prism_orca_bridge::prism_orca_available_stub)},
        {.dll = PRISM_ORCA_BRIDGE_DLL,
         .func = "prism_orca_create",
         .stub = stub_cast(prism_orca_bridge::prism_orca_create_stub)},
        {.dll = PRISM_ORCA_BRIDGE_DLL,
         .func = "prism_orca_destroy",
         .stub = stub_cast(prism_orca_bridge::prism_orca_destroy_stub)},
        {.dll = PRISM_ORCA_BRIDGE_DLL,
         .func = "prism_orca_speak",
         .stub = stub_cast(prism_orca_bridge::prism_orca_speak_stub)},
        {.dll = PRISM_ORCA_BRIDGE_DLL,
         .func = "prism_orca_stop",
         .stub = stub_cast(prism_orca_bridge::prism_orca_stop_stub)},
        {.dll = PRISM_SPEECH_DISPATCHER_BRIDGE_DLL,
         .func = "prism_speechd_available",
         .stub = stub_cast(
             prism_speech_dispatcher_bridge::prism_speechd_available_stub)},
        {.dll = PRISM_SPEECH_DISPATCHER_BRIDGE_DLL,
         .func = "prism_speechd_create",
         .stub = stub_cast(
             prism_speech_dispatcher_bridge::prism_speechd_create_stub)},
        {.dll = PRISM_SPEECH_DISPATCHER_BRIDGE_DLL,
         .func = "prism_speechd_destroy",
         .stub = stub_cast(
             prism_speech_dispatcher_bridge::prism_speechd_destroy_stub)},
        {.dll = PRISM_SPEECH_DISPATCHER_BRIDGE_DLL,
         .func = "prism_speechd_speak",
         .stub = stub_cast(
             prism_speech_dispatcher_bridge::prism_speechd_speak_stub)},
        {.dll = PRISM_SPEECH_DISPATCHER_BRIDGE_DLL,
         .func = "prism_speechd_stop",
         .stub = stub_cast(
             prism_speech_dispatcher_bridge::prism_speechd_stop_stub)},
    });
#else
    auto stubs = std::to_array<StubEntry>({
        {.dll = PRISM_ORCA_BRIDGE_DLL,
         .func = "prism_orca_available",
         .stub = stub_cast(prism_orca_bridge::prism_orca_available_stub)},
        {.dll = PRISM_ORCA_BRIDGE_DLL,
         .func = "prism_orca_create",
         .stub = stub_cast(prism_orca_bridge::prism_orca_create_stub)},
        {.dll = PRISM_ORCA_BRIDGE_DLL,
         .func = "prism_orca_destroy",
         .stub = stub_cast(prism_orca_bridge::prism_orca_destroy_stub)},
        {.dll = PRISM_ORCA_BRIDGE_DLL,
         .func = "prism_orca_speak",
         .stub = stub_cast(prism_orca_bridge::prism_orca_speak_stub)},
        {.dll = PRISM_ORCA_BRIDGE_DLL,
         .func = "prism_orca_stop",
         .stub = stub_cast(prism_orca_bridge::prism_orca_stop_stub)},
        {.dll = PRISM_SPEECH_DISPATCHER_BRIDGE_DLL,
         .func = "prism_speechd_available",
         .stub = stub_cast(
             prism_speech_dispatcher_bridge::prism_speechd_available_stub)},
        {.dll = PRISM_SPEECH_DISPATCHER_BRIDGE_DLL,
         .func = "prism_speechd_create",
         .stub = stub_cast(
             prism_speech_dispatcher_bridge::prism_speechd_create_stub)},
        {.dll = PRISM_SPEECH_DISPATCHER_BRIDGE_DLL,
         .func = "prism_speechd_destroy",
         .stub = stub_cast(
             prism_speech_dispatcher_bridge::prism_speechd_destroy_stub)},
        {.dll = PRISM_SPEECH_DISPATCHER_BRIDGE_DLL,
         .func = "prism_speechd_speak",
         .stub = stub_cast(
             prism_speech_dispatcher_bridge::prism_speechd_speak_stub)},
        {.dll = PRISM_SPEECH_DISPATCHER_BRIDGE_DLL,
         .func = "prism_speechd_stop",
         .stub = stub_cast(
             prism_speech_dispatcher_bridge::prism_speechd_stop_stub)},
    });
#endif

static int dummy_count = 0;

static FARPROC WINAPI DelayLoadFailureHook(unsigned dliNotify,
                                           PDelayLoadInfo pdli) {
  switch (dliNotify) {
  case dliFailLoadLib: {
    namespace fs = std::filesystem;
    static const int anchor = 0;
    HMODULE hModule = nullptr;
    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(&anchor), &hModule)) {
      std::wstring path_buffer;
      path_buffer.resize(MAX_PATH);
      const DWORD len = GetModuleFileName(
          hModule, path_buffer.data(), static_cast<DWORD>(path_buffer.size()));
      if (len > 0) {
        path_buffer.resize(len);
        const auto dll_path =
            fs::path(path_buffer).replace_filename(pdli->szDll);
        if (auto *const h = LoadLibrary(dll_path.c_str()); h != nullptr) {
          return reinterpret_cast<FARPROC>(h);
        }
      }
    }
#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64__) ||          \
    defined(__amd64) || defined(_M_X64) || defined(_M_IX86) ||                 \
    defined(__i386__)
    if (_stricmp(pdli->szDll, ZDSR_DLL) == 0) {
      HKEY zdsr_key;
#if defined(_M_X64) || defined(__x86_64__)
      if (const auto res = RegOpenKeyEx(
              HKEY_LOCAL_MACHINE, _T("SOFTWARE\\WOW6432Node\\zhiduo\\zdsr"), 0,
              KEY_QUERY_VALUE | KEY_READ, &zdsr_key);
          res == ERROR_SUCCESS) {
#elif defined(_M_IX86) || defined(__i386__)
      if (const auto res =
              RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\zhiduo\\zdsr"), 0,
                           KEY_QUERY_VALUE | KEY_READ, &zdsr_key);
          res == ERROR_SUCCESS) {
#endif
        std::wstring path;
        path.resize(MAX_PATH);
        DWORD size = MAX_PATH * sizeof(wchar_t);
        if (const auto res2 =
                RegQueryValueEx(zdsr_key, _T("path"), nullptr, nullptr,
                                reinterpret_cast<LPBYTE>(path.data()), &size);
            res2 == ERROR_SUCCESS) {
          path.resize(std::wcslen(path.c_str()));
          if (!path.empty() && path.back() != _T('\\')) {
            path += _T('\\');
          }
          path += fs::path(ZDSR_DLL).wstring();
          auto *const h = LoadLibrary(path.c_str());
          RegCloseKey(zdsr_key);
          if (h != nullptr) {
            return reinterpret_cast<FARPROC>(h);
          }
        } else {
          RegCloseKey(zdsr_key);
        }
      }
    }
#endif
#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64__) ||          \
    defined(__amd64) || defined(_M_X64) || defined(_M_IX86) ||                 \
    defined(__i386__)
    if (_stricmp(pdli->szDll, BOY_PC_READER_DLL) == 0) {
      HKEY boy_pc_reader_key;
#if defined(_M_X64) || defined(__x86_64__)
      if (const auto res = RegOpenKeyEx(
              HKEY_LOCAL_MACHINE,
              _T("SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\U")
              _T("ninstall\\{1F0FDAE0-3E94-4B86-8F08-C68E70D5D87D}_is1"),
              0, KEY_QUERY_VALUE | KEY_READ, &boy_pc_reader_key);
          res == ERROR_SUCCESS) {
#elif defined(_M_IX86) || defined(__i386__)
      if (const auto res = RegOpenKeyEx(
              HKEY_LOCAL_MACHINE,
              _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{")
              _T("1F0FDAE0-3E94-4B86-8F08-C68E70D5D87D}_is1"),
              0, KEY_QUERY_VALUE | KEY_READ, &boy_pc_reader_key);
          res == ERROR_SUCCESS) {
#endif
        std::wstring path;
        path.resize(MAX_PATH);
        DWORD size = MAX_PATH * sizeof(wchar_t);
        if (const auto res2 = RegQueryValueEx(
                boy_pc_reader_key, _T("InstallLocation"), nullptr, nullptr,
                reinterpret_cast<LPBYTE>(path.data()), &size);
            res2 == ERROR_SUCCESS) {
          path.resize(std::wcslen(path.c_str()));
          if (!path.empty() && path.back() != _T('\\')) {
            path += _T('\\');
          }
          path += fs::path(BOY_PC_READER_DLL).wstring();
          auto *const h = LoadLibrary(path.c_str());
          RegCloseKey(boy_pc_reader_key);
          if (h != nullptr) {
            return reinterpret_cast<FARPROC>(h);
          }
        } else {
          RegCloseKey(boy_pc_reader_key);
        }
      }
    }
#endif
    if (dummy_count < 512) {
      // NOLINTNEXTLINE(performance-no-int-to-ptr)
      auto *dummy = reinterpret_cast<HMODULE>(
          static_cast<uintptr_t>(0xDEAD0000 + dummy_count));
      dummy_count++;
      return reinterpret_cast<FARPROC>(dummy);
    }
    return reinterpret_cast<FARPROC>(reinterpret_cast<HMODULE>(1));
  } break;
  case dliFailGetProc: {
    for (const auto &e : stubs) {
      if (_stricmp(pdli->szDll, e.dll) == 0 &&
          strcmp(pdli->dlp.szProcName, e.func) == 0) {
        return e.stub;
      }
    }
    return nullptr;
  } break;
  default:
    break;
  }
  return nullptr;
}

const PfnDliHook __pfnDliFailureHook2 = DelayLoadFailureHook;
}
#endif
