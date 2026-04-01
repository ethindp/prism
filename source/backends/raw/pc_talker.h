// SPDX-License-Identifier: MPL-2.0

#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif


#define PS_STATUS       0x10001
#define PS_PREV         0x10002
#define PS_NEXT         0x10003
#define PS_SETEX        0x20001
#define PS_UIACTION     0x20002
#define PS_UIACTION2    0x20003
#define PKDI_KEYGUIDE   0x18
#define PKDI_IME        0x13
#define PKDI_CURSOR     0x1C
#define PKDI_MOUSE      0x1D
#define PCTK_PRIORITY_DEFAULT   0
#define PCTK_PRIORITY_LOW       1
#define PCTK_PRIORITY_HIGH      2
#define PCTK_PRIORITY_OVERRIDE  3
#define PCTK_SPEAK_FORCE        0x0001
#define PCTK_SPEAK_RAW          0x0002
#define PCTK_SPEAK_EXTANALYSIS  0x0004
#define PCTK_SPEAK_SYNC         0x0040
#define PCTK_PIN_MODE_DEFAULT   0x00000000u
#define PCTK_PIN_MODE_1         0x40000000u
#define PCTK_PIN_MODE_2         0x80000000u
#define PCTK_PIN_MODE_3         0xC0000000u
#define PCTK_PIN_MODE_MASK      0xC0000000u


__declspec(dllimport) BOOL  __stdcall PCTKStatus(void);
__declspec(dllimport) DWORD __stdcall PCTKGetVersion(void);
__declspec(dllimport) BOOL __stdcall PCTKPRead(
    const char *text, int priority, BOOL analyze);
__declspec(dllimport) BOOL __stdcall PCTKPReadEx(
    const char *text, int priority, BOOL analyze, int flags);
__declspec(dllimport) BOOL __stdcall PCTKPReadW(
    const wchar_t *text, int priority, BOOL analyze);
__declspec(dllimport) BOOL __stdcall PCTKPReadExW(
    const wchar_t *text, int priority, BOOL analyze, int flags);
__declspec(dllimport) void __stdcall PCTKVReset(void);
__declspec(dllimport) BOOL __stdcall PCTKGetVStatus(void);
__declspec(dllimport) void __stdcall PCTKVoiceGuide(const char *text);
__declspec(dllimport) void __stdcall PCTKBeep(int type, int param);
__declspec(dllimport) BOOL __stdcall PCTKCGuide(
    const char *text, DWORD mode);
__declspec(dllimport) BOOL __stdcall PCTKCGuideEx(
    const char *text, DWORD mode, int flags);
__declspec(dllimport) BOOL __stdcall PCTKCGuideW(
    const wchar_t *text, DWORD mode);
__declspec(dllimport) BOOL __stdcall PCTKCGuideExW(
    const wchar_t *text, DWORD mode, int flags);
__declspec(dllimport) DWORD __stdcall PCTKGetStatus(
    UINT item, LPVOID param1, LPVOID param2);
__declspec(dllimport) DWORD __stdcall PCTKSetStatus(
    UINT item, LPVOID param1, LPVOID param2);
__declspec(dllimport) int __stdcall PCTKCommand(
    const char *cmdstr, LPARAM param1, LPARAM param2);
__declspec(dllimport) BOOL __stdcall PCTKLoadUserDict(void);
__declspec(dllimport) BOOL __stdcall PCTKPinStatus(void);
__declspec(dllimport) BOOL __stdcall PCTKPinFocus(
    LONG_PTR context, const char *text, DWORD disp_flags,
    const char *aux_text, LONG_PTR aux_param);
__declspec(dllimport) BOOL __stdcall PCTKPinFocusW(
    LONG_PTR context, const wchar_t *text, DWORD disp_flags,
    const wchar_t *aux_text, LONG_PTR aux_param);
__declspec(dllimport) BOOL __stdcall PCTKPinIsFocus(LONG_PTR context);
__declspec(dllimport) BOOL __stdcall PCTKPinWrite(
    const char *text, int mode, int flags);
__declspec(dllimport) BOOL __stdcall PCTKPinWriteW(
    const wchar_t *text, int mode, int flags);
