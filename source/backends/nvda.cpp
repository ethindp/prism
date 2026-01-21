// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#ifdef _WIN32
#include "nvda_controller.h"
#include <cstdlib>
#include <format>
#include <tchar.h>
#include <windows.h>

extern "C" {
static onSsmlMarkReachedFuncType ssml_mark_reached_callback = nullptr;
void *__RPC_USER midl_user_allocate(_In_ size_t size) { return malloc(size); }

void __RPC_USER midl_user_free(void *p) {
  if (p != nullptr)
    free(p);
}

error_status_t __stdcall nvdaController_onSsmlMarkReached(const wchar_t *mark) {
  if (ssml_mark_reached_callback == nullptr) {
    return ERROR_CALL_NOT_IMPLEMENTED;
  }
  return ssml_mark_reached_callback(mark);
}

error_status_t __stdcall nvdaController_setOnSsmlMarkReachedCallback(
    onSsmlMarkReachedFuncType callback) {
  ssml_mark_reached_callback = callback;
  return ERROR_SUCCESS;
}
}

class NvdaBackend final : public TextToSpeechBackend {
public:
  ~NvdaBackend() override {
    if (nvdaControllerBindingHandle != nullptr) {
      RpcBindingFree(&nvdaControllerBindingHandle);
      nvdaControllerBindingHandle = nullptr;
    }
    if (nvdaController2BindingHandle != nullptr) {
      RpcBindingFree(&nvdaController2BindingHandle);
      nvdaController2BindingHandle = nullptr;
    }
  }

  [[nodiscard]] std::string_view get_name() const override { return "NVDA"; }

  BackendResult<> initialize() override {
    if (nvdaControllerBindingHandle != nullptr ||
        nvdaController2BindingHandle != nullptr)
      return std::unexpected(BackendError::AlreadyInitialized);
    DWORD sid = 0;
    if (const auto res = ProcessIdToSessionId(GetCurrentProcessId(), &sid);
        res == 0)
      return std::unexpected(BackendError::BackendNotAvailable);
    const HANDLE desktop_handle = GetThreadDesktop(GetCurrentThreadId());
    if (desktop_handle == nullptr)
      return std::unexpected(BackendError::BackendNotAvailable);
    std::wstring desktop_name;
    desktop_name.resize(32);
    if (const auto res = GetUserObjectInformation(
            desktop_handle, UOI_NAME, desktop_name.data(),
            static_cast<DWORD>(desktop_name.size()) * sizeof(wchar_t), nullptr);
        res == 0)
      return std::unexpected(BackendError::BackendNotAvailable);
    const std::wstring desktop_ns = std::format(_T("{}.{}"), sid, desktop_name);
    RPC_STATUS status;
    const auto endpoint = std::format(_T("NvdaCtlr.{}"), desktop_ns);
    RPC_WSTR string_binding = nullptr;
    status = RpcStringBindingCompose(nullptr, RPC_WSTR(_T("ncalrpc")), nullptr,
                                     RPC_WSTR(endpoint.c_str()), nullptr,
                                     &string_binding);
    if (status != RPC_S_OK)
      return std::unexpected(BackendError::BackendNotAvailable);
    status = RpcBindingFromStringBinding(string_binding,
                                         &nvdaControllerBindingHandle);
    if (status != RPC_S_OK) {
      RpcStringFree(&string_binding);
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    status = RpcBindingFromStringBinding(string_binding,
                                         &nvdaController2BindingHandle);
    RpcStringFree(&string_binding);
    if (status != RPC_S_OK) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    if (nvdaController_testIfRunning() != ERROR_SUCCESS)
      return std::unexpected(BackendError::BackendNotAvailable);
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (nvdaControllerBindingHandle == nullptr ||
        nvdaController2BindingHandle == nullptr ||
        nvdaController_testIfRunning() != ERROR_SUCCESS)
      return std::unexpected(BackendError::BackendNotAvailable);
    if (interrupt) {
      if (nvdaController_cancelSpeech() != ERROR_SUCCESS)
        return std::unexpected(BackendError::InternalBackendError);
    }
    const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
    std::wstring wstr;
    wstr.resize(len);
    if (const auto res = simdutf::convert_utf8_to_utf16le(
            text.data(), text.size(),
            reinterpret_cast<char16_t *>(wstr.data()));
        res == 0)
      return std::unexpected(BackendError::InvalidUtf8);
    if (nvdaController_speakText(wstr.c_str()) != ERROR_SUCCESS)
      return std::unexpected(BackendError::InternalBackendError);
    return {};
  }

  BackendResult<> braille(std::string_view text) override {
    if (nvdaControllerBindingHandle == nullptr ||
        nvdaController2BindingHandle == nullptr ||
        nvdaController_testIfRunning() != ERROR_SUCCESS)
      return std::unexpected(BackendError::BackendNotAvailable);
    const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
    std::wstring wstr;
    wstr.resize(len);
    if (const auto res = simdutf::convert_utf8_to_utf16le(
            text.data(), text.size(),
            reinterpret_cast<char16_t *>(wstr.data()));
        res == 0)
      return std::unexpected(BackendError::InvalidUtf8);
    if (nvdaController_brailleMessage(wstr.c_str()) != ERROR_SUCCESS)
      return std::unexpected(BackendError::InternalBackendError);
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    if (const auto res = speak(text, interrupt); !res)
      return res;
    if (const auto res = braille(text); !res)
      return res;
    return {};
  }

  BackendResult<> stop() override {
    if (nvdaControllerBindingHandle == nullptr ||
        nvdaController2BindingHandle == nullptr ||
        nvdaController_testIfRunning() != ERROR_SUCCESS)
      return std::unexpected(BackendError::BackendNotAvailable);
    if (nvdaController_cancelSpeech() != ERROR_SUCCESS)
      return std::unexpected(BackendError::InternalBackendError);
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(NvdaBackend, Backends::NVDA, "NVDA", 100);
#endif
