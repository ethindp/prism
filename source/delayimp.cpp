// SPDX-License-Identifier: MPL-2.0

#ifdef _WIN32
#include <windows.h>
#include <array>
#include <cstring>
#include <cwchar>
#include <delayimp.h>
#include <filesystem>
#include <nvdaController.h>
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

static error_status_t __stdcall stub_nvdaController_setOnSsmlMarkReachedCallback(
    [[maybe_unused]] onSsmlMarkReachedFuncType callback) {
  return E_NOTIMPL;
}

static error_status_t __stdcall stub_nvdaController_testIfRunning() {
  return E_NOTIMPL;
}

static error_status_t __stdcall stub_nvdaController_speakText(
    [[maybe_unused]] const wchar_t *text) {
  return E_NOTIMPL;
}

static error_status_t __stdcall stub_nvdaController_cancelSpeech() {
  return E_NOTIMPL;
}

static error_status_t __stdcall stub_nvdaController_brailleMessage(
    [[maybe_unused]] const wchar_t *message) {
  return E_NOTIMPL;
}

static error_status_t __stdcall stub_nvdaController_getProcessId(
    unsigned long *pid) {
  if (pid != nullptr)
    *pid = 0;
  return E_NOTIMPL;
}

static error_status_t __stdcall stub_nvdaController_speakSsml(
    [[maybe_unused]] const wchar_t *ssml,
    [[maybe_unused]] const SYMBOL_LEVEL symbolLevel,
    [[maybe_unused]] const SPEECH_PRIORITY priority,
    [[maybe_unused]] const boolean asynchronous) {
  return E_NOTIMPL;
}

static error_status_t __stdcall stub_nvdaController_onSsmlMarkReached(
    [[maybe_unused]] const wchar_t *mark) {
  return E_NOTIMPL;
}

static BOOL __stdcall stub_SA_SayW([[maybe_unused]] const wchar_t *text) {
  return FALSE;
}

static BOOL __stdcall stub_SA_BrlShowTextW(
    [[maybe_unused]] const wchar_t *msg) {
  return FALSE;
}

static BOOL __stdcall stub_SA_StopAudio() { return FALSE; }

static BOOL __stdcall stub_SA_IsRunning() { return FALSE; }

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

static const auto stubs = std::to_array<StubEntry>(
    {{.dll = "nvdaControllerClient.dll",
      .func = "nvdaController_setOnSsmlMarkReachedCallback",
      .stub = stub_cast(stub_nvdaController_setOnSsmlMarkReachedCallback)},
     {.dll = "nvdaControllerClient.dll",
      .func = "nvdaController_testIfRunning",
      .stub = stub_cast(stub_nvdaController_testIfRunning)},
     {.dll = "nvdaControllerClient.dll",
      .func = "nvdaController_speakText",
      .stub = stub_cast(stub_nvdaController_speakText)},
     {.dll = "nvdaControllerClient.dll",
      .func = "nvdaController_cancelSpeech",
      .stub = stub_cast(stub_nvdaController_cancelSpeech)},
     {.dll = "nvdaControllerClient.dll",
      .func = "nvdaController_brailleMessage",
      .stub = stub_cast(stub_nvdaController_brailleMessage)},
     {.dll = "nvdaControllerClient.dll",
      .func = "nvdaController_getProcessId",
      .stub = stub_cast(stub_nvdaController_getProcessId)},
     {.dll = "nvdaControllerClient.dll",
      .func = "nvdaController_speakSsml",
      .stub = stub_cast(stub_nvdaController_speakSsml)},
     {.dll = "nvdaControllerClient.dll",
      .func = "nvdaController_onSsmlMarkReached",
      .stub = stub_cast(stub_nvdaController_onSsmlMarkReached)},
     {.dll = "SAAPI64.dll", .func = "SA_SayW", .stub = stub_cast(stub_SA_SayW)},
     {.dll = "SAAPI64.dll",
      .func = "SA_BrlShowTextW",
      .stub = stub_cast(stub_SA_BrlShowTextW)},
     {.dll = "SAAPI64.dll",
      .func = "SA_StopAudio",
      .stub = stub_cast(stub_SA_StopAudio)},
     {.dll = "SAAPI64.dll",
      .func = "SA_IsRunning",
      .stub = stub_cast(stub_SA_IsRunning)},
     {.dll = "ZDSRAPI_x64.dll",
      .func = "InitTTS",
      .stub = stub_cast(stub_zdsr_InitTTS)},
     {.dll = "ZDSRAPI_x64.dll",
      .func = "Speak",
      .stub = stub_cast(stub_zdsr_Speak)},
     {.dll = "ZDSRAPI_x64.dll",
      .func = "GetSpeakState",
      .stub = stub_cast(stub_zdsr_GetSpeakState)},
     {.dll = "ZDSRAPI_x64.dll",
      .func = "StopSpeak",
      .stub = stub_cast(stub_zdsr_StopSpeak)}});

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
                RegQueryValueEx(zdsr_key, _T("path"), nullptr, nullptr,
                                reinterpret_cast<LPBYTE>(path.data()), &size);
            res2 == ERROR_SUCCESS) {
          path.resize(std::wcslen(path.c_str()));
          if (!path.empty() && path.back() != _T('\\')) {
            path += _T('\\');
          }
          path += _T("ZDSRAPI_x64.dll");
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