__declspec(dllimport) BOOL __stdcall PCTKPinEDWrite(
    const char *text, int edit_mode, int cursor_pos,
    int sel_start, int sel_len, int line_offset, int char_attr);
__declspec(dllimport) BOOL __stdcall PCTKPinEDWriteW(
    const wchar_t *text, int edit_mode, int cursor_pos,
    int sel_start, int sel_len, int line_offset, int char_attr);
__declspec(dllimport) BOOL __stdcall PCTKPinStatusCell(
    const void *cell_data, void *out_buf);
__declspec(dllimport) BOOL __stdcall PCTKPinStatusCellW(
    const void *cell_data, void *out_buf);
__declspec(dllimport) void __stdcall PCTKPinReset(void);
__declspec(dllimport) BOOL __stdcall SoundMessage(
    const char *text, int flags);
__declspec(dllimport) BOOL __stdcall SoundStatus(void);
__declspec(dllimport) BOOL __stdcall SoundModifyMode(int on, int off);
__declspec(dllimport) BOOL __stdcall SoundPause(BOOL sw);
__declspec(dllimport) int      __stdcall AGSEvent(
    int event_type, LPARAM param1, LPARAM param2);
__declspec(dllimport) LONGLONG __stdcall GetUIActionMode(void);
__declspec(dllimport) BOOL     __stdcall IsImmInput(HWND hwnd);
__declspec(dllimport) int  __stdcall PCTKEventHook(void);
__declspec(dllimport) int  __stdcall PCTKGetVoiceLog(void);
__declspec(dllimport) int  __stdcall SetKeyBreak(void);
__declspec(dllimport) void __stdcall EncodeFlags(void);
__declspec(dllimport) int  __stdcall dic_regist(void);
__declspec(dllimport) int  __stdcall dic_regist_detail(void);
__declspec(dllimport) int  __stdcall dic_reg_from_file(void);
__declspec(dllimport) int  __stdcall dic_text_out(void);
__declspec(dllimport) int  __stdcall dic_reg_detail_from_file(void);
__declspec(dllimport) int  __stdcall dic_detail_text_out(void);
__declspec(dllimport) BOOL    __stdcall PCTKSTATUS(void);
__declspec(dllimport) DWORD   __stdcall PCTKGETVERSION(void);
__declspec(dllimport) BOOL    __stdcall PCTKGETVSTATUS(void);
__declspec(dllimport) DWORD   __stdcall PCTKGETSTATUS(UINT, LPVOID, LPVOID);
__declspec(dllimport) DWORD   __stdcall PCTKSETSTATUS(UINT, LPVOID, LPVOID);
__declspec(dllimport) BOOL    __stdcall PCTKPREAD(const char *, int, BOOL);
__declspec(dllimport) BOOL    __stdcall PCTKPREADEX(const char *, int, BOOL, int);
__declspec(dllimport) BOOL    __stdcall PCTKPREADW(const wchar_t *, int, BOOL);
__declspec(dllimport) BOOL    __stdcall PCTKPREADEXW(const wchar_t *, int, BOOL, int);
__declspec(dllimport) BOOL    __stdcall PCTKCGUIDE(const char *, DWORD);
__declspec(dllimport) BOOL    __stdcall PCTKCGUIDEEX(const char *, DWORD, int);
__declspec(dllimport) BOOL    __stdcall PCTKCGUIDEW(const wchar_t *, DWORD);
__declspec(dllimport) BOOL    __stdcall PCTKCGUIDEEXW(const wchar_t *, DWORD, int);
__declspec(dllimport) int     __stdcall PCTKCOMMAND(const char *, LPARAM, LPARAM);
__declspec(dllimport) void    __stdcall PCTKVOICEGUIDE(const char *);
__declspec(dllimport) void    __stdcall PCTKVRESET(void);
__declspec(dllimport) void    __stdcall PCTKBEEP(int, int);
__declspec(dllimport) BOOL    __stdcall PCTKLOADUSERDICT(void);
__declspec(dllimport) int     __stdcall PCTKEVENTHOOK(void);
__declspec(dllimport) int     __stdcall PCTKGETVOICELOG(void);

#ifdef __cplusplus
}
#endif
