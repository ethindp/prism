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
static thread_local onSsmlMarkReachedFuncType ssml_mark_reached_callback =
    nullptr;
void *__RPC_USER midl_user_allocate(size_t size) { return malloc(size); }

void __RPC_USER midl_user_free(void *p) {
  if (p != nullptr)
    std::free(p);
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
private:
  handle_t controller_handle;
  handle_t controller2_handle;

public:
  ~NvdaBackend() override {
    if (controller_handle != nullptr) {
      RpcBindingFree(&controller_handle);
      controller_handle = nullptr;
    }
    if (controller2_handle != nullptr) {
      RpcBindingFree(&controller2_handle);
      controller2_handle = nullptr;
    }
  }

  [[nodiscard]] std::string_view get_name() const override { return "NVDA"; }

[[nodiscard]] std::bitset<64> get_features() const override {
  using namespace BackendFeature;
  std::bitset<64> features;
  features |= SUPPORTS_SPEAK | SUPPORTS_BRAILLE | SUPPORTS_OUTPUT | SUPPORTS_STOP;
  DWORD session_id = 0;
  if (ProcessIdToSessionId(GetCurrentProcessId(), &session_id) == 0)
    return features;
  const HANDLE desktop = GetThreadDesktop(GetCurrentThreadId());
  if (desktop == nullptr)
    return features;
  std::wstring desktop_name(32, _T('\0'));
  DWORD bytes_written = 0;
  if (GetUserObjectInformation(desktop, UOI_NAME, desktop_name.data(), static_cast<DWORD>(desktop_name.size() * sizeof(wchar_t)), &bytes_written) == 0)
    return features;
  desktop_name.resize((bytes_written / sizeof(wchar_t)) - 1);
  const auto endpoint = std::format(_T("NvdaCtlr.{}.{}"), session_id, desktop_name);
  RPC_WSTR string_binding = nullptr;
  if (RpcStringBindingCompose(nullptr, RPC_WSTR(_T("ncalrpc")), nullptr,
                                RPC_WSTR(endpoint.c_str()), nullptr,
                                &string_binding) != RPC_S_OK)
    return features;
  handle_t handle = nullptr;
  if (RpcBindingFromStringBinding(string_binding, &handle) != RPC_S_OK) {
    RpcStringFree(&string_binding);
    return features;
  }
  RpcStringFree(&string_binding);
  if (nvdaController_testIfRunning(handle) == ERROR_SUCCESS)
    features |= IS_SUPPORTED_AT_RUNTIME;
  RpcBindingFree(&handle);
  return features;
}

  BackendResult<> initialize() override {
    if (controller_handle != nullptr || controller2_handle != nullptr)
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
    status = RpcBindingFromStringBinding(string_binding, &controller_handle);
    if (status != RPC_S_OK) {
      RpcStringFree(&string_binding);
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    status = RpcBindingFromStringBinding(string_binding, &controller2_handle);
    RpcStringFree(&string_binding);
    if (status != RPC_S_OK) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    if (nvdaController_testIfRunning(controller_handle) != ERROR_SUCCESS)
      return std::unexpected(BackendError::BackendNotAvailable);
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (controller_handle == nullptr || controller2_handle == nullptr ||
        nvdaController_testIfRunning(controller_handle) != ERROR_SUCCESS)
      return std::unexpected(BackendError::BackendNotAvailable);
    if (interrupt) {
      if (nvdaController_cancelSpeech(controller_handle) != ERROR_SUCCESS)
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
    if (nvdaController_speakText(controller_handle, wstr.c_str()) !=
        ERROR_SUCCESS)
      return std::unexpected(BackendError::InternalBackendError);
    return {};
  }

  BackendResult<> braille(std::string_view text) override {
    if (controller_handle == nullptr || controller2_handle == nullptr ||
        nvdaController_testIfRunning(controller_handle) != ERROR_SUCCESS)
      return std::unexpected(BackendError::BackendNotAvailable);
    const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
    std::wstring wstr;
    wstr.resize(len);
    if (const auto res = simdutf::convert_utf8_to_utf16le(
            text.data(), text.size(),
            reinterpret_cast<char16_t *>(wstr.data()));
        res == 0)
      return std::unexpected(BackendError::InvalidUtf8);
    if (nvdaController_brailleMessage(controller_handle, wstr.c_str()) !=
        ERROR_SUCCESS)
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
    if (controller_handle == nullptr || controller2_handle == nullptr ||
        nvdaController_testIfRunning(controller_handle) != ERROR_SUCCESS)
      return std::unexpected(BackendError::BackendNotAvailable);
    if (nvdaController_cancelSpeech(controller_handle) != ERROR_SUCCESS)
      return std::unexpected(BackendError::InternalBackendError);
    return {};
  }
};

REGISTER_BACKEND_WITH_ID(NvdaBackend, Backends::NVDA, "NVDA", 103);
#endif
