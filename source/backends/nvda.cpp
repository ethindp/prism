// SPDX-License-Identifier: MPL-2.0

#ifdef _WIN32
#include "backend.h"
#include "backend_registry.h"
#include <cstdlib>
#include <format>
#include <nvda_controller.h>
#include <shared_mutex>
#include <simdutf/simdutf.h>
#include <tchar.h>
#include <windows.h>

extern "C" {
static onSsmlMarkReachedFuncType ssml_mark_reached_callback = nullptr;
static std::shared_mutex ssml_mark_reached_callback_mtx;

_Must_inspect_result_ _Ret_maybenull_ _Post_writable_byte_size_(
    size) void *__RPC_USER MIDL_user_allocate(_In_ size_t size) {
  return malloc(size);
}

void __RPC_USER MIDL_user_free(_Pre_maybenull_ _Post_invalid_ void *p) {
  if (p != nullptr)
    std::free(p);
}

error_status_t __stdcall nvdaController_onSsmlMarkReached(const wchar_t *mark) {
  std::shared_lock sl(ssml_mark_reached_callback_mtx);
  if (ssml_mark_reached_callback == nullptr) {
    return ERROR_CALL_NOT_IMPLEMENTED;
  }
  return ssml_mark_reached_callback(mark);
}

error_status_t __stdcall nvdaController_setOnSsmlMarkReachedCallback(
    onSsmlMarkReachedFuncType callback) {
  {
    std::unique_lock ul(ssml_mark_reached_callback_mtx);
    ssml_mark_reached_callback = callback;
  }
  return ERROR_SUCCESS;
}
}

class NvdaBackend final : public TextToSpeechBackend {
private:
  handle_t controller_handle;

  static bool server_supports_interface(handle_t binding,
                                        RPC_IF_HANDLE ifspec) {
    RPC_IF_ID_VECTOR *vec = nullptr;
    if (RpcMgmtInqIfIds(binding, &vec) != RPC_S_OK)
      return false;
    const auto *iface = static_cast<const RPC_CLIENT_INTERFACE *>(ifspec);
    const UUID &wanted = iface->InterfaceId.SyntaxGUID;
    const auto wanted_mj = iface->InterfaceId.SyntaxVersion.MajorVersion;
    const auto wanted_mn = iface->InterfaceId.SyntaxVersion.MinorVersion;
    bool found = false;
    for (unsigned i = 0; i < vec->Count && !found; ++i) {
      const RPC_IF_ID *id = vec->IfId[i];
      RPC_STATUS st = RPC_S_OK;
      if (UuidEqual(const_cast<UUID *>(&wanted), const_cast<UUID *>(&id->Uuid),
                    &st) != FALSE &&
          id->VersMajor == wanted_mj && id->VersMinor == wanted_mn) {
        found = true;
      }
    }
    RpcIfIdVectorFree(&vec);
    return found;
  }

public:
  ~NvdaBackend() override {
    if (controller_handle != nullptr) {
      RpcBindingFree(&controller_handle);
      controller_handle = nullptr;
    }
  }

  [[nodiscard]] std::string_view get_name() const override { return "NVDA"; }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    features |=
        SUPPORTS_SPEAK | SUPPORTS_BRAILLE | SUPPORTS_OUTPUT | SUPPORTS_STOP;
    DWORD session_id = 0;
    if (ProcessIdToSessionId(GetCurrentProcessId(), &session_id) == 0)
      return features;
    const HANDLE desktop = GetThreadDesktop(GetCurrentThreadId());
    if (desktop == nullptr)
      return features;
    std::wstring desktop_name(32, _T('\0'));
    DWORD bytes_written = 0;
    if (GetUserObjectInformation(
            desktop, UOI_NAME, desktop_name.data(),
            static_cast<DWORD>(desktop_name.size() * sizeof(wchar_t)),
            &bytes_written) == 0)
      return features;
    desktop_name.resize((bytes_written / sizeof(wchar_t)) - 1);
    const auto endpoint =
        std::format(_T("NvdaCtlr.{}.{}"), session_id, desktop_name);
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
    if (server_supports_interface(handle,
                                  nvdaController_NvdaController_v1_0_c_ifspec))
      features |= IS_SUPPORTED_AT_RUNTIME;
    if (server_supports_interface(handle,
                                  nvdaController_NvdaController3_v1_0_c_ifspec))
      features |= SUPPORTS_IS_SPEAKING;
    RpcBindingFree(&handle);
    return features;
  }

  BackendResult<> initialize() override {
    if (controller_handle != nullptr)
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
    RpcStringFree(&string_binding);
    if (status != RPC_S_OK) {
      RpcStringFree(&string_binding);
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    if (server_supports_interface(
            controller_handle, nvdaController_NvdaController_v1_0_c_ifspec)) {
      if (nvdaController_testIfRunning(controller_handle) != ERROR_SUCCESS) {
        return std::unexpected(BackendError::BackendNotAvailable);
      }
    }
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (controller_handle == nullptr ||
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
    if (controller_handle == nullptr ||
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
    if (controller_handle == nullptr ||
        nvdaController_testIfRunning(controller_handle) != ERROR_SUCCESS)
      return std::unexpected(BackendError::BackendNotAvailable);
    if (nvdaController_cancelSpeech(controller_handle) != ERROR_SUCCESS)
      return std::unexpected(BackendError::InternalBackendError);
    return {};
  }

  BackendResult<bool> is_speaking() override {
    if (controller_handle == nullptr ||
        nvdaController_testIfRunning(controller_handle) != ERROR_SUCCESS)
      return std::unexpected(BackendError::BackendNotAvailable);
    if (!server_supports_interface(
            controller_handle, nvdaController_NvdaController3_v1_0_c_ifspec))
      return std::unexpected(BackendError::NotImplemented);
    BOOLEAN speaking = FALSE;
    if (nvdaController_isSpeaking(controller_handle, &speaking) !=
        ERROR_SUCCESS)
      return std::unexpected(BackendError::InternalBackendError);
    return static_cast<bool>(speaking);
  }
};

REGISTER_BACKEND_WITH_ID(NvdaBackend, Backends::NVDA, "NVDA", 103);
#endif
