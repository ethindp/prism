// SPDX-License-Identifier: MPL-2.0

#ifdef _WIN32
#include <array>
#include <cstring>
#include <cwchar>
#include <delayimp.h>
#include <filesystem>
#include <tchar.h>
#include <utility>
#include <windows.h>

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
#endif

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
    });
#else
    std::array<StubEntry, 0>
        stubs = {};
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