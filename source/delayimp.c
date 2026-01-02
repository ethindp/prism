#ifdef _WIN32
#include <windows.h>
#include <delayimp.h>
#include <nvdaController.h>
#include <string.h>

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
    {NULL, NULL, NULL}};

static int dummy_count = 0;

static FARPROC WINAPI DelayLoadFailureHook(unsigned dliNotify,
                                           PDelayLoadInfo pdli) {
  switch (dliNotify) {
  case dliFailLoadLib: {
    if (dummy_count < 512) {
      HMODULE dummy = (HMODULE)(intptr_t)(0xDEAD0000 + dummy_count);
      dummy_count++;
      return (FARPROC)dummy;
    }
    return (FARPROC)(HMODULE)1;
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
#endif