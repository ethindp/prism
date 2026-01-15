// SPDX-License-Identifier: MPL-2.0

#ifdef _WIN32
#include <windows.h>
#include <cwchar>
#include <delayimp.h>
#include <filesystem>
#include <nvdaController.h>
#include <string.h>
#include <tchar.h>

extern "C" {
typedef struct {
  const char *dll;
  const char *func;
  FARPROC stub;
} StubEntry;

static error_status_t __stdcall stub_nvdaController_setOnSsmlMarkReachedCallback(
    onSsmlMarkReachedFuncType callback) {
  return E_NOTIMPL;
}

static error_status_t __stdcall stub_nvdaController_testIfRunning() {
  return E_NOTIMPL;
}

static error_status_t __stdcall stub_nvdaController_speakText(
    const wchar_t *text) {
  return E_NOTIMPL;
}

static error_status_t __stdcall stub_nvdaController_cancelSpeech() {
  return E_NOTIMPL;
}

static error_status_t __stdcall stub_nvdaController_brailleMessage(
    const wchar_t *message) {
  return E_NOTIMPL;
}

static error_status_t __stdcall stub_nvdaController_getProcessId(
    unsigned long *pid) {
  if (pid)
    *pid = 0;
  return E_NOTIMPL;
}

static error_status_t __stdcall stub_nvdaController_speakSsml(
    const wchar_t *ssml, const SYMBOL_LEVEL symbolLevel,
    const SPEECH_PRIORITY priority, const boolean asynchronous) {
  return E_NOTIMPL;
}

static error_status_t __stdcall stub_nvdaController_onSsmlMarkReached(
    const wchar_t *mark) {
  return E_NOTIMPL;
}

static BOOL __stdcall stub_SA_SayW(const wchar_t *text) { return FALSE; }

static BOOL __stdcall stub_SA_BrlShowTextW(const wchar_t *msg) { return FALSE; }

static BOOL __stdcall stub_SA_StopAudio(void) { return FALSE; }

static BOOL __stdcall stub_SA_IsRunning(void) { return FALSE; }

static int WINAPI stub_zdsr_InitTTS(int type, const WCHAR *channelName,
                                    BOOL bKeyDownInterrupt) {
  return 2;
}

static int WINAPI stub_zdsr_Speak(const WCHAR *text, BOOL bInterrupt) {
  return 2;
}

static int WINAPI stub_zdsr_GetSpeakState() { return 2; }

static void WINAPI stub_zdsr_StopSpeak() { return; }

static const StubEntry stubs[] = {
    {"nvdaControllerClient.dll", "nvdaController_setOnSsmlMarkReachedCallback",
     (FARPROC)stub_nvdaController_setOnSsmlMarkReachedCallback},
    {"nvdaControllerClient.dll", "nvdaController_testIfRunning",
     (FARPROC)stub_nvdaController_testIfRunning},
    {"nvdaControllerClient.dll", "nvdaController_speakText",
     (FARPROC)stub_nvdaController_speakText},
    {"nvdaControllerClient.dll", "nvdaController_cancelSpeech",
     (FARPROC)stub_nvdaController_cancelSpeech},
    {"nvdaControllerClient.dll", "nvdaController_brailleMessage",
     (FARPROC)stub_nvdaController_brailleMessage},
    {"nvdaControllerClient.dll", "nvdaController_getProcessId",
     (FARPROC)stub_nvdaController_getProcessId},
    {"nvdaControllerClient.dll", "nvdaController_speakSsml",
     (FARPROC)stub_nvdaController_speakSsml},
    {"nvdaControllerClient.dll", "nvdaController_onSsmlMarkReached",
     (FARPROC)stub_nvdaController_onSsmlMarkReached},
    {"SAAPI64.dll", "SA_SayW", (FARPROC)stub_SA_SayW},
    {"SAAPI64.dll", "SA_BrlShowTextW", (FARPROC)stub_SA_BrlShowTextW},
    {"SAAPI64.dll", "SA_StopAudio", (FARPROC)stub_SA_StopAudio},
    {"SAAPI64.dll", "SA_IsRunning", (FARPROC)stub_SA_IsRunning},
    {"ZDSRAPI_x64.dll", "InitTTS", (FARPROC)stub_zdsr_InitTTS},
    {"ZDSRAPI_x64.dll", "Speak", (FARPROC)stub_zdsr_Speak},
    {"ZDSRAPI_x64.dll", "GetSpeakState", (FARPROC)stub_zdsr_GetSpeakState},
    {"ZDSRAPI_x64.dll", "StopSpeak", (FARPROC)stub_zdsr_StopSpeak},
    {NULL, NULL, NULL}};

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
        if (const auto h = LoadLibrary(dll_path.c_str()); h != NULL) {
          return reinterpret_cast<FARPROC>(h);
        }
      }
    }
    if (_stricmp(pdli->szDll, "ZDSRAPI_x64.dll") == 0) {
      HKEY zdsr_key;
      if (const auto res = RegOpenKeyEx(
              HKEY_LOCAL_MACHINE, _T("SOFTWARE\\WOW6432Node\\zhiduo\\zdsr"), 0,
              KEY_QUERY_VALUE | KEY_READ, &zdsr_key);
          res == ERROR_SUCCESS) {
        std::wstring path;
        path.resize(MAX_PATH);
        DWORD size = MAX_PATH * sizeof(wchar_t);
        if (const auto res2 =
                RegQueryValueEx(zdsr_key, _T("path"), 0, nullptr,
                                reinterpret_cast<LPBYTE>(path.data()), &size);
            res2 == ERROR_SUCCESS) {
          path.resize(std::wcslen(path.c_str()));
          if (!path.empty() && path.back() != _T('\\')) {
            path += _T('\\');
          }
          path += _T("ZDSRAPI_x64.dll");
          const auto h = LoadLibrary(path.c_str());
          RegCloseKey(zdsr_key);
          if (h != NULL) {
            return reinterpret_cast<FARPROC>(h);
          }
        } else {
          RegCloseKey(zdsr_key);
        }
      }
    }
    if (dummy_count < 512) {
      HMODULE dummy = (HMODULE)(intptr_t)(0xDEAD0000 + dummy_count);
      dummy_count++;
      return reinterpret_cast<FARPROC>(dummy);
    }
    return reinterpret_cast<FARPROC>(reinterpret_cast<HMODULE>(1));
  } break;
  case dliFailGetProc: {
    for (const StubEntry *e = stubs; e->dll; e++) {
      if (_stricmp(pdli->szDll, e->dll) == 0 &&
          strcmp(pdli->dlp.szProcName, e->func) == 0) {
        return e->stub;
      }
    }
    return NULL;
  } break;
  }
  return NULL;
}

const PfnDliHook __pfnDliFailureHook2 = DelayLoadFailureHook;
}
#endif